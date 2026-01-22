// VolcanicTerrainGenerator.h
// Volcanic biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Volcanic biome terrain generator.
 * Currently flat - ready for future implementation of craters, lava flows, and volcanic formations.
 */
class RANDOMLANDSCAPE_5_7_API FVolcanicTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Volcanic; }
};
