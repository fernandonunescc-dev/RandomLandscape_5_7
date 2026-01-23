// DesertTerrainGenerator.h
// Desert biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Desert biome terrain generator.
 * Creates gentle rolling dunes with occasional flat areas.
 */
class RANDOMLANDSCAPE_5_7_API FDesertTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Desert; }

private:
	float FractalNoise(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const;
};
