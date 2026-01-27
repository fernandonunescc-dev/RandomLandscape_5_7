// MapTypes.h
// Defines common types and enums for the map generation system

#pragma once

#include "CoreMinimal.h"
#include "MapTypes.generated.h"

/** Conversion factor: 1 meter = 100 Unreal Units (centimeters) */
constexpr float MetersToUnrealUnits = 100.0f;
constexpr float UnrealUnitsToMeters = 0.01f;

/**
 * Defines the type of map to generate
 */
UENUM(BlueprintType)
enum class EMapType : uint8
{
	/** Large connected landmass with possible inland features */
	Continent UMETA(DisplayName = "Continent"),
	
	/** Multiple smaller islands scattered across water */
	Archipelago UMETA(DisplayName = "Archipelago")
};

/**
 * Configuration settings for map generation
 * Note: MapSizeInMeters is the length/width of the square map. Height is determined per-biome.
 */
USTRUCT(BlueprintType)
struct FMapGenerationSettings
{
	GENERATED_BODY()

	/** The type of map to generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation")
	EMapType MapType = EMapType::Continent;

	/** The length and width of the map in meters (square map) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "10", ClampMax = "10000"))
	int32 MapSizeInMeters = 100;

	/** 
	 * Resolution scale (1-100) where 100 is maximum vertices/triangles per chunk.
	 * Higher values = more detail but more performance cost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MapResolution = 50;

	/** Get the map size converted to Unreal Units (centimeters) - returns X and Y dimensions */
	float GetMapSizeInUnrealUnits() const
	{
		return static_cast<float>(MapSizeInMeters) * MetersToUnrealUnits;
	}

	/** Set the map size from Unreal Units (centimeters) */
	void SetMapSizeFromUnrealUnits(float SizeInUnrealUnits)
	{
		MapSizeInMeters = FMath::RoundToInt(SizeInUnrealUnits * UnrealUnitsToMeters);
	}
};
