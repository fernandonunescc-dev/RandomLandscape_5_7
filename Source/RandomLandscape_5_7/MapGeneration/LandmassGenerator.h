// LandmassGenerator.h
// Generates the initial land/ocean mask texture for procedural maps
// Version: 01.26.2026.23.36

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include "LandmassGenerator.generated.h"

class UTexture2D;

/**
 * Configuration settings for landmass generation.
 * Controls the random seed, output resolution, map type, size, and land/ocean ratio.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FLandmassSettings
{
	GENERATED_BODY()

	/**
	 * Target land area in square kilometres.
	 * The algorithm generates exactly this much land, then wraps ocean around it.
	 * The actual world dimensions are derived from the land area and ocean padding.
	 * Typical range: 0.05 (tiny island) to 16.0 (continent-sized).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass", meta = (ClampMin = "0.01", ClampMax = "64.0", UIMin = "0.01", UIMax = "16.0"))
	float TargetLandAreaSqKm = 0.25f;

	/**
	 * Maximum height of the terrain mesh in meters.
	 * Controls how tall mountains and the highest biome features can be.
	 * This is the absolute ceiling - individual biome HeightScale is relative to this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass", meta = (ClampMin = "10.0", ClampMax = "2000.0", UIMin = "10.0", UIMax = "2000.0"))
	float MaxMapHeight = 100.0f;

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
	 * Internal land coverage fraction used during generation.
	 * Computed automatically from TargetLandAreaSqKm and OceanPaddingMeters.
	 * NOT user-facing — use TargetLandAreaSqKm instead.
	 */
	float ComputedLandCoveragePercent = 30.0f;

	// === Domain Warp Settings ===
	// Domain warping offsets sampling coordinates before noise evaluation,
	// creating large-scale bends, peninsulas, and bays for more realistic coastlines.

	/** Enable domain warping for more organic coastline shapes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass|Domain Warp")
	bool bEnableDomainWarp = false;

	/** 
	 * Frequency of the warp noise field (in normalized coordinate space).
	 * Lower values = larger, smoother bends; higher values = more chaotic warping.
	 * Typical range: 0.5 to 3.0
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass|Domain Warp", meta = (ClampMin = "0.1", ClampMax = "10.0", EditCondition = "bEnableDomainWarp"))
	float WarpFrequency = 1.5f;

	/** 
	 * Maximum displacement in normalized coordinate units.
	 * Controls how far coordinates can be pushed by the warp field.
	 * Typical range: 0.02 to 0.15 (2% to 15% of texture size)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass|Domain Warp", meta = (ClampMin = "0.0", ClampMax = "0.5", EditCondition = "bEnableDomainWarp"))
	float WarpAmplitude = 0.08f;

	/** 
	 * Number of noise octaves for warp field.
	 * More octaves = finer detail in the warping pattern.
	 * Typical range: 2 to 4
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass|Domain Warp", meta = (ClampMin = "1", ClampMax = "8", EditCondition = "bEnableDomainWarp"))
	int32 WarpOctaves = 2;

	/** 
	 * Persistence for warp noise octaves.
	 * Controls how much each successive octave contributes.
	 * Typical range: 0.5 to 0.7
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass|Domain Warp", meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bEnableDomainWarp"))
	float WarpPersistence = 0.5f;

	// === Post-Processing Settings ===

	/**
	 * If true, only the largest connected land component is kept.
	 * All other land components (islands) are converted to ocean.
	 * Disable to allow multiple separate landmasses (archipelago-like results).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass|Post-Processing")
	bool bKeepOnlyLargestLandmass = false;

	/** 
	 * If true, fills enclosed ocean areas (holes/lakes) inside the landmass.
	 * Works by flood-filling ocean from borders and converting unreachable ocean to land.
	 * Applied after bKeepOnlyLargestLandmass if both are enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass|Post-Processing")
	bool bFillEnclosedHoles = false;

	// === Edge Margin Settings ===

	/**
	 * Minimum width of visible ocean around ALL land, in metres.
	 * After land is generated freely, the world size is expanded so that
	 * every side has at least this much ocean.  Land is never cut — if it
	 * grows larger or more spread-out than expected the world just gets
	 * bigger to accommodate.  Default 200 m.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmass", meta = (ClampMin = "100.0", ClampMax = "2000.0", Tooltip = "Minimum ocean border around all land in metres.  The world expands to guarantee this. Default 200 m."))
	float OceanPaddingMeters = 200.0f;

	/**
	 * Computed world-space size in centimeters.
	 * Derived from TargetLandAreaSqKm + OceanPaddingMeters during Initialize().
	 * Use GetWorldSizeCm() to access this value.
	 */
	float ComputedWorldSizeCm = 100000.0f;

	/**
	 * Get the world-space size in centimeters.
	 * After Initialize() this is derived from the target land area and ocean padding.
	 */
	float GetWorldSizeCm() const
	{
		return ComputedWorldSizeCm;
	}
};

