// BiomeHeightMapGenerator.h
// Coordinates heightmap generation by delegating to individual terrain generators

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"

/**
 * Coordinates heightmap generation for biomes.
 * Delegates actual generation to individual FBiomeTerrainGeneratorBase subclasses
 * via FBiomeTerrainGeneratorFactory.
 * 
 * Workflow:
 * 1. Call GenerateBiomeHeightMap() or GenerateAllBiomeHeightMaps()
 * 2. Factory retrieves the appropriate terrain generator for each biome
 * 3. Each generator uses its own noise algorithm (Perlin, Simplex, Voronoi, etc.)
 * 4. SampleBlendedHeight() blends between biome heightmaps at boundaries
 */
class RANDOMLANDSCAPE_5_7_API FBiomeHeightMapGenerator
{
public:
	FBiomeHeightMapGenerator() = default;

	/**
	 * Generate a heightmap for a specific biome via its terrain generator.
	 * @param BiomeType - The biome to generate
	 * @param MeshSettings - Biome-specific noise settings
	 * @param Resolution - Grid resolution
	 * @param Seed - Random seed
	 * @param MapSizeInMeters - Map size for frequency scaling
	 * @param bTileable - Use tileable generation
	 * @return Array of raw noise values (-1 to 1 range), empty if no generator found
	 */
	static TArray<float> GenerateBiomeHeightMap(
		EBiomeType BiomeType,
		const FBiomeMeshSettings& MeshSettings,
		int32 Resolution,
		int32 Seed,
		float MapSizeInMeters,
		bool bTileable = false);

	/**
	 * Generate heightmaps for multiple biomes.
	 * @param BiomeSettings - Array of biome mesh settings
	 * @param Resolution - Grid resolution
	 * @param Seed - Base seed (each biome gets a derived seed)
	 * @param MapSizeInMeters - Map size for scaling
	 * @param bTileable - Use tileable generation
	 * @return Map of biome type to heightmap array
	 */
	static TMap<EBiomeType, TArray<float>> GenerateAllBiomeHeightMaps(
		const TArray<FBiomeMeshSettings>& BiomeSettings,
		int32 Resolution,
		int32 Seed,
		float MapSizeInMeters,
		bool bTileable = false);

	/**
	 * Sample a blended height from pre-generated biome heightmaps.
	 * Performs bilinear interpolation and biome blending at boundaries.
	 */
	static float SampleBlendedHeight(
		float NormX, float NormY,
		int32 HeightMapResolution,
		int32 BiomeTextureResolution,
		const TMap<EBiomeType, TArray<float>>& BiomeHeightMaps,
		const TArray<int32>& BiomeMap,
		const TArray<bool>& LandMask,
		const TArray<FBiomeConfig>& BiomeConfigs,
		float BlendRadius = 0.02f);
};
