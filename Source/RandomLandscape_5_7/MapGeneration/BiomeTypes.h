// BiomeTypes.h
// Defines biome types, map configuration, and per-biome terrain settings for map generation

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.generated.h"

class UTexture2D;

/**
 * Defines the map generation type.
 * Controls overall land/ocean distribution pattern.
 */
UENUM(BlueprintType)
enum class EMapType : uint8
{
	Continent UMETA(DisplayName = "Continent"),     // Large landmass, small ocean
	Island UMETA(DisplayName = "Island"),            // Small landmass, large ocean
	Archipelago UMETA(DisplayName = "Archipelago")   // Multiple islands
};

/**
 * Defines the map physical size.
 * Controls the world-space dimensions of the generated mesh.
 */
UENUM(BlueprintType)
enum class EMapSize : uint8
{
	Large UMETA(DisplayName = "Large (2x2 km)"),     // 2x2 kilometers
	Medium UMETA(DisplayName = "Medium (1x1 km)"),   // 1x1 kilometers
	Small UMETA(DisplayName = "Small (500x500 m)")    // 500x500 meters
};

/**
 * Defines how a biome's percentage is distributed across the map.
 */
UENUM(BlueprintType)
enum class ESpreadType : uint8
{
	Single UMETA(DisplayName = "Single"),                          // Single contiguous area
	// Multi is reserved for future implementation (split percentage between several blobs)
	Multi UMETA(DisplayName = "Multi (Disabled)", Hidden)          // Multiple blobs (not yet implemented)
};

/**
 * Defines the available biome types.
 */
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
	Ocean UMETA(DisplayName = "Ocean"),
	Land UMETA(DisplayName = "Land"),
	Forest UMETA(DisplayName = "Forest"),
	Desert UMETA(DisplayName = "Desert"),
	Snow UMETA(DisplayName = "Snow"),
	Ice UMETA(DisplayName = "Ice"),
	Mountain UMETA(DisplayName = "Mountain"),
	Volcanic UMETA(DisplayName = "Volcanic")
};

/**
 * Per-biome terrain/height generation settings.
 * Controls how heightmap noise is generated for each biome type.
 * Fields are organized by biome type; irrelevant fields are ignored by the generator.
 */
USTRUCT(BlueprintType)
struct FBiomeTerrainSettings
{
	GENERATED_BODY()

	// === Common Noise Settings (all biomes) ===

	/** Base noise frequency for terrain features */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float NoiseFrequency = 1.0f;

