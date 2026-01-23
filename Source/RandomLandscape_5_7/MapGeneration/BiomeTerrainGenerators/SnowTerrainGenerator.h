// SnowTerrainGenerator.h
// Snow biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Snow biome terrain generator.
 * Creates icy peaks and frozen valleys using FastNoise2 Perlin noise.
 */
class RANDOMLANDSCAPE_5_7_API FSnowTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Snow; }
};
