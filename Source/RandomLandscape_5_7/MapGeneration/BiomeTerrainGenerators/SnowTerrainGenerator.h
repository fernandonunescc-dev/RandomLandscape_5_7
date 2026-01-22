// SnowTerrainGenerator.h
// Snow biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Snow biome terrain generator.
 * Currently flat - ready for future implementation of snowy peaks and frozen terrain.
 */
class RANDOMLANDSCAPE_5_7_API FSnowTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Snow; }
};