	/** Number of noise octaves for fractal detail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	/** Persistence for fractal noise (lower = smoother) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NoisePersistence = 0.5f;

	/**
	 * Maximum height this biome can reach, relative to MaxMapHeight (0.0 - 1.0).
	 * Mountain/Volcanic = high (can reach map peak). Land/Ice = low (mostly flat).
	 * This directly controls the brightness range of the heightmap texture.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeightScale = 0.3f;

	// === Flatness Controls (Land, Desert, Ice) ===

	/** How flat the terrain is (0 = very hilly, 1 = perfectly flat). Used by Land, Desert, Ice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flatness", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Flatness = 0.5f;

	// === Hill Controls (Land, Forest, Snow) ===

	/** Maximum hill height as fraction of HeightScale. Used by Land, Forest, Snow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hills", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HillHeight = 0.3f;

	/** Frequency of hills. Higher = more frequent hills. Used by Land, Forest, Snow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hills", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float HillFrequency = 2.0f;

	// === Dune Controls (Desert) ===

	/** Height of dunes relative to HeightScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dunes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DuneHeight = 0.4f;

	/** Frequency of dune ridges. Higher = more frequent dunes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dunes", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float DuneFrequency = 6.0f;

	/** Sharpness of dune ridges. Higher = sharper crests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dunes", meta = (ClampMin = "1.0", ClampMax = "5.0"))
	float DuneRidgeSharpness = 2.0f;

	// === Iceberg Controls (Ice) ===

	/** Height of icebergs/ice mountains relative to HeightScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icebergs", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IcebergHeight = 0.5f;

	/** Frequency of iceberg features. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icebergs", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float IcebergFrequency = 1.5f;

	// === Mountain Controls ===

	/** Maximum number of mountain peaks that may fit in this biome area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MountainCount = 3;

	/** Peak height as fraction of HeightScale (1.0 = full height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MountainPeakHeight = 0.9f;

	/** Sharpness of mountain peaks. Higher = sharper peaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float PeakSharpness = 2.0f;

	// === Volcanic Controls ===

	/** Volcano cone height as fraction of HeightScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float VolcanoHeight = 0.85f;

	/** Crater radius as fraction of the volcano (0 = no crater, 1 = crater fills volcano). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float CraterRadius = 0.3f;

	/** Depth of the crater as fraction of VolcanoHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcano", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CraterDepth = 0.4f;

	FBiomeTerrainSettings() {}

	/** Create default terrain settings for a given biome type */
	static FBiomeTerrainSettings DefaultForBiome(EBiomeType BiomeType)
	{
		FBiomeTerrainSettings S;
		switch (BiomeType)
		{
		case EBiomeType::Land:
			S.NoiseFrequency = 1.0f;
			S.NoiseOctaves = 3;
			S.NoisePersistence = 0.3f;
			S.HeightScale = 0.08f;
			S.Flatness = 0.85f;
			S.HillHeight = 0.2f;
			S.HillFrequency = 2.0f;
			break;
		case EBiomeType::Forest:
			S.NoiseFrequency = 1.5f;
			S.NoiseOctaves = 4;
			S.NoisePersistence = 0.45f;
			S.HeightScale = 0.15f;
			S.Flatness = 0.4f;
			S.HillHeight = 0.5f;
			S.HillFrequency = 3.0f;
			break;
		case EBiomeType::Desert:
			S.NoiseFrequency = 1.0f;
			S.NoiseOctaves = 3;
			S.NoisePersistence = 0.3f;
			S.HeightScale = 0.1f;
			S.Flatness = 0.8f;
			S.DuneHeight = 0.4f;
			S.DuneFrequency = 6.0f;
			S.DuneRidgeSharpness = 2.0f;
			break;
		case EBiomeType::Snow:
			S.NoiseFrequency = 1.5f;
			S.NoiseOctaves = 4;
			S.NoisePersistence = 0.45f;
			S.HeightScale = 0.15f;
			S.Flatness = 0.4f;
			S.HillHeight = 0.5f;
			S.HillFrequency = 3.0f;
			break;
		case EBiomeType::Ice:
			S.NoiseFrequency = 0.8f;
			S.NoiseOctaves = 2;
			S.NoisePersistence = 0.25f;
			S.HeightScale = 0.05f;
			S.Flatness = 0.95f;
			S.IcebergHeight = 0.5f;
			S.IcebergFrequency = 1.5f;
			break;
		case EBiomeType::Mountain:
			S.NoiseFrequency = 2.0f;
			S.NoiseOctaves = 5;
			S.NoisePersistence = 0.55f;
			S.HeightScale = 1.0f;
			S.MountainCount = 3;
			S.MountainPeakHeight = 0.9f;
			S.PeakSharpness = 2.0f;
			break;
		case EBiomeType::Volcanic:
			S.NoiseFrequency = 1.5f;
			S.NoiseOctaves = 4;
			S.NoisePersistence = 0.5f;
			S.HeightScale = 0.9f;
			S.VolcanoHeight = 0.85f;
			S.CraterRadius = 0.3f;
			S.CraterDepth = 0.4f;
			break;
		default: // Ocean
			S.NoiseFrequency = 0.5f;
			S.NoiseOctaves = 3;
			S.NoisePersistence = 0.4f;
			S.HeightScale = 0.0f;
			break;
		}
		return S;
	}
};

/**
 * Noise generation settings for a biome
 */
USTRUCT(BlueprintType)
struct FBiomeMeshSettings
{
	GENERATED_BODY()

