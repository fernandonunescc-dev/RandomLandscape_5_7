// ForestTerrainGenerator.h
// Forest biome terrain generation - smooth rolling hills

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Forest biome terrain generator.
 * Creates smooth rolling hills using fractal noise with low frequency
 * and persistence for gentle, natural-looking terrain.
 */
class RANDOMLANDSCAPE_5_7_API FForestTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Forest; }

private:
	/**
	 * Fractal Perlin noise implementation for smooth rolling hills.
	 * Uses multiple octaves with low persistence for gentle terrain.
	 * @param MapSizeInMeters - Used to scale frequency appropriately for map size
	 */
	float FractalNoise(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const;
};
