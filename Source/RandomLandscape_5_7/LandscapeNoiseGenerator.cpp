// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeNoiseGenerator.h"
#include "Math/UnrealMathUtility.h"

FLandscapeNoiseGenerator::FLandscapeNoiseGenerator()
{
	// Default values already set in header
}

bool FLandscapeNoiseGenerator::GenerateHeightMap(int32 InCellX, int32 InCellY, float InMapSize, float InGridScale, 
	TArray<float>& OutHeightMap, int32 VertexCount)
{
	OutHeightMap.Empty();
	OutHeightMap.Reserve(VertexCount);

	// Calculate cell bounds in world space
	float CellSizeUnreal = InGridScale * 100.0f;	// Convert meters to Unreal units
	float OriginX = (InMapSize * 100.0f / 2.0f) + (InCellX * CellSizeUnreal);
	float OriginY = (InMapSize * 100.0f / 2.0f) + (InCellY * CellSizeUnreal);

	// Calculate vertex spacing
	int32 GridResolution = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(VertexCount)));
	float VertexSpacing = CellSizeUnreal / (GridResolution - 1);

	// Generate height for each vertex in this cell
	for (int32 Y = 0; Y < GridResolution; ++Y)
	{
		for (int32 X = 0; X < GridResolution; ++X)
		{
			float WorldX = OriginX + (X * VertexSpacing);
			float WorldY = OriginY + (Y * VertexSpacing);

			float Height = BlendNoiseLayers(WorldX, WorldY);
			OutHeightMap.Add(Height);
		}
	}

	return OutHeightMap.Num() == VertexCount;
}

float FLandscapeNoiseGenerator::GetHeightAtWorldPosition(float InWorldX, float InWorldY)
{
	return BlendNoiseLayers(InWorldX, InWorldY);
}

float FLandscapeNoiseGenerator::GetNoise(float InX, float InY, float InFrequency, int32 InOctaves)
{
	// TODO: Integrate with FastNoise2 library
	// For now, using a simple Perlin-like noise approximation
	
	float Result = 0.0f;
	float Amplitude = 1.0f;
	float MaxAmplitude = 0.0f;
	float Scale = InFrequency;

	for (int32 i = 0; i < InOctaves; ++i)
	{
		// Simple sine-based pseudo-noise (placeholder for FastNoise2)
		float SampleX = InX * Scale + NoiseSeed;
		float SampleY = InY * Scale + NoiseSeed * 2;
		
		float Noise = FMath::Sin(SampleX * 12.9898f + SampleY * 78.233f) * 43758.5453f;
		Noise = FMath::Frac(Noise);
		Noise = Noise * 2.0f - 1.0f;	// Convert to -1 to 1

		Result += Noise * Amplitude;
		MaxAmplitude += Amplitude;

		Amplitude *= 0.5f;
		Scale *= 2.0f;
	}

	return MaxAmplitude > 0.0f ? Result / MaxAmplitude : 0.0f;
}

float FLandscapeNoiseGenerator::BlendNoiseLayers(float InX, float InY)
{
	float Primary = GetNoise(InX, InY, PrimaryNoiseFrequency, PrimaryNoiseOctaves);
	float Secondary = GetNoise(InX, InY, SecondaryNoiseFrequency, SecondaryNoiseOctaves);
	float Tertiary = GetNoise(InX, InY, TertiaryNoiseFrequency, TertiaryNoiseOctaves);

	float Blended = (Primary * PrimaryWeight) + (Secondary * SecondaryWeight) + (Tertiary * TertiaryWeight);
	
	return FMath::Clamp(Blended, -1.0f, 1.0f);
}
