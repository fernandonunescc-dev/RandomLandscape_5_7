// OceanTerrainGenerator.cpp
// Ocean biome terrain generation implementation
// Uses FastNoise2 Perlin noise for gentle underwater variations

#include "OceanTerrainGenerator.h"

float FOceanTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// Ocean terrain: underwater with gentle variations using Perlin noise
	float NormalizedNoise = FBiomeTerrainGeneratorBase::PerlinNoise(
		NormX, NormY,
		MeshSettings.NoiseFrequency,
		MeshSettings.NoiseOctaves,
		MeshSettings.NoisePersistence,
		Seed,
		MapSizeInMeters
	);
	
	// Return raw noise mapped to -1..1 range
	return (NormalizedNoise * 2.0f) - 1.0f;
}

