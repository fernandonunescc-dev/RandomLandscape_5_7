// BiomeMapGenerator.h
// Deterministic biome layout generator - single blob per biome, exact target matching
// Version: 01.28.2026.20.42

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include "BiomeMapGenerator.generated.h"

/**
 * Settings for a single biome layer.
 * Each biome is carved as a single contiguous blob.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FBiomeLayerSettings
{
	GENERATED_BODY()

	/** The type of biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome")
	EBiomeType BiomeType = EBiomeType::Desert;

	/** Display name for this biome (for UI/debugging) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome")
	FString DisplayName = TEXT("Biome");

	/** Target share of LAND pixels (0..100). All biomes are carved as single blobs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome", meta=(ClampMin="0.0", ClampMax="100.0"))
	float TargetPercentOfLand = 10.0f;

	/** The color used to represent this biome on the map texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome")
	FLinearColor Color = FLinearColor(0.5f, 0.5f, 0.5f, 1.0f);

	/** How the biome's percentage is distributed. Single = one contiguous area. Multi = disabled for now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome")
	ESpreadType SpreadType = ESpreadType::Single;

	/** Per-biome terrain/height generation settings for heightmap texture generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
	FBiomeTerrainSettings TerrainSettings;

	/** Growth noise frequency (higher = more jagged borders) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced", AdvancedDisplay, meta=(ClampMin="0.01", ClampMax="64.0"))
	float SpreadNoiseFrequency = 6.0f;

	/** Noise weight added to growth cost */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced", AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="10.0"))
	float SpreadNoiseAmplitude = 1.25f;

	/** Enable macro-field bias for natural zone shapes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced", AdvancedDisplay)
	bool bEnableMacroBias = true;

	/** Macro noise frequency (lower = larger coherent zones) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced", AdvancedDisplay, meta=(ClampMin="0.05", ClampMax="4.0", EditCondition="bEnableMacroBias"))
	float MacroFrequency = 0.6f;

	/** Macro bias weight (higher = stronger zoning) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Advanced", AdvancedDisplay, meta=(ClampMin="0.0", ClampMax="10.0", EditCondition="bEnableMacroBias"))
	float MacroWeight = 2.0f;

	FBiomeLayerSettings() = default;

	FBiomeLayerSettings(EBiomeType InType, const FString& InName, float InPercent, const FLinearColor& InColor)
		: BiomeType(InType)
		, DisplayName(InName)
		, TargetPercentOfLand(InPercent)
		, Color(InColor)
		, TerrainSettings(FBiomeTerrainSettings::DefaultForBiome(InType))
	{}
};

/**
 * Overall settings for biome layout generation.
 * All biomes (including Land) are carved as single contiguous blobs.
 * Remaining pixels after carving are assigned to the nearest biome.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FBiomeLayoutSettings
{
	GENERATED_BODY()

	/** 
	 * Seed for biome placement randomness.
	 * Changing this produces different biome layouts on the same landmass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Generation")
	int32 BiomeSeed = 1337;

	/** Texture resolution (width=height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Generation", meta=(ClampMin="64", ClampMax="4096"))
	int32 TextureResolution = 512;

	/** Color for ocean (non-land pixels) - Dark Blue */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Generation")
	FLinearColor OceanColor = FLinearColor(0.05f, 0.1f, 0.4f, 1.0f);

	/** 
	 * List of biomes to place on land.
	 * Each biome is carved as a single contiguous blob.
	 * Remaining pixels after carving are assigned to the nearest biome.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Generation")
	TArray<FBiomeLayerSettings> Layers;

	FBiomeLayoutSettings()
	{
		// Default biomes with required colors - even distribution (~14.29% each)
		const float EvenPercent = 100.0f / 7.0f;
		Layers.Add(FBiomeLayerSettings(EBiomeType::Land, TEXT("Land"), EvenPercent, FLinearColor(0.6f, 0.8f, 0.3f, 1.0f)));          // Light Green
		Layers.Add(FBiomeLayerSettings(EBiomeType::Forest, TEXT("Forest"), EvenPercent, FLinearColor(0.1f, 0.4f, 0.1f, 1.0f)));      // Dark Green
		Layers.Add(FBiomeLayerSettings(EBiomeType::Desert, TEXT("Desert"), EvenPercent, FLinearColor(0.95f, 0.85f, 0.3f, 1.0f)));    // Yellow
		Layers.Add(FBiomeLayerSettings(EBiomeType::Snow, TEXT("Snow"), EvenPercent, FLinearColor(0.95f, 0.95f, 1.0f, 1.0f)));        // White
		Layers.Add(FBiomeLayerSettings(EBiomeType::Ice, TEXT("Ice"), EvenPercent, FLinearColor(0.7f, 0.85f, 1.0f, 1.0f)));            // Light Blue
		Layers.Add(FBiomeLayerSettings(EBiomeType::Mountain, TEXT("Mountain"), EvenPercent, FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));  // Light Gray
		Layers.Add(FBiomeLayerSettings(EBiomeType::Volcanic, TEXT("Volcanic"), EvenPercent, FLinearColor(0.9f, 0.4f, 0.3f, 1.0f)));  // Light Red
	}

	/** Find layer settings by biome type (returns nullptr if not found) */
	const FBiomeLayerSettings* FindLayerByType(EBiomeType BiomeType) const
	{
		for (const FBiomeLayerSettings& Layer : Layers)
		{
			if (Layer.BiomeType == BiomeType)
			{
				return &Layer;
			}
		}
		return nullptr;
	}

	/** Get color for a biome type */
	FLinearColor GetBiomeColor(EBiomeType BiomeType) const
	{
		const FBiomeLayerSettings* Layer = FindLayerByType(BiomeType);
		return Layer ? Layer->Color : FLinearColor::White;
	}
};

