// ForestTerrainGenerator.cpp
// Forest biome terrain generation implementation
// Uses FastNoise2 Perlin noise for smooth rolling hills

#include "ForestTerrainGenerator.h"

float FForestTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// Use FastNoise2 Perlin noise for smooth rolling hills
	// Returns normalized noise value in 0..1 range
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

