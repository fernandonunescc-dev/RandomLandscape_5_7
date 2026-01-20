// Copyright Fernando Araujo. All Rights Reserved.

#include "LandscapeNoiseService.h"

// FastNoise2 includes
THIRD_PARTY_INCLUDES_START
#include "FastNoise/FastNoise.h"
THIRD_PARTY_INCLUDES_END

// ============================================================================
// Constructor / Destructor
// ============================================================================

FLandscapeNoiseService::FLandscapeNoiseService()
{
}

FLandscapeNoiseService::~FLandscapeNoiseService()
{
	CleanupNoiseNode();
}

// ============================================================================
// Initialization
// ============================================================================

void FLandscapeNoiseService::Initialize(int32 Seed)
{
	NoiseSeed = Seed;
	CleanupNoiseNode();
	InitializeNoiseNode();
}

void FLandscapeNoiseService::InitializeNoiseNode()
{
	// Create a fractal FBm noise using Simplex as the base
	auto Simplex = FastNoise::New<FastNoise::Simplex>();
	auto FractalFBm = FastNoise::New<FastNoise::FractalFBm>();
	FractalFBm->SetSource(Simplex);
	FractalFBm->SetOctaveCount(NoiseOctaves);
	FractalFBm->SetLacunarity(NoiseLacunarity);
	FractalFBm->SetGain(NoiseGain);
	
	TerrainNoiseNode = new FastNoise::SmartNode<FastNoise::Generator>(FractalFBm);
}

void FLandscapeNoiseService::CleanupNoiseNode()
{
	if (TerrainNoiseNode)
	{
		delete reinterpret_cast<FastNoise::SmartNode<FastNoise::Generator>*>(TerrainNoiseNode);
		TerrainNoiseNode = nullptr;
	}
}

// ============================================================================
// Noise Sampling
// ============================================================================

// Simple hash-based noise function as fallback
static float SimpleNoise2D(float X, float Y, int32 Seed)
{
	auto Hash = [](int32 x, int32 y, int32 seed) -> float
	{
		int32 n = x + y * 57 + seed * 131;
		n = (n << 13) ^ n;
		float value = (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
		return value;
	};

	int32 xi = FMath::FloorToInt(X);
	int32 yi = FMath::FloorToInt(Y);
	
	float xf = X - xi;
	float yf = Y - yi;
	
	// Smooth interpolation
	float u = xf * xf * (3.0f - 2.0f * xf);
	float v = yf * yf * (3.0f - 2.0f * yf);
	
	// Bilinear interpolation
	float n00 = Hash(xi, yi, Seed);
	float n10 = Hash(xi + 1, yi, Seed);
	float n01 = Hash(xi, yi + 1, Seed);
	float n11 = Hash(xi + 1, yi + 1, Seed);
	
	float nx0 = FMath::Lerp(n00, n10, u);
	float nx1 = FMath::Lerp(n01, n11, u);
	
	return FMath::Lerp(nx0, nx1, v);
}

// Fractal noise using multiple octaves
static float FractalNoise2D(float X, float Y, int32 Seed, int32 Octaves, float Lacunarity, float Gain)
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float MaxAmplitude = 0.0f;
	float Frequency = 1.0f;
	
	for (int32 i = 0; i < Octaves; ++i)
	{
		Total += SimpleNoise2D(X * Frequency, Y * Frequency, Seed + i * 1000) * Amplitude;
		MaxAmplitude += Amplitude;
		Amplitude *= Gain;
		Frequency *= Lacunarity;
	}
	
	return Total / MaxAmplitude;  // Returns -1 to 1
}

float FLandscapeNoiseService::SampleNoise(float X, float Y) const
{
	// Apply frequency scaling
	float ScaledX = X * NoiseFrequency;
	float ScaledY = Y * NoiseFrequency;
	
	// Use simple fractal noise (FastNoise2 integration can be added later)
	return FractalNoise2D(ScaledX, ScaledY, NoiseSeed, NoiseOctaves, NoiseLacunarity, NoiseGain);
}

// ============================================================================
// Height Map Generation
// ============================================================================

void FLandscapeNoiseService::GenerateHeightMapRegion(
	float StartX, float StartY,
	float SizeMeters,
	int32 Resolution,
	TArray<float>& OutHeights) const
{
	const int32 TotalVertices = Resolution * Resolution;
	OutHeights.SetNumUninitialized(TotalVertices);
	
	const float Step = SizeMeters / (Resolution - 1);
	const float HeightRange = MaxHeight - MinHeight;
	
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const float WorldX = StartX + X * Step;
			const float WorldY = StartY + Y * Step;
			
			// Get noise value (-1 to 1)
			float NoiseValue = SampleNoise(WorldX, WorldY);
			
			// Remap from [-1, 1] to [0, 1]
			float NormalizedHeight = (NoiseValue + 1.0f) * 0.5f;
			
			// Apply height exponent to make tall peaks rare
			// pow(x, exponent) where exponent > 1 makes high values rarer
			// Example: with exponent 2.0, a noise value of 0.7 becomes 0.49
			// This compresses most terrain to lower heights while preserving rare peaks
			if (HeightExponent != 1.0f)
			{
				NormalizedHeight = FMath::Pow(NormalizedHeight, HeightExponent);
			}
			
			// Scale to height range
			float Height = MinHeight + NormalizedHeight * HeightRange;
			
			OutHeights[Y * Resolution + X] = Height;
		}
	}
}
