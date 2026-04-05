// FastNoise2Noise.h
// FastNoise2-backed noise utilities for 2D and 3D terrain generation.
// Provides SIMD-accelerated noise via Auburn/FastNoise2 node-graph API.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "FastNoise/FastNoise.h"
THIRD_PARTY_INCLUDES_END

/**
 * FastNoise2-backed noise utilities for SIMD-accelerated bulk grid generation.
 *
 * IMPORTANT: FN2 SmartNodes are NOT thread-safe for concurrent creation/destruction.
 * Build node trees once on the main thread, then pass them to GenGrid2D/GenGrid3D.
 * For per-sample noise in ParallelFor, use WorldNoise:: functions (see NoiseUtility.h).
 *
 * All functions are deterministic: same inputs always produce the same output.
 */
namespace FN2
{
	// ----------------------------------------------------------------
	// Node builders — create reusable FN2 node trees
	// ----------------------------------------------------------------

	/** Build a standard FBM fractal node with Simplex source. */
	inline FastNoise::SmartNode<FastNoise::Generator> MakeFBM(int32 Octaves = 5, float Gain = 0.5f, float Lacunarity = 2.0f)
	{
		auto Source = FastNoise::New<FastNoise::Simplex>();
		auto Fractal = FastNoise::New<FastNoise::FractalFBm>();
		Fractal->SetSource(Source);
		Fractal->SetOctaveCount(Octaves);
		Fractal->SetGain(Gain);
		Fractal->SetLacunarity(Lacunarity);  // Pass through to FN2
		return Fractal;
	}

	/** Build a ridged fractal node with Simplex source. */
	inline FastNoise::SmartNode<FastNoise::Generator> MakeRidged(int32 Octaves = 5, float Gain = 0.5f, float Lacunarity = 2.0f)
	{
		auto Source = FastNoise::New<FastNoise::Simplex>();
		auto Fractal = FastNoise::New<FastNoise::FractalRidged>();
		Fractal->SetSource(Source);
		Fractal->SetOctaveCount(Octaves);
		Fractal->SetGain(Gain);
		Fractal->SetLacunarity(Lacunarity);  // Pass through to FN2
		return Fractal;
	}

	/** Build a domain-warped FBM for organic-looking terrain. */
	inline FastNoise::SmartNode<FastNoise::Generator> MakeWarpedFBM(int32 Octaves = 5, float Gain = 0.5f, float WarpAmplitude = 30.0f)
	{
		auto FBM = MakeFBM(Octaves, Gain);

		auto DomainWarp = FastNoise::New<FastNoise::DomainWarpGradient>();
		DomainWarp->SetSource(FBM);
		DomainWarp->SetWarpAmplitude(WarpAmplitude);

		auto WarpedFractal = FastNoise::New<FastNoise::DomainWarpFractalProgressive>();
		WarpedFractal->SetSource(DomainWarp);
		WarpedFractal->SetOctaveCount(3);
		WarpedFractal->SetGain(0.5f);
		WarpedFractal->SetLacunarity(2.0f);
		return WarpedFractal;
	}

	// ----------------------------------------------------------------
	// Bulk generation — SIMD-accelerated grid fills
	// ----------------------------------------------------------------

	/**
	 * Generate a 2D grid of noise values using any FN2 generator node.
	 * Output is row-major: Index = Y * SizeX + X
	 */
	inline void GenGrid2D(
		const FastNoise::SmartNode<FastNoise::Generator>& Generator,
		TArray<float>& OutData,
		int32 StartX, int32 StartY,
		int32 SizeX, int32 SizeY,
		float Frequency, int32 Seed)
	{
		OutData.SetNumUninitialized(SizeX * SizeY);
		Generator->GenUniformGrid2D(
			OutData.GetData(),
			static_cast<float>(StartX), static_cast<float>(StartY),
			SizeX, SizeY,
			Frequency, Frequency, Seed);
	}

	/**
	 * Generate a 3D grid of noise values using any FN2 generator node.
	 * Output is: Index = Z * SizeY * SizeX + Y * SizeX + X
	 */
	inline void GenGrid3D(
		const FastNoise::SmartNode<FastNoise::Generator>& Generator,
		TArray<float>& OutData,
		int32 StartX, int32 StartY, int32 StartZ,
		int32 SizeX, int32 SizeY, int32 SizeZ,
		float Frequency, int32 Seed)
	{
		OutData.SetNumUninitialized(SizeX * SizeY * SizeZ);
		Generator->GenUniformGrid3D(
			OutData.GetData(),
			static_cast<float>(StartX), static_cast<float>(StartY), static_cast<float>(StartZ),
			SizeX, SizeY, SizeZ,
			Frequency, Frequency, Frequency, Seed);
	}

}
