// BiomeTypes.h
// Defines biome types and configuration for map generation

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.generated.h"

class UTexture2D;

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
 * Mesh generation settings for a biome or ocean
 */
USTRUCT(BlueprintType)
struct FBiomeMeshSettings
{
	GENERATED_BODY()

	/** Display name for identification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	FString DisplayName = TEXT("Biome");

	/** The biome type this mesh settings belongs to */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	EBiomeType BiomeType = EBiomeType::Forest;

	/** Seed for this biome's terrain generation. 0 = random. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	int32 Seed = 0;

	/** 
	 * Maximum height for this biome in meters.
	 * This is the absolute max height the terrain can reach.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "-500.0", ClampMax = "500.0"))
	float MaxHeightInMeters = 10.0f;

	/** 
	 * Minimum height for this biome in meters.
	 * The terrain will vary between MinHeight and MaxHeight.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "-500.0", ClampMax = "500.0"))
	float MinHeightInMeters = 0.0f;

	/** Noise frequency for terrain height (higher = more frequent hills/features) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float NoiseFrequency = 2.0f;

	/** Number of noise octaves for fractal detail (more = finer detail) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	/** Persistence for fractal noise (how much each octave contributes, lower = smoother) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NoisePersistence = 0.5f;

	/** 
	 * Generated mask texture for this biome.
	 * Shows biome color where this biome exists, black elsewhere.
	 * Used for mesh generation.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated")
	TObjectPtr<UTexture2D> MaskTexture = nullptr;

	/** 
	 * Generated high-resolution height map texture for this biome.
	 * Grayscale noise map where white = max height, black = min height.
	 * Generated directly from FastNoise2 at HeightMapResolution.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated")
	TObjectPtr<UTexture2D> HighResTexture = nullptr;


	FBiomeMeshSettings() {}

	FBiomeMeshSettings(EBiomeType InType, const FString& InName, 
		float InMaxHeight = 10.0f, float InMinHeight = 0.0f, 
		float InNoiseFreq = 2.0f, int32 InOctaves = 4, float InPersistence = 0.5f)
		: DisplayName(InName)
		, BiomeType(InType)
		, MaxHeightInMeters(InMaxHeight)
		, MinHeightInMeters(InMinHeight)
		, NoiseFrequency(InNoiseFreq)
		, NoiseOctaves(InOctaves)
		, NoisePersistence(InPersistence)
	{}
};

/**
 * Configuration for a single biome (texture/distribution settings only)
 */
USTRUCT(BlueprintType)
struct FBiomeConfig
{
	GENERATED_BODY()

	/** The type of biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	EBiomeType BiomeType = EBiomeType::Forest;

	/** Display name for this biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	FString DisplayName = TEXT("Forest");

	/** 
	 * Target percentage of land this biome should cover (0-100).
	 * Note: Ocean is calculated separately as it surrounds the continent.
	 * Land biomes should add up to 100%.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float Percentage = 20.0f;

	/** The color used to represent this biome on the map texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
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
 * Contains all biome configurations for continent generation (texture/distribution only)
 */
USTRUCT(BlueprintType)
struct FContinentBiomeSettings
{
	GENERATED_BODY()

	/** 
	 * Percentage of the total map area that should be land (vs ocean).
	 * 50% means half land, half ocean. Higher values = more land.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10.0", ClampMax = "90.0", UIMin = "10.0", UIMax = "90.0"))
	float LandCoveragePercent = 50.0f;

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
		// Initialize with default biomes (texture settings only)
		LandBiomes.Add(FBiomeConfig(EBiomeType::Forest, TEXT("Forest"), 35.0f, FLinearColor(0.2f, 0.6f, 0.2f, 1.0f)));
		LandBiomes.Add(FBiomeConfig(EBiomeType::Mountain, TEXT("Mountain"), 20.0f, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
		LandBiomes.Add(FBiomeConfig(EBiomeType::Desert, TEXT("Desert"), 20.0f, FLinearColor(0.95f, 0.85f, 0.3f, 1.0f)));
		LandBiomes.Add(FBiomeConfig(EBiomeType::Snow, TEXT("Snow"), 15.0f, FLinearColor(0.9f, 0.95f, 1.0f, 1.0f)));
		LandBiomes.Add(FBiomeConfig(EBiomeType::Volcanic, TEXT("Volcanic"), 10.0f, FLinearColor(0.9f, 0.4f, 0.1f, 1.0f)));
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

/**
 * Contains mesh generation settings for all biomes including ocean
 */
USTRUCT(BlueprintType)
struct FMeshGenerationSettings
{
	GENERATED_BODY()

	/** Ocean mesh settings (underwater terrain) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ocean")
	FBiomeMeshSettings OceanSettings;

	/** Mesh settings for each land biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Land Biomes")
	TArray<FBiomeMeshSettings> BiomeMeshSettings;

	FMeshGenerationSettings()
	{
		// Initialize ocean with default underwater settings
		OceanSettings = FBiomeMeshSettings(EBiomeType::Ocean, TEXT("Ocean"), -5.0f, -50.0f, 1.0f, 3, 0.4f);

		// Initialize land biome mesh settings with defaults
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Forest, TEXT("Forest"), 4.0f, 0.5f, 0.5f, 4, 0.3f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Mountain, TEXT("Mountain"), 25.0f, 5.0f, 1.5f, 5, 0.55f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Desert, TEXT("Desert"), 2.0f, 0.0f, 0.8f, 3, 0.25f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Snow, TEXT("Snow"), 15.0f, 3.0f, 1.2f, 4, 0.45f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Volcanic, TEXT("Volcanic"), 20.0f, 2.0f, 2.0f, 5, 0.6f));
	}

	/** Get mesh settings for a specific biome type */
	const FBiomeMeshSettings* GetSettingsForBiome(EBiomeType BiomeType) const
	{
		if (BiomeType == EBiomeType::Ocean)
		{
			return &OceanSettings;
		}
		for (const FBiomeMeshSettings& Settings : BiomeMeshSettings)
		{
			if (Settings.BiomeType == BiomeType)
			{
				return &Settings;
			}
		}
		return nullptr;
	}

	/** Get mutable mesh settings for a specific biome type */
	FBiomeMeshSettings* GetSettingsForBiomeMutable(EBiomeType BiomeType)
	{
		if (BiomeType == EBiomeType::Ocean)
		{
			return &OceanSettings;
		}
		for (FBiomeMeshSettings& Settings : BiomeMeshSettings)
		{
			if (Settings.BiomeType == BiomeType)
			{
				return &Settings;
			}
		}
		return nullptr;
	}
};
