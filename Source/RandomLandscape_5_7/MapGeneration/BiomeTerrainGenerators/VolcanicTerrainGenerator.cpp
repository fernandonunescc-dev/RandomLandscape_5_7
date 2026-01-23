// VolcanicTerrainGenerator.cpp
// Volcanic biome terrain generation implementation

#include "VolcanicTerrainGenerator.h"

float FVolcanicTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	// Volcanic terrain: dramatic steep formations
	float NormalizedNoise = FractalNoise(NormX, NormY, MeshSettings, Seed, MapSizeInMeters);
	
	float MinHeightUU = MeshSettings.MinHeightInMeters * 100.0f;
	float MaxHeightUU = MeshSettings.MaxHeightInMeters * 100.0f;
	
	return FMath::Lerp(MinHeightUU, MaxHeightUU, NormalizedNoise);
}

float FVolcanicTerrainGenerator::FractalNoise(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float MaxValue = 0.0f;
	
	float MapSizeScale = FMath::Max(MapSizeInMeters / 100.0f, 1.0f);
	float BaseFrequency = MeshSettings.NoiseFrequency * MapSizeScale;
	float Frequency = BaseFrequency;
	
	float SeedOffsetX = (Seed % 10000) * 0.37f;
	float SeedOffsetY = (Seed % 10000) * 0.53f;
	
	for (int32 i = 0; i < MeshSettings.NoiseOctaves; ++i)
	{
		float X = (NormX + SeedOffsetX) * Frequency;
		float Y = (NormY + SeedOffsetY) * Frequency;
		
		int32 Xi = FMath::FloorToInt(X);
		int32 Yi = FMath::FloorToInt(Y);
		float Xf = X - Xi;
		float Yf = Y - Yi;
		
		float U = Smoothstep(Xf);
		float V = Smoothstep(Yf);
		
		float A = HashNoise(Xi, Yi, Seed);
		float B = HashNoise(Xi + 1, Yi, Seed);
		float C = HashNoise(Xi, Yi + 1, Seed);
		float D = HashNoise(Xi + 1, Yi + 1, Seed);
		
		float AB = FMath::Lerp(A, B, U);
		float CD = FMath::Lerp(C, D, U);
		float NoiseValue = FMath::Lerp(AB, CD, V);
		
		Total += NoiseValue * Amplitude;
		MaxValue += Amplitude;
		
		Amplitude *= MeshSettings.NoisePersistence;
		Frequency *= 2.0f;
	}
	
	return (Total / MaxValue + 1.0f) * 0.5f;
}
