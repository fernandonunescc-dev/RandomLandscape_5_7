// OceanTerrainGenerator.h
// Ocean biome terrain generation

#pragma once

#include "BiomeTerrainGeneratorBase.h"

/**
 * Ocean biome terrain generator.
 * Creates underwater terrain with gentle variations using FastNoise2 Perlin noise.
 */
class RANDOMLANDSCAPE_5_7_API FOceanTerrainGenerator : public FBiomeTerrainGeneratorBase
{
public:
	virtual EBiomeType GetBiomeType() const override { return EBiomeType::Ocean; }

	/** Ocean-specific heightmap generation */
	virtual TArray<float> GenerateHeightMap(
		const FBiomeMeshSettings& MeshSettings,
		int32 Resolution,
		int32 Seed,
		float MapSizeInMeters,
		bool bTileable = false) const override;
};