/**
 * Generates a biome ID map ensuring:
 * - Each biome (including Land) forms exactly ONE contiguous blob
 * - Each biome gets exactly TargetCount pixels (largest-remainder allocation)
 * - Deterministic: same (LandMask + BiomeSeed + settings) => same output
 * 
 * Algorithm:
 * 1) Fill all land with UncarvedLandId (placeholder)
 * 2) For each biome (including Land): pick ONE seed, grow until TargetCount
 * 3) Assign any remaining uncarved land pixels to the nearest biome via BFS
 */
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class RANDOMLANDSCAPE_5_7_API UBiomeMapGenerator : public UObject
{
	GENERATED_BODY()

public:
	UBiomeMapGenerator();

	/** 
	 * All biome generation settings. 
	 * This is the single source of truth - edit these in the Details panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biome Generation")
	FBiomeLayoutSettings LayoutSettings;

	/** Initialize with current LayoutSettings (call before Generate) */
	void Initialize();

	/** Initialize with external settings (overrides LayoutSettings) */
	void Initialize(const FBiomeLayoutSettings& InSettings);

	/**
	 * Generates a biome ID map for the given LandMask.
	 * Output is per-pixel biome ID (stored as int32 of EBiomeType).
	 */
	bool Generate(const TArray<uint8>& LandMask);

	/** Per-pixel biome id map, indexed [Y*Res + X], stores (int32)EBiomeType */
	const TArray<int32>& GetBiomeMap() const { return BiomeMap; }

	/** BiomeSeed actually used */
	int32 GetBiomeSeed() const { return Settings.BiomeSeed; }

	/** Get current settings (after Initialize) */
	const FBiomeLayoutSettings& GetSettings() const { return Settings; }

	/** Land is the connector biome */
	static constexpr int32 ConnectorBiomeId = static_cast<int32>(EBiomeType::Land);

	/** Placeholder value for land pixels that haven't been assigned a biome yet */
	static constexpr int32 UncarvedLandId = -2;

private:
	FBiomeLayoutSettings Settings;
	mutable FRandomStream RandomStream; // initialized with BiomeSeed

	TArray<int32> BiomeMap;         // size Res*Res
	TArray<int32> LandIndices;      // indices of land pixels (1D index)

	// ---- Hardcoded algorithm defaults ----
	static constexpr bool bUseEightWayAdjacency = true;

	// ---- internal helpers ----
	void BuildLandIndexList(const TArray<uint8>& LandMask);
	void FillConnector();
	void NormalizeLayerPercentages();

	static FORCEINLINE int32 Idx(int32 X, int32 Y, int32 Res) { return Y * Res + X; }
	static FORCEINLINE void XY(int32 Index, int32 Res, int32& OutX, int32& OutY) { OutY = Index / Res; OutX = Index - OutY * Res; }

	// Cheap deterministic noise in [-1, 1]
	static float ValueNoise2D(float X, float Y, uint32 Seed);
	static float SmoothNoise2D(float X, float Y, float Frequency, uint32 Seed);

	// Pick a deterministic random seed from uncarved land
	int32 PickRandomConnectorIndex(const TArray<uint8>& LandMask, int32 BiomeId) const;

	// Sequential carve: grow each biome from uncarved land
	void CarveBiomesSequentially(const TArray<uint8>& LandMask);

	// Grow a single biome from one seed, stopping at TargetCount
	// Only paints on uncarved land cells
	// Uses hardcoded noise constants for organic borders
	int32 GrowSingleBiome(
		const TArray<uint8>& LandMask,
		int32 SeedIndex,
		int32 BiomeId,
		int32 TargetCount,
		uint32 NoiseSeed);

	// Assign remaining uncarved land pixels to the nearest carved biome via BFS
	void AssignUncarvedPixels();

	// Log final biome counts by scanning BiomeMap
	void LogFinalBiomeCounts(const TArray<uint8>& LandMask) const;

	// UnassignedId used for ocean pixels
	static constexpr int32 UnassignedId = -1;
};
