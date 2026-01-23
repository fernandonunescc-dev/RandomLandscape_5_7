// BiomeTerrainGeneratorBase.h
// Base class for biome-specific terrain generation

#pragma once

#include "CoreMinimal.h"
#include "../BiomeTypes.h"
#include <FastNoise/FastNoise.h>

/**
 * Base class for biome terrain generators.
 * Each biome type can have its own derived class with custom terrain/noise logic.
 */
class RANDOMLANDSCAPE_5_7_API FBiomeTerrainGeneratorBase
{
public:
	FBiomeTerrainGeneratorBase() = default;
	virtual ~FBiomeTerrainGeneratorBase() = default;

	/**
	 * Get the biome type this generator handles.
	 */
	virtual EBiomeType GetBiomeType() const = 0;

	/**
	 * Generate a heightmap for this biome.
	 * Override in derived classes to use different noise algorithms per biome.
	 * @param MeshSettings - Biome-specific noise settings
	 * @param Resolution - Grid resolution (width = height)
	 * @param Seed - Random seed
	 * @param MapSizeInMeters - Map size for frequency scaling
	 * @param bTileable - Use tileable generation for seamless edges
	 * @return Array of raw noise values (typically -1 to 1 range)
	 */
	virtual TArray<float> GenerateHeightMap(
		const FBiomeMeshSettings& MeshSettings,
		int32 Resolution,
		int32 Seed,
		float MapSizeInMeters,
		bool bTileable = false) const;

protected:
	// --- FastNoise2 helper methods for derived classes ---

	/** Create a fractal Perlin generator (default noise type) */
	static FastNoise::SmartNode<FastNoise::Generator> CreatePerlinFractalGenerator(
		int32 Octaves,
		float Persistence,
		float Lacunarity = 2.0f);

	/** Generate noise grid using GenUniformGrid2D */
	static TArray<float> GenerateNoiseGrid(
		const FastNoise::SmartNode<FastNoise::Generator>& Generator,
		int32 Resolution,
		float Frequency,
		int32 Seed,
		float MapSizeInMeters);

	/** Generate tileable noise grid using GenTileable2D */
	static TArray<float> GenerateTileableNoiseGrid(
		const FastNoise::SmartNode<FastNoise::Generator>& Generator,
		int32 Resolution,
		float Frequency,
		int32 Seed,
		float MapSizeInMeters);
};
