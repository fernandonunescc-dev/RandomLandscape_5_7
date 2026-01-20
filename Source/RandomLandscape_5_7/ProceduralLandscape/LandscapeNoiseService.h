// Copyright Fernando Araujo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Simple noise generation service for terrain height.
 * Generates basic fractal noise for terrain height.
 * Thread-safe for use in async generation tasks.
 */
class RANDOMLANDSCAPE_5_7_API FLandscapeNoiseService
{
public:
	FLandscapeNoiseService();
	~FLandscapeNoiseService();

	/**
	 * Initialize the noise generator with a seed.
	 * @param Seed Random seed for consistent terrain generation
	 */
	void Initialize(int32 Seed = 1337);

	/**
	 * Generate height map for a rectangular region.
	 * Thread-safe - can be called from async tasks.
	 * @param StartX Start X in meters
	 * @param StartY Start Y in meters
	 * @param SizeMeters Size of the region in meters
	 * @param Resolution Number of samples per side
	 * @param OutHeights Output array of heights (Unreal units)
	 */
	void GenerateHeightMapRegion(
		float StartX, float StartY,
		float SizeMeters,
		int32 Resolution,
		TArray<float>& OutHeights) const;

	// =========================================================================
	// NOISE CONFIGURATION
	// =========================================================================

	/** Base frequency of the noise. Lower = larger features. Default: 0.002 */
	float NoiseFrequency = 0.002f;

	/** Number of octaves for fractal noise. More = more detail. Default: 5 */
	int32 NoiseOctaves = 5;

	/** Lacunarity - frequency multiplier per octave. Default: 2.0 */
	float NoiseLacunarity = 2.0f;

	/** Gain - amplitude multiplier per octave. Default: 0.5 */
	float NoiseGain = 0.5f;

	/** Maximum height in Unreal units (cm). Default: 5000 */
	float MaxHeight = 5000.0f;

	/** Minimum height in Unreal units (cm). Default: 0 */
	float MinHeight = 0.0f;

	/**
	 * Height redistribution exponent. Controls how rare tall peaks are.
	 * 1.0 = Linear (uniform distribution)
	 * 2.0 = Squared (tall peaks are rarer)
	 * 3.0 = Cubed (tall peaks are very rare, most terrain is low)
	 * Higher values = flatter terrain with occasional dramatic peaks
	 * Default: 1.0 (no redistribution)
	 */
	float HeightExponent = 1.0f;

private:
	/** Random seed for noise generation */
	int32 NoiseSeed = 1337;

	/** FastNoise2 terrain generator node (opaque pointer) */
	void* TerrainNoiseNode = nullptr;

	/** Initialize FastNoise2 node */
	void InitializeNoiseNode();
	
	/** Cleanup FastNoise2 node */
	void CleanupNoiseNode();

	/** Sample noise at a position (returns -1 to 1) */
	float SampleNoise(float X, float Y) const;
};
