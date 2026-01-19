// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Enumeration for different terrain biome types
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
	Ocean,		// Height < -500cm
	Lake,		// Height -500 to 0cm
	Plains,		// Height 0 to 3000cm
	Cliffs,		// Height 3000 to 5000cm
	Mountains,	// Height 5000 to 10000cm
	Peaks,		// Height > 10000cm
	None		// Unknown/unclassified
};

/**
 * Structure containing height range and properties for each biome type
 */
struct FBiomeDefinition
{
	EBiomeType BiomeType;
	float MinHeight;	// cm (in Unreal units)
	float MaxHeight;	// cm (in Unreal units)
	FName BiomeName;
	FColor DebugColor;	// For visualization
};

/**
 * Manages biome classification based on height values
 */
class FBiomeClassifier
{
public:
	FBiomeClassifier();

	/**
	 * Get the biome type for a given height value
	 * @param InHeight Height value in Unreal units (cm)
	 * @return Biome type for that height
	 */
	EBiomeType GetBiomeAtHeight(float InHeight) const;

	/**
	 * Get biome definition struct
	 */
	FBiomeDefinition GetBiomeDefinition(EBiomeType InBiome) const;

	/**
	 * Initialize default biome definitions
	 */
	void InitializeDefaultBiomes();

private:
	// Map of biome types to their definitions
	TMap<EBiomeType, FBiomeDefinition> BiomeDefinitions;
};
