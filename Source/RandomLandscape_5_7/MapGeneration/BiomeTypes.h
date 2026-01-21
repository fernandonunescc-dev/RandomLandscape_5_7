// BiomeTypes.h
// Defines biome types and configuration for map generation

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.generated.h"

/**
 * Defines the available biome types
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
	Ocean UMETA(DisplayName = "Ocean"),
	Forest UMETA(DisplayName = "Forest"),
	Mountain UMETA(DisplayName = "Mountain"),
	Desert UMETA(DisplayName = "Desert"),
	Snow UMETA(DisplayName = "Snow"),
	Volcanic UMETA(DisplayName = "Volcanic")
};

/**
 * Configuration for a single biome
 */
USTRUCT(BlueprintType)
struct FBiomeConfig
{
	GENERATED_BODY()

	/** The type of biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	EBiomeType BiomeType = EBiomeType::Forest;

	/** Display name for this biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	FString DisplayName = TEXT("Forest");

	/** 
	 * Target percentage of land this biome should cover (0-100).
	 * Note: Ocean is calculated separately as it surrounds the continent.
	 * Land biomes should add up to 100%.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float Percentage = 20.0f;

	/** The color used to represent this biome on the map texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	FLinearColor Color = FLinearColor::Green;

	FBiomeConfig() {}

	FBiomeConfig(EBiomeType InType, const FString& InName, float InPercentage, const FLinearColor& InColor)
		: BiomeType(InType)
		, DisplayName(InName)
		, Percentage(InPercentage)
		, Color(InColor)
	{}
};

/**
 * Contains all biome configurations for continent generation
 */
USTRUCT(BlueprintType)
struct FContinentBiomeSettings
{
	GENERATED_BODY()

	/** Ocean color (surrounds the continent) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	FLinearColor OceanColor = FLinearColor(0.4f, 0.7f, 0.9f, 1.0f); // Light blue

	/** 
	 * Land biome configurations.
	 * Percentages should add up to 100% for proper distribution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
	TArray<FBiomeConfig> LandBiomes;

	FContinentBiomeSettings()
	{
		// Initialize with default biomes matching the reference image
		LandBiomes.Add(FBiomeConfig(EBiomeType::Forest, TEXT("Forest"), 35.0f, FLinearColor(0.2f, 0.6f, 0.2f, 1.0f)));      // Green
		LandBiomes.Add(FBiomeConfig(EBiomeType::Mountain, TEXT("Mountain"), 20.0f, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));  // Gray
		LandBiomes.Add(FBiomeConfig(EBiomeType::Desert, TEXT("Desert"), 20.0f, FLinearColor(0.95f, 0.85f, 0.3f, 1.0f)));    // Yellow
		LandBiomes.Add(FBiomeConfig(EBiomeType::Snow, TEXT("Snow"), 15.0f, FLinearColor(0.3f, 0.7f, 0.9f, 1.0f)));          // Light Blue
		LandBiomes.Add(FBiomeConfig(EBiomeType::Volcanic, TEXT("Volcanic"), 10.0f, FLinearColor(0.9f, 0.4f, 0.1f, 1.0f)));  // Orange
	}

	/** Get total percentage of all land biomes */
	float GetTotalPercentage() const
	{
		float Total = 0.0f;
		for (const FBiomeConfig& Biome : LandBiomes)
		{
			Total += Biome.Percentage;
		}
		return Total;
	}

	/** Check if percentages are valid (add up to ~100%) */
	bool ArePercentagesValid(float Tolerance = 1.0f) const
	{
		float Total = GetTotalPercentage();
		return FMath::Abs(Total - 100.0f) <= Tolerance;
	}

	/** Normalize percentages to add up to exactly 100% */
	void NormalizePercentages()
	{
		float Total = GetTotalPercentage();
		if (Total > 0.0f && !FMath::IsNearlyEqual(Total, 100.0f))
		{
			float Scale = 100.0f / Total;
			for (FBiomeConfig& Biome : LandBiomes)
			{
				Biome.Percentage *= Scale;
			}
		}
	}
};
