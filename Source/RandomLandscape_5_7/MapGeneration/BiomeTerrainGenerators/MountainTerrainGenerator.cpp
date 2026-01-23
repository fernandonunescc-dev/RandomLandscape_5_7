// MountainTerrainGenerator.cpp
// Mountain biome terrain generation implementation

#include "MountainTerrainGenerator.h"

float FMountainTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// TODO: Implement dramatic mountain terrain with peaks and ridges
	// For now, return flat terrain at the midpoint height
	
	float MinHeightUU = MeshSettings.MinHeightInMeters * 100.0f;
	float MaxHeightUU = MeshSettings.MaxHeightInMeters * 100.0f;
	
	// Flat at midpoint
	return (MinHeightUU + MaxHeightUU) * 0.5f;
}
