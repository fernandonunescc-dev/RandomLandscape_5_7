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
 * Note: MapSizeInMeters is exposed to the user, MapSizeInUnrealUnits is used internally
 */
USTRUCT(BlueprintType)
struct FMapGenerationSettings
{
	GENERATED_BODY()

	/** The type of map to generate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation")
	EMapType MapType = EMapType::Continent;

	/** The size of the map in meters (X, Y, Z for width, depth, height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "1.0"))
	FVector MapSizeInMeters = FVector(100.0f, 100.0f, 10.0f);

	/** 
	 * Resolution scale (1-100) where 100 is maximum vertices/triangles per chunk.
	 * Higher values = more detail but more performance cost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MapResolution = 50;

	/** Get the map size converted to Unreal Units (centimeters) */
	FVector GetMapSizeInUnrealUnits() const
	{
		return MapSizeInMeters * MetersToUnrealUnits;
	}

	/** Set the map size from Unreal Units (centimeters) */
	void SetMapSizeFromUnrealUnits(const FVector& SizeInUnrealUnits)
	{
		MapSizeInMeters = SizeInUnrealUnits * UnrealUnitsToMeters;
	}
};
