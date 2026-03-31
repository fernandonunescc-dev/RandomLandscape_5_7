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

/**
 * Terrain archetypes — describe the fundamental shape of the terrain.
 * These are structural categories determined by elevation, slope, and
 * geological features (plateau mask, canyon mask), independent of climate
 * or surface cover.  Lakes, rivers, and waterfalls are NOT archetypes —
 * they are generated features tracked separately.
 */
UENUM(BlueprintType)
enum class ETerrainArchetype : uint8
{
	Ocean    UMETA(DisplayName = "Ocean"),
	Plains   UMETA(DisplayName = "Plains"),
	Hills    UMETA(DisplayName = "Hills"),
	Desert   UMETA(DisplayName = "Desert"),
	Mountains UMETA(DisplayName = "Mountains"),
	Plateaus UMETA(DisplayName = "Plateaus"),
	Canyons  UMETA(DisplayName = "Canyons")
};

/**
 * Generated water/hydrological features — layered on top of terrain
 * archetypes.  These are outputs of the hydrology simulation, not primary
 * terrain types.
 */
UENUM(BlueprintType)
enum class EGeneratedFeature : uint8
{
	None      UMETA(DisplayName = "None"),
	River     UMETA(DisplayName = "River"),
	Lake      UMETA(DisplayName = "Lake"),
	Waterfall UMETA(DisplayName = "Waterfall")
};

/**
 * Surface / content overlays — describe what covers the terrain surface.
 * Determined by climate (temperature, moisture, precipitation) and proximity
 * to water features.  A single terrain archetype can host many different
 * surface overlays depending on local climate conditions.
 */
UENUM(BlueprintType)
enum class ESurfaceOverlay : uint8
{
	None        UMETA(DisplayName = "None"),
	Forest      UMETA(DisplayName = "Forest"),
	Grassland   UMETA(DisplayName = "Grassland"),
	Snow        UMETA(DisplayName = "Snow"),
	Wetlands    UMETA(DisplayName = "Wetlands"),
	DesertScrub UMETA(DisplayName = "Desert Scrub")
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

/** Get the canonical debug color for a terrain archetype */
inline FLinearColor GetArchetypeDebugColor(ETerrainArchetype Type)
{
	switch (Type)
	{
	case ETerrainArchetype::Ocean:     return FLinearColor(0.05f, 0.10f, 0.40f, 1.0f);
	case ETerrainArchetype::Plains:    return FLinearColor(0.60f, 0.80f, 0.30f, 1.0f);
	case ETerrainArchetype::Hills:     return FLinearColor(0.45f, 0.65f, 0.25f, 1.0f);
	case ETerrainArchetype::Desert:    return FLinearColor(0.95f, 0.85f, 0.30f, 1.0f);
	case ETerrainArchetype::Mountains: return FLinearColor(0.70f, 0.70f, 0.70f, 1.0f);
	case ETerrainArchetype::Plateaus:  return FLinearColor(0.80f, 0.65f, 0.45f, 1.0f);
	case ETerrainArchetype::Canyons:   return FLinearColor(0.55f, 0.30f, 0.15f, 1.0f);
	default:                           return FLinearColor::White;
	}
}

/** Get the canonical debug color for a surface overlay */
inline FLinearColor GetSurfaceOverlayDebugColor(ESurfaceOverlay Type)
{
	switch (Type)
	{
	case ESurfaceOverlay::None:        return FLinearColor(0.50f, 0.50f, 0.50f, 1.0f);
	case ESurfaceOverlay::Forest:      return FLinearColor(0.10f, 0.40f, 0.10f, 1.0f);
	case ESurfaceOverlay::Grassland:   return FLinearColor(0.55f, 0.75f, 0.30f, 1.0f);
	case ESurfaceOverlay::Snow:        return FLinearColor(0.95f, 0.95f, 1.00f, 1.0f);
	case ESurfaceOverlay::Wetlands:    return FLinearColor(0.20f, 0.45f, 0.35f, 1.0f);
	case ESurfaceOverlay::DesertScrub: return FLinearColor(0.75f, 0.70f, 0.40f, 1.0f);
	default:                           return FLinearColor::White;
	}
}
