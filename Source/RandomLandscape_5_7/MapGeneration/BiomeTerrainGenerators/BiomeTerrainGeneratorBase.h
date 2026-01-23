// BiomeTerrainGeneratorBase.h
// Base class for biome-specific terrain generation

#pragma once

#include "CoreMinimal.h"
#include "../BiomeTypes.h"

/**
 * Base class for biome terrain generators.
 * Each biome type can have its own derived class with custom terrain logic.
 */
class RANDOMLANDSCAPE_5_7_API FBiomeTerrainGeneratorBase
{
public:
	FBiomeTerrainGeneratorBase() = default;
	virtual ~FBiomeTerrainGeneratorBase() = default;

	/**
	 * Calculate the terrain height at a normalized position.
	 * @param NormX - Normalized X position (0-1)
	 * @param NormY - Normalized Y position (0-1)
	 * @param MeshSettings - The biome mesh settings with height/noise configuration
	 * @param Seed - Random seed for deterministic generation
	 * @param MapSizeInMeters - The total map size in meters (used for frequency scaling)
	 * @return Height in Unreal Units (centimeters)
	 */
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const = 0;

	/**
	 * Get the biome type this generator handles.
	 */
	virtual EBiomeType GetBiomeType() const = 0;

protected:
	/**
	 * Simple hash-based noise function.
	 * Can be used by derived classes for basic noise generation.
	 */
	static float HashNoise(int32 X, int32 Y, int32 Seed)
	{
		int32 N = X + Y * 57 + Seed;
		N = (N << 13) ^ N;
		return (1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
	}

	/**
	 * Smoothstep interpolation function.
	 */
	static float Smoothstep(float T)
	{
		return T * T * (3.0f - 2.0f * T);
	}
};
