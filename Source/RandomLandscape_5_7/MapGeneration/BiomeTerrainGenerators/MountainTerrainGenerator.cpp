// MountainTerrainGenerator.cpp
// Mountain biome terrain generation implementation
// Uses FastNoise2 Perlin noise for dramatic peaks and ridges

#include "MountainTerrainGenerator.h"

float FMountainTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// Mountain terrain: dramatic peaks and ridges using Perlin noise
	float NormalizedNoise = FBiomeTerrainGeneratorBase::PerlinNoise(
		NormX, NormY,
		MeshSettings.NoiseFrequency,
		MeshSettings.NoiseOctaves,
		MeshSettings.NoisePersistence,
		Seed,
		MapSizeInMeters
	);
	
	float MinHeightUU = MeshSettings.MinHeightInMeters * 100.0f;
	float MaxHeightUU = MeshSettings.MaxHeightInMeters * 100.0f;
	
	return FMath::Lerp(MinHeightUU, MaxHeightUU, NormalizedNoise);
}

