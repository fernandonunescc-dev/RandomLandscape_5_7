// BiomeMapGenerator.h
// Deterministic biome layout generator - single blob per biome, exact target matching
// Version: 01.27.2026.23.51

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include "BiomeMapGenerator.generated.h"

/**
 * Settings for a single biome layer to be carved from connector land.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FBiomeLayerSettings
{
	GENERATED_BODY()

	/** Which biome this layer paints */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes")
	EBiomeType BiomeType = EBiomeType::Desert;

	/** Target share of LAND pixels this biome should occupy (0..100). Forest gets the remainder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes", meta=(ClampMin="0.0", ClampMax="100.0"))
	float TargetPercentOfLand = 10.0f;

	/** Growth noise: higher frequency = more jagged, lower = smoother */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes", meta=(ClampMin="0.01", ClampMax="64.0"))
	float SpreadNoiseFrequency = 6.0f;

	/** Noise weight added to growth cost (higher = more irregular borders) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes", meta=(ClampMin="0.0", ClampMax="10.0"))
	float SpreadNoiseAmplitude = 1.25f;
};

/**
 * Overall settings for biome layout generation.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FBiomeLayoutSettings
{
	GENERATED_BODY()

	/** 
	 * Seed for biome placement randomness.
	 * Changing this produces different biome layouts on the same landmass.
	 * Independent from landmass generation seed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes")
	int32 BiomeSeed = 1337;

	/** Texture resolution (width=height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes", meta=(ClampMin="64", ClampMax="4096"))
	int32 TextureResolution = 512;

	/** Connector biome that fills all land first (the "always passable" biome) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes")
	EBiomeType ConnectorBiomeType = EBiomeType::Forest;

	/** List of non-connector biomes to carve into the connector biome (one blob each) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes")
	TArray<FBiomeLayerSettings> Layers;

	/** 4-way keeps chunkier shapes, 8-way gives rounder/diagonal growth */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes")
	bool bUseEightWayAdjacency = true;

	/** 
	 * When multiple components can fit a biome, choose randomly among them.
	 * If false, always picks the largest fitting component (less variety).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes")
	bool bRandomizeComponentChoice = true;

	/** 
	 * When randomly choosing among fitting components, weight by size.
	 * If true: larger components more likely to be chosen.
	 * If false: uniform random among all fitting components.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Biomes")
	bool bWeightComponentChoiceBySize = true;
};

/**
 * Represents a connected component of connector land.
 * Contains both deterministic min-index (for tiebreaks) and random index (for seed placement).
 */
struct FConnectorComponent
{
	int32 ComponentId = -1;
	int32 Size = 0;
	int32 RepresentativeIndexMin = INT32_MAX;    // minimum pixel index (for deterministic tiebreak)
	int32 RepresentativeIndexRandom = -1;        // random pixel index via reservoir sampling (for seed placement)
};

/**
 * Generates a biome ID map ensuring:
 * - Each non-connector biome forms exactly ONE contiguous blob
 * - Each biome matches its TargetCount exactly (largest-remainder allocation)
 * - Deterministic: same (LandMask + BiomeSeed + settings) => same output
 * - Changing BiomeSeed changes biome placements while keeping same landmass
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UBiomeMapGenerator : public UObject
{
	GENERATED_BODY()

public:
	UBiomeMapGenerator();

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

private:
	FBiomeLayoutSettings Settings;
	mutable FRandomStream RandomStream; // initialized with BiomeSeed

	TArray<int32> BiomeMap;         // size Res*Res
	TArray<int32> LandIndices;      // indices of land pixels (1D index)
	
	// Reusable buffer for component IDs per pixel (avoids reallocation)
	mutable TArray<int32> CompIdPerPixelBuffer;

	// ---- internal helpers ----
	void BuildLandIndexList(const TArray<uint8>& LandMask);
	void FillConnector();

	// Compute connected components of remaining connector land (uses bUseEightWayAdjacency)
	// Also computes random representative via reservoir sampling during BFS
	void ComputeConnectorComponents(
		const TArray<uint8>& LandMask,
		int32 ConnectorId,
		TArray<int32>& OutCompIdPerPixel,
		TArray<FConnectorComponent>& OutComponents) const;

	// Choose best component for a biome with given TargetCount
	// BiomeId and CarveOrder used to derive deterministic random seed for component choice
	int32 ChooseComponentForBiome(
		const TArray<FConnectorComponent>& Components,
		int32 TargetCount,
		int32 BiomeId,
		int32 CarveOrder) const;


	static FORCEINLINE int32 Idx(int32 X, int32 Y, int32 Res) { return Y * Res + X; }
	static FORCEINLINE void XY(int32 Index, int32 Res, int32& OutX, int32& OutY) { OutY = Index / Res; OutX = Index - OutY * Res; }

	// Cheap deterministic noise in [-1, 1]
	static float ValueNoise2D(float X, float Y, uint32 Seed);
	static float SmoothNoise2D(float X, float Y, float Frequency, uint32 Seed);

	// Sequential carve: grow one biome at a time
	void CarveBiomesSequentially(const TArray<uint8>& LandMask);

	// Grow a single biome from one seed, stopping at TargetCount
	int32 GrowSingleBiome(
		const TArray<uint8>& LandMask,
		int32 SeedIndex,
		int32 BiomeId,
		int32 TargetCount,
		int32 ConnectorId,
		float NoiseFreq,
		float NoiseAmp,
		uint32 NoiseSeed);
};
