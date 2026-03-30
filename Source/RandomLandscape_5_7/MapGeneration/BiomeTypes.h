// BiomeTypes.h
// Core enumerations for the geology-driven world generation pipeline.
// Biomes are derived from climate + terrain, not placed randomly.

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.generated.h"

/**
 * Map generation type — controls overall land/ocean distribution pattern.
 */
UENUM(BlueprintType)
enum class EMapType : uint8
{
	Continent   UMETA(DisplayName = "Continent"),
	Island      UMETA(DisplayName = "Island"),
	Archipelago UMETA(DisplayName = "Archipelago")
};

/**
 * Map physical size in world space.
 */
UENUM(BlueprintType)
enum class EMapSize : uint8
{
	Large  UMETA(DisplayName = "Large (2x2 km)"),
	Medium UMETA(DisplayName = "Medium (1x1 km)"),
	Small  UMETA(DisplayName = "Small (500x500 m)")
};

/**
 * Biome types derived from climate, elevation, and geology.
 * Assigned by the climate/terrain classification stage — NOT placed randomly.
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
	Ocean    UMETA(DisplayName = "Ocean"),
	Land     UMETA(DisplayName = "Grassland"),
	Forest   UMETA(DisplayName = "Forest"),
	Desert   UMETA(DisplayName = "Desert"),
	Snow     UMETA(DisplayName = "Snow/Tundra"),
	Ice      UMETA(DisplayName = "Ice/Glacier"),
	Mountain UMETA(DisplayName = "Mountain"),
	Volcanic UMETA(DisplayName = "Volcanic")
};

/** Get the canonical debug color for a biome type */
inline FLinearColor GetBiomeDebugColor(EBiomeType Type)
{
	switch (Type)
	{
	case EBiomeType::Ocean:    return FLinearColor(0.05f, 0.10f, 0.40f, 1.0f);
	case EBiomeType::Land:     return FLinearColor(0.60f, 0.80f, 0.30f, 1.0f);
	case EBiomeType::Forest:   return FLinearColor(0.10f, 0.40f, 0.10f, 1.0f);
	case EBiomeType::Desert:   return FLinearColor(0.95f, 0.85f, 0.30f, 1.0f);
	case EBiomeType::Snow:     return FLinearColor(0.95f, 0.95f, 1.00f, 1.0f);
	case EBiomeType::Ice:      return FLinearColor(0.70f, 0.85f, 1.00f, 1.0f);
	case EBiomeType::Mountain: return FLinearColor(0.70f, 0.70f, 0.70f, 1.0f);
	case EBiomeType::Volcanic: return FLinearColor(0.90f, 0.40f, 0.30f, 1.0f);
	default:                   return FLinearColor::White;
	}
}
