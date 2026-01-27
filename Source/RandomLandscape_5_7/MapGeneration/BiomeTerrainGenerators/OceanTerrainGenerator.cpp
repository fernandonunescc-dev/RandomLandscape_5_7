// OceanTerrainGenerator.cpp
// Ocean biome terrain generation - gentle underwater variations

#include "OceanTerrainGenerator.h"

TArray<float> FOceanTerrainGenerator::GenerateHeightMap(
	const FBiomeMeshSettings& MeshSettings,
	int32 Resolution,
	int32 Seed,
	float MapSizeInMeters,
	bool bTileable) const
{
	// Ocean uses Perlin fractal fBm for gentle underwater terrain
	auto Generator = CreatePerlinFractalGenerator(
		MeshSettings.NoiseOctaves,
		MeshSettings.NoisePersistence);

	TArray<float> HeightMap;
	if (bTileable)
	{
		HeightMap = GenerateTileableNoiseGrid(Generator, Resolution, MeshSettings.NoiseFrequency, Seed, MapSizeInMeters);
	}
	else
	{
		HeightMap = GenerateNoiseGrid(Generator, Resolution, MeshSettings.NoiseFrequency, Seed, MapSizeInMeters);
	}

	UE_LOG(LogTemp, Log, TEXT("FOceanTerrainGenerator: Generated %dx%d heightmap"), Resolution, Resolution);
	return HeightMap;
}
