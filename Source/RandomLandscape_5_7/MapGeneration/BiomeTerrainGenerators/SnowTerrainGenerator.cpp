// SnowTerrainGenerator.cpp
// Snow biome terrain generation implementation

#include "SnowTerrainGenerator.h"

float FSnowTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// TODO: Implement snow terrain with icy peaks and frozen valleys
	// For now, return flat terrain at the midpoint height
	
	float MinHeightUU = MeshSettings.MinHeightInMeters * 100.0f;
	float MaxHeightUU = MeshSettings.MaxHeightInMeters * 100.0f;
	
	// Flat at midpoint
	return (MinHeightUU + MaxHeightUU) * 0.5f;
}
