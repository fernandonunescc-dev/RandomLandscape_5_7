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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|General")
	EBiomeType BiomeType = EBiomeType::Forest;

	/** Display name for this biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|General")
	FString DisplayName = TEXT("Forest");

	/** 
	 * Target percentage of land this biome should cover (0-100).
	 * Note: Ocean is calculated separately as it surrounds the continent.
	 * Land biomes should add up to 100%.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|General", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float Percentage = 20.0f;

	/** The color used to represent this biome on the map texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|General")
	FLinearColor Color = FLinearColor::Green;

	// ==================== Terrain Settings ====================

	/** Noise frequency for terrain height (higher = more hills) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Terrain", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float NoiseFrequency = 2.0f;

	/** Number of noise octaves for fractal detail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Terrain", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	/** Persistence for fractal noise (how much each octave contributes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Terrain", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NoisePersistence = 0.5f;

	/** Height multiplier for this biome (1.0 = full height from MapSize.Z) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Terrain", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float HeightMultiplier = 1.0f;

	/** Base height offset for this biome (0-1, added before noise) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseHeight = 0.0f;

	FBiomeConfig() {}

	FBiomeConfig(EBiomeType InType, const FString& InName, float InPercentage, const FLinearColor& InColor,
		float InNoiseFreq = 2.0f, int32 InOctaves = 4, float InPersistence = 0.5f, float InHeightMult = 1.0f, float InBaseHeight = 0.0f)
		: BiomeType(InType)
		, DisplayName(InName)
		, Percentage(InPercentage)
		, Color(InColor)
		, NoiseFrequency(InNoiseFreq)
		, NoiseOctaves(InOctaves)
		, NoisePersistence(InPersistence)
		, HeightMultiplier(InHeightMult)
		, BaseHeight(InBaseHeight)
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor OceanColor = FLinearColor(0.4f, 0.7f, 0.9f, 1.0f); // Light blue

	/** 
	 * Land biome configurations.
	 * Percentages should add up to 100% for proper distribution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBiomeConfig> LandBiomes;

	FContinentBiomeSettings()
	{
		// Initialize with default biomes matching the reference image
		// Parameters: Type, Name, Percentage, Color, NoiseFreq, Octaves, Persistence, HeightMult, BaseHeight
		
		// Forest: Rolling hills, medium height
		LandBiomes.Add(FBiomeConfig(EBiomeType::Forest, TEXT("Forest"), 35.0f, 
			FLinearColor(0.2f, 0.6f, 0.2f, 1.0f), 2.0f, 4, 0.5f, 0.6f, 0.1f));
		
		// Mountain: Tall, jagged peaks
		LandBiomes.Add(FBiomeConfig(EBiomeType::Mountain, TEXT("Mountain"), 20.0f, 
			FLinearColor(0.5f, 0.5f, 0.5f, 1.0f), 3.0f, 5, 0.6f, 1.5f, 0.3f));
		
		// Desert: Flat with gentle dunes
		LandBiomes.Add(FBiomeConfig(EBiomeType::Desert, TEXT("Desert"), 20.0f, 
			FLinearColor(0.95f, 0.85f, 0.3f, 1.0f), 1.5f, 3, 0.4f, 0.3f, 0.05f));
		
		// Snow: Higher elevation, moderate terrain
		LandBiomes.Add(FBiomeConfig(EBiomeType::Snow, TEXT("Snow"), 15.0f, 
			FLinearColor(0.3f, 0.7f, 0.9f, 1.0f), 2.5f, 4, 0.5f, 1.0f, 0.4f));
		
		// Volcanic: Dramatic, steep terrain
		LandBiomes.Add(FBiomeConfig(EBiomeType::Volcanic, TEXT("Volcanic"), 10.0f, 
			FLinearColor(0.9f, 0.4f, 0.1f, 1.0f), 4.0f, 5, 0.7f, 1.2f, 0.2f));
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
