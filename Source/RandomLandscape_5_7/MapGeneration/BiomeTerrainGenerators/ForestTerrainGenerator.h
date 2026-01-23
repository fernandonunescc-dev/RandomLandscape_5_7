// ForestTerrainGenerator.h
// Forest biome terrain generation - smooth rolling hills

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Forest biome terrain generator.
 * Creates smooth rolling hills using FastNoise2 Perlin noise with fractal fBm
 * for gentle, natural-looking terrain.
 */
class RANDOMLANDSCAPE_5_7_API FForestTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:

	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Forest; }
};
