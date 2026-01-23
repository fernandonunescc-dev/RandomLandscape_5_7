// BiomeTerrainGeneratorBase.h
// Base class for biome-specific terrain generation

#pragma once

#include "CoreMinimal.h"
#include "../BiomeTypes.h"
#include <FastNoise/FastNoise.h>

/**
 * Base class for biome terrain generators.
 * Each biome type can have its own derived class with custom terrain logic.
 * Uses FastNoise2 for high-quality Perlin noise generation.
 */
class RANDOMLANDSCAPE_5_7_API FBiomeTerrainGeneratorBase
{
public:
	FBiomeTerrainGeneratorBase() = default;
	virtual ~FBiomeTerrainGeneratorBase() = default;

	/**
	 * Calculate the terrain height at a normalized position.
	 * @param NormX - Normalized X position (0-1)
	 * @param NormY - Normalized Y position (0-1)
	 * @param MeshSettings - The biome mesh settings with height/noise configuration
	 * @param Seed - Random seed for deterministic generation
	 * @param MapSizeInMeters - The total map size in meters (used for frequency scaling)
	 * @return Height in Unreal Units (centimeters)
	 */
	virtual float CalculateHeight(float NormX, float NormY, const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const = 0;

	/**
	 * Get the biome type this generator handles.
	 */
	virtual EBiomeType GetBiomeType() const = 0;

protected:
	/**
	 * FastNoise2 Perlin noise with fractal Brownian motion (fBm).
	 * Uses true Perlin noise algorithm for high-quality coherent noise.
	 * @param NormX - Normalized X position (0-1)
	 * @param NormY - Normalized Y position (0-1) 
	 * @param Frequency - Base noise frequency
	 * @param Octaves - Number of octave layers
	 * @param Persistence - How much each octave contributes (gain)
	 * @param Seed - Random seed for deterministic generation
	 * @param MapSizeInMeters - Map size for frequency scaling
	 * @return Noise value normalized to 0-1 range
	 */
	static float PerlinNoise(float NormX, float NormY, float Frequency, int32 Octaves, float Persistence, int32 Seed, float MapSizeInMeters)
	{
		// Scale frequency based on map size
		float MapSizeScale = FMath::Max(MapSizeInMeters / 100.0f, 1.0f);
		float ScaledFrequency = Frequency * MapSizeScale;
		
		// Create FastNoise2 Perlin generator with fractal fBm
		auto PerlinGen = FastNoise::New<FastNoise::Perlin>();
		
		auto FractalGen = FastNoise::New<FastNoise::FractalFBm>();
		FractalGen->SetSource(PerlinGen);
		FractalGen->SetOctaveCount(Octaves);
		FractalGen->SetGain(Persistence);
		FractalGen->SetLacunarity(2.0f);
		
		// Calculate scaled coordinates
		float X = NormX * ScaledFrequency;
		float Y = NormY * ScaledFrequency;
		
		// GenSingle2D returns noise in approximately -1 to 1 range
		// The seed is passed directly to the generation function
		float NoiseValue = FractalGen->GenSingle2D(X, Y, Seed);
		
		// Normalize to 0-1 range
		return (NoiseValue + 1.0f) * 0.5f;
	}
	
	/**
	 * Simple hash-based noise function (legacy fallback).
	 * Can be used by derived classes for basic noise generation.
	 */
	static float HashNoise(int32 X, int32 Y, int32 Seed)
	{
		int32 N = X + Y * 57 + Seed;
		N = (N << 13) ^ N;
		return (1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
	}

	/**
	 * Smoothstep interpolation function.
	 */
	static float Smoothstep(float T)
	{
		return T * T * (3.0f - 2.0f * T);
	}
};
