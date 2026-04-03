// FastNoise2Noise.h
// FastNoise2-backed noise utilities for 2D and 3D terrain generation.
// Provides SIMD-accelerated noise via Auburn/FastNoise2 node-graph API.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "FastNoise/FastNoise.h"
THIRD_PARTY_INCLUDES_END

/**
 * FastNoise2-backed noise utilities.
 * Replaces hand-rolled WorldNoise:: hash-based value noise with SIMD-accelerated
 * Simplex / OpenSimplex2 / Perlin noise from the FastNoise2 library.
 *
 * All functions are deterministic: same inputs always produce the same output.
 */
namespace FN2
{
	// ----------------------------------------------------------------
	// Single-sample convenience wrappers
	// ----------------------------------------------------------------

	/** 2D Simplex noise via FN2. Returns approximately [-1, 1]. */
	inline float Noise2D(float X, float Y, int32 Seed)
	{
		auto Generator = FastNoise::New<FastNoise::Simplex>();
		float Result = 0.0f;
		Generator->GenUniformGrid2D(&Result,
			static_cast<int>(FMath::FloorToInt(X)),
			static_cast<int>(FMath::FloorToInt(Y)),
			1, 1, 1.0f, Seed);
		return Result;
	}

	/** 3D Simplex noise via FN2. Returns approximately [-1, 1]. */
	inline float Noise3D(float X, float Y, float Z, int32 Seed)
	{
		auto Generator = FastNoise::New<FastNoise::Simplex>();
		float Result = 0.0f;
		Generator->GenUniformGrid3D(&Result,
			static_cast<int>(FMath::FloorToInt(X)),
			static_cast<int>(FMath::FloorToInt(Y)),
			static_cast<int>(FMath::FloorToInt(Z)),
			1, 1, 1, 1.0f, Seed);
		return Result;
	}

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
		auto WarpSource = FastNoise::New<FastNoise::Simplex>();
		auto DomainWarp = FastNoise::New<FastNoise::DomainWarpGradient>();
		DomainWarp->SetSource(WarpSource);
		DomainWarp->SetWarpAmplitude(WarpAmplitude);

		auto WarpedFractal = FastNoise::New<FastNoise::DomainWarpFractalProgressive>();
		WarpedFractal->SetSource(FBM);
		WarpedFractal->SetDomainWarpSource(DomainWarp);
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
			StartX, StartY,
			SizeX, SizeY,
			Frequency, Seed);
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
			StartX, StartY, StartZ,
			SizeX, SizeY, SizeZ,
			Frequency, Seed);
	}

	// ----------------------------------------------------------------
	// Drop-in replacements for WorldNoise:: functions
	// ----------------------------------------------------------------

	/** FBM via FN2 — drop-in replacement for WorldNoise::FBM.
	 *  Returns approximately [-1, 1]. */
	inline float FBM(float X, float Y, int32 Octaves, float Persistence, int32 Seed)
	{
		auto Generator = MakeFBM(Octaves, Persistence);
		float Result = 0.0f;
		Generator->GenPositionArray2D(
			&Result, 1, &X, &Y, 0.0f, 0.0f, Seed);
		return Result;
	}

	/** Ridged FBM via FN2 — drop-in replacement for WorldNoise::RidgedFBM.
	 *  Returns approximately [0, 1]. */
	inline float RidgedFBM(float X, float Y, int32 Octaves, float Persistence, float /*Sharpness*/, int32 Seed)
	{
		auto Generator = MakeRidged(Octaves, Persistence);
		float Result = 0.0f;
		Generator->GenPositionArray2D(
			&Result, 1, &X, &Y, 0.0f, 0.0f, Seed);
		// Remap from [-1,1] to [0,1]
		return (Result + 1.0f) * 0.5f;
	}

	/** 3D FBM via FN2 for volumetric noise. Returns approximately [-1, 1]. */
	inline float FBM3D(float X, float Y, float Z, int32 Octaves, float Persistence, int32 Seed)
	{
		auto Generator = MakeFBM(Octaves, Persistence);
		float Result = 0.0f;
		Generator->GenPositionArray3D(
			&Result, 1, &X, &Y, &Z, 0.0f, 0.0f, 0.0f, Seed);
		return Result;
	}

	/** 3D Ridged FBM via FN2. Returns approximately [0, 1]. */
	inline float RidgedFBM3D(float X, float Y, float Z, int32 Octaves, float Persistence, int32 Seed)
	{
		auto Generator = MakeRidged(Octaves, Persistence);
		float Result = 0.0f;
		Generator->GenPositionArray3D(
			&Result, 1, &X, &Y, &Z, 0.0f, 0.0f, 0.0f, Seed);
		return (Result + 1.0f) * 0.5f;
	}
}
