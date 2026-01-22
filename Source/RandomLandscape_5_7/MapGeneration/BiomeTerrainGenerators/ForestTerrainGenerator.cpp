// ForestTerrainGenerator.cpp
// Forest biome terrain generation implementation

#include "ForestTerrainGenerator.h"

float FForestTerrainGenerator::CalculateHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const
{
	// Use fractal noise for smooth rolling hills
	float NormalizedNoise = FractalNoise(NormX, NormY, BiomeConfig, Seed, MapSizeInMeters);
	
	// Convert biome height settings from meters to Unreal Units (cm)
	float MinHeightUU = BiomeConfig.MinHeightInMeters * 100.0f;
	float MaxHeightUU = BiomeConfig.MaxHeightInMeters * 100.0f;
	
	// Lerp between min and max height based on noise
	return FMath::Lerp(MinHeightUU, MaxHeightUU, NormalizedNoise);
}

float FForestTerrainGenerator::FractalNoise(float NormX, float NormY, const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float MaxValue = 0.0f;
	
	// Scale frequency based on map size
	// For a 100m map, NoiseFrequency of 1.0 gives ~1 hill
	// For a 1000m map, we want ~10 hills at the same NoiseFrequency setting
	// So we multiply by (MapSize / 100) to maintain consistent feature density
	float MapSizeScale = FMath::Max(MapSizeInMeters / 100.0f, 1.0f);
	float BaseFrequency = BiomeConfig.NoiseFrequency * MapSizeScale;
	float Frequency = BaseFrequency;
	
	// Use seed to offset the noise for variation
	float SeedOffsetX = (Seed % 10000) * 0.37f;
	float SeedOffsetY = (Seed % 10000) * 0.53f;
	
	for (int32 i = 0; i < BiomeConfig.NoiseOctaves; ++i)
	{
		// Calculate noise coordinates
		float X = (NormX + SeedOffsetX) * Frequency;
		float Y = (NormY + SeedOffsetY) * Frequency;
		
		// Get integer and fractional parts
		int32 Xi = FMath::FloorToInt(X);
		int32 Yi = FMath::FloorToInt(Y);
		float Xf = X - Xi;
		float Yf = Y - Yi;
		
		// Smooth interpolation using smoothstep
		float U = Smoothstep(Xf);
		float V = Smoothstep(Yf);
		
		// Sample noise at 4 corners
		float A = HashNoise(Xi, Yi, Seed);
		float B = HashNoise(Xi + 1, Yi, Seed);
		float C = HashNoise(Xi, Yi + 1, Seed);
		float D = HashNoise(Xi + 1, Yi + 1, Seed);
		
		// Bilinear interpolation
		float AB = FMath::Lerp(A, B, U);
		float CD = FMath::Lerp(C, D, U);
		float NoiseValue = FMath::Lerp(AB, CD, V);
		
		Total += NoiseValue * Amplitude;
		MaxValue += Amplitude;
		
		// Decrease amplitude and increase frequency for next octave
		Amplitude *= BiomeConfig.NoisePersistence;
		Frequency *= 2.0f;
	}
	
	// Normalize noise to 0-1 range
	return (Total / MaxValue + 1.0f) * 0.5f;
}
