// DesertTerrainGenerator.cpp
// Desert biome terrain generation implementation
// Uses FastNoise2 Perlin noise for gentle rolling dunes

#include "DesertTerrainGenerator.h"

float FDesertTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// Desert terrain: gentle rolling dunes with occasional flat areas using Perlin noise
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

