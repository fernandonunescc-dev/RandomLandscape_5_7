// OceanTerrainGenerator.h
// Ocean biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Ocean biome terrain generator.
 * Creates underwater terrain with gentle variations.
 */
class RANDOMLANDSCAPE_5_7_API FOceanTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const override;
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Ocean; }

private:
	float FractalNoise(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const;
};
