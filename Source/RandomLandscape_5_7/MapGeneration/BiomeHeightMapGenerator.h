// BiomeHeightMapGenerator.h
// Efficient biome heightmap generation using FastNoise2 GenUniformGrid2D

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include <FastNoise/FastNoise.h>

/**
 * Generates heightmaps for each biome using SIMD-optimized batch generation.
 * Uses GenUniformGrid2D for efficient grid-based noise generation instead of
 * per-point sampling with GenSingle2D.
 * 
 * Workflow:
 * 1. Generate individual heightmaps for each biome type (Forest, Ocean, Desert, Snow, etc.)
 * 2. Lerp/blend between biome heightmaps based on biome boundaries for smooth transitions
 */
class RANDOMLANDSCAPE_5_7_API FBiomeHeightMapGenerator
{
public:
	FBiomeHeightMapGenerator() = default;

	/**
	 * Generate a heightmap for a specific biome using GenUniformGrid2D.
	 * @param BiomeType - The biome to generate heightmap for
	 * @param MeshSettings - Biome-specific mesh/noise settings
	 * @param Resolution - Grid resolution (width and height in samples)
	 * @param Seed - Random seed for deterministic generation
	 * @param MapSizeInMeters - Total map size for frequency scaling
	 * @param bTileable - If true, uses GenTileable2D for seamless edges
	 * @return Array of height values in Unreal Units (normalized to min/max height range)
	 */
	static TArray<float> GenerateBiomeHeightMap(
		EBiomeType BiomeType,
		const FBiomeMeshSettings& MeshSettings,
		int32 Resolution,
		int32 Seed,
		float MapSizeInMeters,
		bool bTileable = false);

	/**
	 * Generate heightmaps for all biomes at once.
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
	 * Performs bilinear interpolation and biome blending.
	 * @param NormX - Normalized X position (0-1)
	 * @param NormY - Normalized Y position (0-1)
	 * @param HeightMapResolution - Resolution of the noise heightmaps (e.g., 4096 or 8192)
	 * @param BiomeTextureResolution - Resolution of biome assignment map (for biome lookup)
	 * @param BiomeHeightMaps - Pre-generated heightmaps per biome
	 * @param BiomeMap - Biome assignments per pixel (at BiomeTextureResolution)
	 * @param LandMask - Land/water mask (at BiomeTextureResolution)
	 * @param BiomeConfigs - Biome configuration array
	 * @param BlendRadius - Blend radius in normalized coords (0-1)
	 * @return Blended height value in Unreal Units
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

private:
	/**
	 * Create a FastNoise2 fractal Perlin generator with the given settings.
	 */
	static FastNoise::SmartNode<FastNoise::Generator> CreatePerlinFractalGenerator(
		int32 Octaves, 
		float Persistence, 
		float Lacunarity = 2.0f);

	/**
	 * Generate raw noise grid using GenUniformGrid2D.
	 * @param Generator - FastNoise2 generator node
	 * @param Resolution - Grid resolution (width = height)
	 * @param Frequency - Noise frequency
	 * @param Seed - Random seed
	 * @param MapSizeInMeters - Map size for scaling
	 * @return Array of noise values (typically -1 to 1 range)
	 */
	static TArray<float> GenerateNoiseGrid(
		const FastNoise::SmartNode<FastNoise::Generator>& Generator,
		int32 Resolution,
		float Frequency,
		int32 Seed,
		float MapSizeInMeters);

	/**
	 * Generate tileable noise grid using GenTileable2D.
	 */
	static TArray<float> GenerateTileableNoiseGrid(
		const FastNoise::SmartNode<FastNoise::Generator>& Generator,
		int32 Resolution,
		float Frequency,
		int32 Seed,
		float MapSizeInMeters);
};
