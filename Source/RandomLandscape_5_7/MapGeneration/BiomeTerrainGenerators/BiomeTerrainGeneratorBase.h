// BiomeTerrainGeneratorBase.h
// Base class for biome-specific terrain generation

#pragma once

#include "CoreMinimal.h"
#include "../BiomeTypes.h"

/**
 * Base class for biome terrain generators.
 * Each biome type can have its own derived class with custom terrain logic.
 * Uses FastNoise2 for high-quality Perlin noise generation.
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
};
