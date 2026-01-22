// MountainTerrainGenerator.h
// Mountain biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Mountain biome terrain generator.
 * Currently flat - ready for future implementation of dramatic peaks and ridges.
 */
class RANDOMLANDSCAPE_5_7_API FMountainTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Mountain; }
};