/**
 * Generates land/ocean mask for procedural map generation.
 * 
 * This class creates a binary land mask using layered FBM (Fractal Brownian Motion) noise
 * to produce organic, natural-looking island shapes with irregular coastlines.
 * 
 * Generation Pipeline:
 *   1. Initialize() - Store settings and seed the random stream
 *   2. Generate()   - Run the full generation pipeline
 *      a. GenerateLandMask()      - Compute per-pixel island mask values and threshold to binary mask
 *      b. GeneratePreviewTexture() - Convert mask to black/white texture for visualization
 * 
 * The algorithm uses distance-from-center combined with multi-octave noise to create
 * island shapes that are roughly centered but have organic, irregular edges.
 * Edge falloff enforces a minimum margin from texture borders.
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

	/** Get the computed world size in centimeters (derived from land area + ocean padding) */
	float GetComputedWorldSizeCm() const { return Settings.ComputedWorldSizeCm; }

protected:
	/**
	 * Generate the binary land mask from noise-based island mask values.
	 * 
	 * Algorithm:
	 *   1. For each pixel, compute GetIslandMask() value (0.0 to 1.0)
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
	 * Compute the "island-ness" value for a normalised texture coordinate.
	 * Higher values are more likely to be land.
	 * 
	 * Components:
	 *   - Distance from centre: land concentrated toward middle
	 *   - Coast noise: irregular coastline detail
	 *   - Shape noise: large-scale island shape variation
	 *   - Detail noise: fine bumps and indentations
	 *   - Edge falloff: enforces minimum margin from texture borders
	 * 
	 * @param NormX, NormY - Normalised coordinates (0.0 to 1.0)
	 * @return Island mask value (0.0 to 1.0, higher = more land-like)
	 */
	float GetIslandMask(float NormX, float NormY) const;

protected:
	/** Generation settings */
	FLandmassSettings Settings;

	/** Random stream for deterministic generation */
	FRandomStream RandomStream;

	// === Seed-derived noise offsets (generated in Initialize) ===
	// Each layer samples noise at a different offset to decorrelate patterns.
	// These replace hard-coded offsets (+50, +100, +200) for better variety per seed.
	
	/** Offset for shape noise layer (low-frequency island shape) */
	FVector2D ShapeNoiseOffset;
	
	/** Offset for detail noise layer (high-frequency coastal details) */
	FVector2D DetailNoiseOffset;
	
	/** Offset for edge falloff noise layer (irregular border padding) */
	FVector2D EdgeNoiseOffset;

	// === Domain Warp Offsets (generated in Initialize) ===
	// Two separate 2D offsets for X and Y warp components to ensure decorrelation.
	
	/** Offset for warp X-component noise sampling */
	FVector2D WarpOffsetX;
	
	/** Offset for warp Y-component noise sampling */
	FVector2D WarpOffsetY;

	// === Multi-peak island nucleation points (generated in Initialize) ===
	// Instead of a single centre bias, multiple peaks scattered across the map
	// create natural archipelago patterns with a main island + satellites.

	/** Island peak data: position (normalised 0-1), strength, and radius */
	struct FIslandPeak
	{
		FVector2D Position;  // Centre in normalised coords
		float Strength;      // Bias amplitude (higher = more likely to be land)
		float Radius;        // Falloff radius in normalised coords
	};

	/** Seed-derived island peaks for multi-island generation */
	TArray<FIslandPeak> IslandPeaks;

	/** Land mask - 1 = land, 0 = ocean (uint8 for thread-safe parallel writes) */
	TArray<uint8> LandMask;

	/** List of all land pixel coordinates */
	TArray<FIntPoint> LandPixels;

	/** Black/white preview texture */
	UPROPERTY()
	TObjectPtr<UTexture2D> PreviewTexture;
};
