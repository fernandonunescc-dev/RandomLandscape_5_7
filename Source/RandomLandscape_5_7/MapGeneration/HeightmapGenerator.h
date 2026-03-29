// HeightmapGenerator.h
// Generates per-biome heightmap textures based on biome type and terrain settings.
// White = tallest possible point, Black = lowest possible point.

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include "BiomeMapGenerator.h"
#include "HeightmapGenerator.generated.h"

class UTexture2D;

/**
 * Settings for heightmap generation.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FHeightmapSettings
{
	GENERATED_BODY()

	/** Seed for heightmap noise generation. 0 = use biome seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap")
	int32 HeightmapSeed = 0;

	/** Texture resolution for heightmap output (width = height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 TextureResolution = 512;

	/**
	 * Maximum terrain height in world units (cm).
	 * Controls the absolute ceiling of the terrain mesh.
	 * Individual biome HeightScale values are relative to this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap", meta = (ClampMin = "1000.0", ClampMax = "200000.0", UIMin = "1000.0", UIMax = "200000.0"))
	float MaxMapHeight = 50000.0f;

	/**
	 * Width of the blending zone at biome boundaries (in pixels).
	 * Larger values produce smoother transitions between biomes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap", meta = (ClampMin = "1", ClampMax = "128"))
	int32 BlendRadius = 32;

	/**
	 * Number of smoothing passes applied to the composited heightmap.
	 * Each pass blurs sharp edges between biomes for a more natural look.
	 * 0 = no smoothing (raw heightmaps), higher = smoother terrain.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Heightmap", meta = (ClampMin = "0", ClampMax = "20"))
	int32 SmoothingPasses = 4;
};

/**
 * Generated heightmap data for a single biome.
 */
USTRUCT(BlueprintType)
struct FBiomeHeightmapResult
{
	GENERATED_BODY()

	/** The biome type this heightmap belongs to */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heightmap")
	EBiomeType BiomeType = EBiomeType::Land;

	/** Display name */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heightmap")
	FString DisplayName;

	/**
	 * Height scale used for this biome (0.0 - 1.0).
	 * This fraction of MaxMapHeight is the tallest this biome can reach.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heightmap")
	float HeightScale = 0.0f;

	/**
	 * Per-biome heightmap texture. White = tallest, Black = lowest.
	 * Pixels outside this biome's area are black.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heightmap")
	TObjectPtr<UTexture2D> HeightmapTexture = nullptr;
};

/**
 * Generates per-biome heightmap textures from biome map data.
 *
 * Each biome gets a grayscale texture where:
 * - White (255) = maximum height for that biome (scaled by HeightScale)
 * - Black (0) = minimum height (sea level / base)
 * - Pixels outside the biome area are black
 *
 * Mountain and Volcanic biomes can reach full white (HeightScale ~1.0).
 * Other biomes are capped to lower brightness ranges.
 *
 * Noise generation is biome-type-specific:
 * - Land: Very flat with tiny hills
 * - Forest: Moderate hills and uneven terrain
 * - Desert: Dune patterns
 * - Snow: Similar to Forest
 * - Ice: Super flat with occasional iceberg peaks
 * - Mountain: Strong peaked terrain
 * - Volcanic: Central cone with crater
 *
 * Boundary blending ensures smooth transitions between adjacent biomes.
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UHeightmapGenerator : public UObject
{
	GENERATED_BODY()

public:
	UHeightmapGenerator();

	/** Initialize with settings */
	void Initialize(const FHeightmapSettings& InSettings);

	/**
	 * Generate per-biome heightmap textures.
	 *
	 * @param BiomeMap - Per-pixel biome ID map from UBiomeMapGenerator
	 * @param LandMask - Binary land mask (1=land, 0=ocean)
	 * @param BiomeLayers - Layer settings with terrain configurations
	 * @return true if generation succeeded
	 */
	bool Generate(
		const TArray<int32>& BiomeMap,
		const TArray<uint8>& LandMask,
		const TArray<FBiomeLayerSettings>& BiomeLayers);

	/** Get generated heightmap results */
	const TArray<FBiomeHeightmapResult>& GetResults() const { return Results; }

	/** Get the actual seed used */
	int32 GetSeed() const { return ActualSeed; }

private:
	FHeightmapSettings Settings;
	FRandomStream RandomStream;
	int32 ActualSeed = 0;

	/** Generated heightmap results, one per biome */
	UPROPERTY()
	TArray<FBiomeHeightmapResult> Results;

	/** Compute raw height value for a pixel based on its biome terrain settings */
	float ComputeBiomeHeight(
		float NormX, float NormY,
		EBiomeType BiomeType,
		const FBiomeTerrainSettings& Terrain,
		const FVector2D& BiomeCentroid,
		float BiomeRadius) const;

	/** 2D value noise */
	float Noise2D(float X, float Y) const;

	/** Fractal Brownian Motion */
	float FBM(float X, float Y, int32 Octaves, float Persistence) const;

	/** Compute distance field from biome boundary for blending */
	void ComputeDistanceField(
		const TArray<int32>& BiomeMap,
		int32 Resolution,
		int32 BiomeId,
		TArray<float>& OutDistances) const;

	/** Find centroid and approximate radius of a biome region */
	void FindBiomeExtents(
		const TArray<int32>& BiomeMap,
		int32 Resolution,
		int32 BiomeId,
		FVector2D& OutCentroid,
		float& OutRadius) const;

	/** Create a transient grayscale BGRA8 texture */
	UTexture2D* CreateHeightmapTexture(int32 Resolution, const TArray<float>& HeightData) const;
};
