// DesertTerrainGenerator.cpp
// Desert biome terrain generation implementation

#include "DesertTerrainGenerator.h"

float FDesertTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const
{
	// TODO: Implement desert terrain with gentle dunes and flat sandy areas
	// For now, return flat terrain at the midpoint height
	
	float MinHeightUU = BiomeConfig.MinHeightInMeters * 100.0f;
	float MaxHeightUU = BiomeConfig.MaxHeightInMeters * 100.0f;
	
	// Flat at midpoint
	return (MinHeightUU + MaxHeightUU) * 0.5f;
}
