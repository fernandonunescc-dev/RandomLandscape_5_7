// DesertTerrainGenerator.h
// Desert biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Desert biome terrain generator.
 * Currently flat - ready for future implementation of dunes and flat sandy areas.
 */
class RANDOMLANDSCAPE_5_7_API FDesertTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Desert; }
};
