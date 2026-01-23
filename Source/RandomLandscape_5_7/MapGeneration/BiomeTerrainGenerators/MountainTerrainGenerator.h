// MountainTerrainGenerator.h
// Mountain biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Mountain biome terrain generator.
 * Creates dramatic peaks and ridges using FastNoise2 Perlin noise with high frequency.
 */
class RANDOMLANDSCAPE_5_7_API FMountainTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Mountain; }
};
