// VolcanicTerrainGenerator.h
// Volcanic biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Volcanic biome terrain generator.
 * Creates dramatic steep formations using FastNoise2 Perlin noise with high variance.
 */
class RANDOMLANDSCAPE_5_7_API FVolcanicTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Volcanic; }
};
