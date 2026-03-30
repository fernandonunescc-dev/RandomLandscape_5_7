// NoiseUtility.h
// Shared deterministic noise functions for the world generation pipeline.
// All functions are static/inline so this is a header-only utility.

#pragma once

#include "CoreMinimal.h"

/**
 * Shared noise utilities used across all pipeline generators.
 * Deterministic: same inputs always produce the same output.
 */
namespace WorldNoise
{
	/** Fast integer hash → float in [-1, 1] */
	FORCEINLINE float Hash2D(int32 X, int32 Y, int32 Seed)
	{
		uint32 N = static_cast<uint32>(X) + static_cast<uint32>(Y) * 57u + static_cast<uint32>(Seed);
		N = (N << 13) ^ N;
		N = N * (N * N * 15731u + 789221u) + 1376312589u;
		return 1.0f - static_cast<float>(N & 0x7FFFFFFFu) / 1073741824.0f;
	}

	/** 2D value noise with smooth interpolation. Returns [-1, 1]. */
	inline float Noise2D(float X, float Y, int32 Seed)
	{
		const float OffsetX = (Seed % 10000) * 0.37f;
		const float OffsetY = (Seed % 10000) * 0.53f;
		X += OffsetX;
		Y += OffsetY;

		const int32 Xi = FMath::FloorToInt(X);
		const int32 Yi = FMath::FloorToInt(Y);
		const float Xf = X - Xi;
		const float Yf = Y - Yi;

		// Smoothstep interpolation
		const float U = Xf * Xf * (3.0f - 2.0f * Xf);
		const float V = Yf * Yf * (3.0f - 2.0f * Yf);

		const float A = Hash2D(Xi, Yi, Seed);
		const float B = Hash2D(Xi + 1, Yi, Seed);
		const float C = Hash2D(Xi, Yi + 1, Seed);
		const float D = Hash2D(Xi + 1, Yi + 1, Seed);

		return FMath::Lerp(
			FMath::Lerp(A, B, U),
			FMath::Lerp(C, D, U),
			V);
	}

	/** Fractal Brownian Motion. Returns approximately [-1, 1]. */
	inline float FBM(float X, float Y, int32 Octaves, float Persistence, int32 Seed)
	{
		float Total = 0.0f;
		float Amplitude = 1.0f;
		float Frequency = 1.0f;
		float MaxValue = 0.0f;

		for (int32 i = 0; i < Octaves; ++i)
		{
			Total += Noise2D(X * Frequency, Y * Frequency, Seed + i * 31) * Amplitude;
			MaxValue += Amplitude;
			Amplitude *= Persistence;
			Frequency *= 2.0f;
		}

		return (MaxValue > 0.0f) ? (Total / MaxValue) : 0.0f;
	}

	/** Ridged FBM — produces ridge-like mountain structures. Returns [0, 1]. */
	inline float RidgedFBM(float X, float Y, int32 Octaves, float Persistence, float Sharpness, int32 Seed)
	{
		float Total = 0.0f;
		float Amplitude = 1.0f;
		float Frequency = 1.0f;
		float MaxValue = 0.0f;

		for (int32 i = 0; i < Octaves; ++i)
		{
			float N = Noise2D(X * Frequency, Y * Frequency, Seed + i * 31);
			N = 1.0f - FMath::Abs(N);  // Fold into ridge
			N = FMath::Pow(N, Sharpness);
			Total += N * Amplitude;
			MaxValue += Amplitude;
			Amplitude *= Persistence;
			Frequency *= 2.0f;
		}

		return (MaxValue > 0.0f) ? (Total / MaxValue) : 0.0f;
	}

	/** Compute a deterministic seed offset from a base seed and stage index */
	FORCEINLINE int32 DeriveSeed(int32 BaseSeed, int32 StageIndex)
	{
		uint32 H = static_cast<uint32>(BaseSeed) ^ (static_cast<uint32>(StageIndex) * 2654435761u);
		H ^= H >> 16;
		H *= 0x7FEB352Du;
		H ^= H >> 15;
		return static_cast<int32>(H & 0x7FFFFFFF);
	}

	/** D8 neighbor offsets: E, NE, N, NW, W, SW, S, SE */
	inline constexpr int32 DX8[] = {  1,  1,  0, -1, -1, -1,  0,  1 };
	inline constexpr int32 DY8[] = {  0, -1, -1, -1,  0,  1,  1,  1 };

	/** D8 neighbor distances (1 for cardinal, sqrt(2) for diagonal) */
	inline constexpr float Dist8[] = { 1.f, 1.41421356f, 1.f, 1.41421356f, 1.f, 1.41421356f, 1.f, 1.41421356f };
}
