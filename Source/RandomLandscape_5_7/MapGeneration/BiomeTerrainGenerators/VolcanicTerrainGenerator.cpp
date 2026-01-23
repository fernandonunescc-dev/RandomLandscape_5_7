// VolcanicTerrainGenerator.cpp
// Volcanic biome terrain generation implementation

#include "VolcanicTerrainGenerator.h"

float FVolcanicTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// TODO: Implement volcanic terrain with craters, lava flows, and steep formations
	// For now, return flat terrain at the midpoint height
	
	float MinHeightUU = MeshSettings.MinHeightInMeters * 100.0f;
	float MaxHeightUU = MeshSettings.MaxHeightInMeters * 100.0f;
	
	// Flat at midpoint
	return (MinHeightUU + MaxHeightUU) * 0.5f;
}
