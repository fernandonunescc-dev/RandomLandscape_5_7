// ForestTerrainGenerator.cpp
// Forest biome terrain generation implementation
// Uses FastNoise2 Perlin noise for smooth rolling hills

#include "ForestTerrainGenerator.h"

float FForestTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// Use FastNoise2 Perlin noise for smooth rolling hills
	float NormalizedNoise = FBiomeTerrainGeneratorBase::PerlinNoise(
		NormX, NormY,
		MeshSettings.NoiseFrequency,
		MeshSettings.NoiseOctaves,
		MeshSettings.NoisePersistence,
		Seed,
		MapSizeInMeters
	);
	
	// Convert biome height settings from meters to Unreal Units (cm)
	float MinHeightUU = MeshSettings.MinHeightInMeters * 100.0f;
	float MaxHeightUU = MeshSettings.MaxHeightInMeters * 100.0f;
	
	// Lerp between min and max height based on noise
	return FMath::Lerp(MinHeightUU, MaxHeightUU, NormalizedNoise);
}