	/** Display name for identification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	FString DisplayName = TEXT("Biome");

	/** The biome type this settings belongs to */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "General")
	EBiomeType BiomeType = EBiomeType::Land;

	/** Seed for this biome's noise generation. 0 = random. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	int32 Seed = 0;

	/** Noise frequency (higher = more frequent features) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float NoiseFrequency = 0.02f;

	/** Number of noise octaves for fractal detail (more = finer detail) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "1", ClampMax = "8"))
	int32 NoiseOctaves = 4;

	/** Persistence for fractal noise (how much each octave contributes, lower = smoother) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NoisePersistence = 0.5f;

	/** 
	 * Visual representation of this biome's position (512x512).
	 * Shows biome color where this biome exists, black elsewhere.
	 * Used for debugging and material overlays.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated")
	TObjectPtr<UTexture2D> MaskTexture = nullptr;

	/** 
	 * High-resolution noise texture for this biome (4096x4096 by default).
	 * Raw noise values mapped to grayscale: -1 -> black, 0 -> gray, 1 -> white.
	 * Resolution controlled by HeightMapResolution in ProceduralMapActor.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated")
	TObjectPtr<UTexture2D> HighResTexture = nullptr;


	FBiomeMeshSettings() {}

	FBiomeMeshSettings(EBiomeType InType, const FString& InName, 
		float InNoiseFreq = 2.0f, int32 InOctaves = 4, float InPersistence = 0.5f)
		: DisplayName(InName)
		, BiomeType(InType)
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
	EBiomeType BiomeType = EBiomeType::Land;

	/** Display name for this biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	FString DisplayName = TEXT("Land");

	/** 
	 * Target percentage of land this biome should cover (0-100).
	 * Note: Ocean is calculated separately as it surrounds the continent.
	 * Land biomes should add up to 100%.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General", meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
	float Percentage = 20.0f;

	/** The color used to represent this biome on the map texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Texture")
	FLinearColor Color = FLinearColor(0.6f, 0.8f, 0.3f, 1.0f);

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

	/** Ocean color (surrounds the continent) - Dark Blue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor OceanColor = FLinearColor(0.05f, 0.1f, 0.4f, 1.0f);

	/** 
	 * Land biome configurations.
	 * Percentages should add up to 100% for proper distribution.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBiomeConfig> LandBiomes;

	FContinentBiomeSettings()
	{
		// Initialize with default biomes using required colors - even distribution
		const float EvenPercent = 100.0f / 7.0f;
		LandBiomes.Add(FBiomeConfig(EBiomeType::Land, TEXT("Land"), EvenPercent, FLinearColor(0.6f, 0.8f, 0.3f, 1.0f)));         // Light Green
		LandBiomes.Add(FBiomeConfig(EBiomeType::Forest, TEXT("Forest"), EvenPercent, FLinearColor(0.1f, 0.4f, 0.1f, 1.0f)));     // Dark Green
		LandBiomes.Add(FBiomeConfig(EBiomeType::Desert, TEXT("Desert"), EvenPercent, FLinearColor(0.95f, 0.85f, 0.3f, 1.0f)));   // Yellow
		LandBiomes.Add(FBiomeConfig(EBiomeType::Snow, TEXT("Snow"), EvenPercent, FLinearColor(0.95f, 0.95f, 1.0f, 1.0f)));       // White
		LandBiomes.Add(FBiomeConfig(EBiomeType::Ice, TEXT("Ice"), EvenPercent, FLinearColor(0.7f, 0.85f, 1.0f, 1.0f)));           // Light Blue
		LandBiomes.Add(FBiomeConfig(EBiomeType::Mountain, TEXT("Mountain"), EvenPercent, FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))); // Light Gray
		LandBiomes.Add(FBiomeConfig(EBiomeType::Volcanic, TEXT("Volcanic"), EvenPercent, FLinearColor(0.9f, 0.4f, 0.3f, 1.0f))); // Light Red
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

	/** Find biome config by type (returns nullptr if not found) */
	const FBiomeConfig* FindBiomeConfig(EBiomeType BiomeType) const
	{
		for (const FBiomeConfig& Config : LandBiomes)
		{
			if (Config.BiomeType == BiomeType)
			{
				return &Config;
			}
		}
		return nullptr;
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
		// Initialize ocean with default noise settings
		OceanSettings = FBiomeMeshSettings(EBiomeType::Ocean, TEXT("Ocean"), 1.0f, 3, 0.4f);

		// Initialize land biome noise settings with defaults
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Land, TEXT("Land"), 0.3f, 3, 0.25f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Forest, TEXT("Forest"), 0.5f, 4, 0.3f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Desert, TEXT("Desert"), 0.8f, 3, 0.25f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Snow, TEXT("Snow"), 1.2f, 4, 0.45f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Ice, TEXT("Ice"), 0.3f, 2, 0.2f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Mountain, TEXT("Mountain"), 1.5f, 5, 0.55f));
		BiomeMeshSettings.Add(FBiomeMeshSettings(EBiomeType::Volcanic, TEXT("Volcanic"), 2.0f, 5, 0.6f));
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
