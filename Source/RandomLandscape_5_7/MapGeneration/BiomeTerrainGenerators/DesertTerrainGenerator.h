// DesertTerrainGenerator.h
// Desert biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Desert biome terrain generator.
 * Creates gentle rolling dunes with occasional flat areas using FastNoise2 Perlin noise.
 */
class RANDOMLANDSCAPE_5_7_API FDesertTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:

	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Desert; }
};
