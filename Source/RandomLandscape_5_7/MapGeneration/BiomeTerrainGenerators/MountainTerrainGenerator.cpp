// MountainTerrainGenerator.cpp
// Mountain biome terrain generation implementation

#include "MountainTerrainGenerator.h"

float FMountainTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const
{
	// TODO: Implement dramatic mountain terrain with peaks and ridges
	// For now, return flat terrain at the midpoint height
	
	float MinHeightUU = BiomeConfig.MinHeightInMeters * 100.0f;
	float MaxHeightUU = BiomeConfig.MaxHeightInMeters * 100.0f;
	
	// Flat at midpoint
	return (MinHeightUU + MaxHeightUU) * 0.5f;
}
