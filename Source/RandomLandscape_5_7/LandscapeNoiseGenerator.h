// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"

/**
 * Handles procedural noise generation for terrain using multi-layered Perlin/Simplex noise
 * Supports different noise layers for base terrain, variation, and detail
 */
class FLandscapeNoiseGenerator
{
public:
	FLandscapeNoiseGenerator();

	/**
	 * Generate a height map for a grid cell
	 * @param InCellX Grid cell X coordinate
	 * @param InCellY Grid cell Y coordinate
	 * @param InMapSize Size of the landscape in meters
	 * @param InGridScale Size of each cell in meters
	 * @param OutHeightMap Output array of height values (-1.0 to 1.0)
	 * @return true if generation succeeded
	 */
	bool GenerateHeightMap(int32 InCellX, int32 InCellY, float InMapSize, float InGridScale, 
		TArray<float>& OutHeightMap, int32 VertexCount = 1024);

	/**
	 * Get a single height value at world coordinates
	 * @param InWorldX X coordinate in world space (Unreal units)
	 * @param InWorldY Y coordinate in world space (Unreal units)
	 * @return Height value (-1.0 to 1.0)
	 */
	float GetHeightAtWorldPosition(float InWorldX, float InWorldY);

	// Noise configuration properties
	float PrimaryNoiseFrequency = 0.001f;
	int32 PrimaryNoiseOctaves = 3;
	float SecondaryNoiseFrequency = 0.01f;
	int32 SecondaryNoiseOctaves = 4;
	float TertiaryNoiseFrequency = 0.05f;
	int32 TertiaryNoiseOctaves = 2;

	// Layer weights (sum should ideally be 1.0)
	float PrimaryWeight = 0.5f;
	float SecondaryWeight = 0.35f;
	float TertiaryWeight = 0.15f;

private:
	// Seed for consistent generation
	int32 NoiseSeed = 1337;

	/**
	 * Generate Perlin-style noise using FastNoise2
	 * @param InX X coordinate
	 * @param InY Y coordinate
	 * @param InFrequency Frequency/scale of noise
	 * @param InOctaves Number of octaves to layer
	 * @return Noise value (-1.0 to 1.0)
	 */
	float GetNoise(float InX, float InY, float InFrequency, int32 InOctaves);

	/**
	 * Blend multiple noise layers together
	 */
	float BlendNoiseLayers(float InX, float InY);
};
