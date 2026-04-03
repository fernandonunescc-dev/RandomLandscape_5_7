// NoiseUtility.h
// Shared deterministic noise functions for the world generation pipeline.
// Now backed by FastNoise2 (SIMD-accelerated Simplex) for 2D and 3D noise,
// while preserving the same WorldNoise:: API for existing callers.

#pragma once

#include "CoreMinimal.h"
#include "FastNoise2Noise.h"

/**
 * Shared noise utilities used across all pipeline generators.
 * Deterministic: same inputs always produce the same output.
 *
 * Noise2D, FBM, and RidgedFBM now delegate to FastNoise2 for SIMD performance.
 * Hash2D and DeriveSeed remain unchanged (pure integer math).
 *
 * For new code that needs 3D noise, use the FN2:: namespace directly
 * (see FastNoise2Noise.h).
 */
namespace WorldNoise
{
	/** Fast integer hash → float in [-1, 1].
	 *  Kept as-is for non-noise uses (seeding, random placement, etc.). */
	FORCEINLINE float Hash2D(int32 X, int32 Y, int32 Seed)
	{
		uint32 N = static_cast<uint32>(X) + static_cast<uint32>(Y) * 57u + static_cast<uint32>(Seed);
		N = (N << 13) ^ N;
		N = N * (N * N * 15731u + 789221u) + 1376312589u;
		return 1.0f - static_cast<float>(N & 0x7FFFFFFFu) / 1073741824.0f;
	}

	/** 2D Simplex noise via FastNoise2. Returns approximately [-1, 1].
	 *  Drop-in replacement for the old hand-rolled value noise. */
	inline float Noise2D(float X, float Y, int32 Seed)
	{
		return FN2::Noise2D(X, Y, Seed);
	}

	/** Fractal Brownian Motion via FastNoise2. Returns approximately [-1, 1]. */
	inline float FBM(float X, float Y, int32 Octaves, float Persistence, int32 Seed)
	{
		return FN2::FBM(X, Y, Octaves, Persistence, Seed);
	}

	/** Ridged FBM via FastNoise2. Returns [0, 1]. */
	inline float RidgedFBM(float X, float Y, int32 Octaves, float Persistence, float Sharpness, int32 Seed)
	{
		return FN2::RidgedFBM(X, Y, Octaves, Persistence, Sharpness, Seed);
	}

	// ==================== 3D Noise (new) ====================

	/** 3D Simplex noise via FastNoise2. Returns approximately [-1, 1]. */
	inline float Noise3D(float X, float Y, float Z, int32 Seed)
	{
		return FN2::Noise3D(X, Y, Z, Seed);
	}

	/** 3D FBM via FastNoise2. Returns approximately [-1, 1]. */
	inline float FBM3D(float X, float Y, float Z, int32 Octaves, float Persistence, int32 Seed)
	{
		return FN2::FBM3D(X, Y, Z, Octaves, Persistence, Seed);
	}

	/** 3D Ridged FBM via FastNoise2. Returns approximately [0, 1]. */
	inline float RidgedFBM3D(float X, float Y, float Z, int32 Octaves, float Persistence, int32 Seed)
	{
		return FN2::RidgedFBM3D(X, Y, Z, Octaves, Persistence, Seed);
	}

	// ==================== Utility (unchanged) ====================

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
