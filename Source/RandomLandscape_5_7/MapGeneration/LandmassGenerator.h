// LandmassGenerator.h
// Generates the initial land/ocean mask texture for procedural maps
// Version: 01.26.2026.21.45

#pragma once

#include "CoreMinimal.h"
#include "LandmassGenerator.generated.h"

class UTexture2D;

/**
 * Configuration settings for landmass generation.
 * Controls the random seed, output resolution, and land/ocean ratio.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FLandmassSettings
{
	GENERATED_BODY()

	/** 
	 * Random seed for landmass shape generation.
	 * Same seed produces identical landmass shapes.
	 * Seed==0 triggers auto-seeding inside Initialize(); GetSeed() returns the actual seed used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass")
	int32 Seed = 0;

	/** 
	 * Output texture resolution (width = height).
	 * Higher values give more detailed coastlines but increase generation time.
	 * Typical values: 256 (fast), 512 (balanced), 1024 (detailed).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 TextureResolution = 512;

	/** 
	 * Target percentage of pixels that should be classified as land.
	 * The algorithm guarantees this exact coverage by computing an adaptive threshold.
	 * Clamped to 5-75% to ensure meaningful land/ocean distribution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass", meta = (ClampMin = "5.0", ClampMax = "75.0"))
	float LandCoveragePercent = 50.0f;
};

/**
 * Generates land/ocean mask for procedural map generation.
 * 
 * This class creates a binary land mask using layered FBM (Fractal Brownian Motion) noise
 * to produce organic, natural-looking continent shapes with irregular coastlines.
 * 
 * Generation Pipeline:
 *   1. Initialize() - Store settings and seed the random stream
 *   2. Generate()   - Run the full generation pipeline
 *      a. GenerateLandMask()      - Compute per-pixel continent values and threshold to binary mask
 *      b. GeneratePreviewTexture() - Convert mask to black/white texture for visualization
 * 
 * The algorithm uses distance-from-center combined with multi-octave noise to create
 * continent shapes that are roughly centered but have organic, irregular edges.
 * Edge falloff ensures land doesn't touch texture borders.
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API ULandmassGenerator : public UObject
{
	GENERATED_BODY()

public:
	ULandmassGenerator();

	/**
	 * Initialize the generator with the given settings.
	 * Must be called before Generate().
	 * 
	 * @param InSettings - Configuration including seed, resolution, and land coverage percentage
	 */
	void Initialize(const FLandmassSettings& InSettings);

	/**
	 * Execute the full landmass generation pipeline.
	 * Generates the land mask and creates a preview texture.
	 * 
	 * @return true if generation succeeded (LandMask has data), false otherwise
	 */
	bool Generate();

	/**
	 * Get the binary land mask array.
	 * Each element corresponds to a pixel: 1 = land, 0 = ocean.
	 * Array is indexed as [Y * Width + X].
	 */
	const TArray<uint8>& GetLandMask() const { return LandMask; }

	/**
	 * Get the coordinates of all pixels classified as land.
	 * Useful for algorithms that need to iterate only over land areas
	 * (e.g., biome seed point placement, land pixel counting).
	 */
	const TArray<FIntPoint>& GetLandPixels() const { return LandPixels; }

	/**
	 * Get the generated black/white preview texture.
	 * White pixels = land, Black pixels = ocean.
	 * Returns nullptr if Generate() hasn't been called or failed.
	 */
	UTexture2D* GetPreviewTexture() const { return PreviewTexture; }

	/** Get the texture resolution (width = height) */
	int32 GetTextureResolution() const { return Settings.TextureResolution; }

	/** Get the seed used for noise generation */
	int32 GetSeed() const { return Settings.Seed; }

protected:
	/**
	 * Generate the binary land mask from noise-based continent values.
	 * 
	 * Algorithm:
	 *   1. For each pixel, compute GetContinentMask() value (0.0 to 1.0)
	 *   2. Sort all values to find the threshold that achieves target land coverage
	 *   3. Apply threshold: pixels >= threshold become land
	 * 
	 * This approach guarantees the exact land coverage percentage requested,
	 * regardless of noise characteristics.
	 */
	void GenerateLandMask();

	/**
	 * Generate a black/white UTexture2D from the LandMask array.
	 * Creates a transient texture suitable for preview/debug visualization.
	 * Format: BGRA8, no mipmaps, nearest filtering for crisp pixel edges.
	 */
	void GeneratePreviewTexture();

	/**
	 * 2D value noise function with seed-based offset.
	 * Uses bilinear interpolation between hashed grid corner values.
	 * 
	 * @param X, Y - Input coordinates (can be any range, will be hashed)
	 * @return Noise value in approximately -1.0 to 1.0 range
	 */
	float Noise2D(float X, float Y) const;

	/**
	 * Fractal Brownian Motion - layered noise for natural-looking patterns.
	 * Combines multiple octaves of Noise2D at increasing frequencies
	 * and decreasing amplitudes for rich, multi-scale detail.
	 * 
	 * @param X, Y        - Input coordinates (normalized 0-1 recommended)
	 * @param Octaves     - Number of noise layers (more = finer detail, slower)
	 * @param Persistence - Amplitude multiplier per octave (0.5 typical; higher = rougher)
	 * @return Normalized noise value in -1.0 to 1.0 range
	 */
	float FBM(float X, float Y, int32 Octaves, float Persistence) const;

	/**
	 * Compute the "continent-ness" value for a normalized texture coordinate.
	 * Higher values are more likely to be land.
	 * 
	 * Components:
	 *   - Distance from center: land concentrated toward middle
	 *   - Coast noise: irregular coastline detail
	 *   - Shape noise: large-scale continent shape variation
	 *   - Detail noise: fine bumps and indentations
	 *   - Edge falloff: prevents land from touching texture borders
	 * 
	 * @param NormX, NormY - Normalized coordinates (0.0 to 1.0)
	 * @return Continent mask value (0.0 to 1.0, higher = more land-like)
	 */
	float GetContinentMask(float NormX, float NormY) const;

protected:
	/** Generation settings */
	FLandmassSettings Settings;

	/** Random stream for deterministic generation */
	FRandomStream RandomStream;

	// === Seed-derived noise offsets (generated in Initialize) ===
	// Each layer samples noise at a different offset to decorrelate patterns.
	// These replace hard-coded offsets (+50, +100, +200) for better variety per seed.
	
	/** Offset for shape noise layer (low-frequency continent shape) */
	FVector2D ShapeNoiseOffset;
	
	/** Offset for detail noise layer (high-frequency coastal details) */
	FVector2D DetailNoiseOffset;
	
	/** Offset for edge falloff noise layer (irregular border padding) */
	FVector2D EdgeNoiseOffset;

	/** Land mask - 1 = land, 0 = ocean (uint8 for thread-safe parallel writes) */
	TArray<uint8> LandMask;

	/** List of all land pixel coordinates */
	TArray<FIntPoint> LandPixels;

	/** Black/white preview texture */
	UPROPERTY()
	TObjectPtr<UTexture2D> PreviewTexture;
};
