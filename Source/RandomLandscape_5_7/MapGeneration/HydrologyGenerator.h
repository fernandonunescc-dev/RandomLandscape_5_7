#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.h"
#include "HydrologyGenerator.generated.h"

/**
 * Stage 3: Hydrology — rivers and lakes from drainage simulation.
 *
 * Produces four output layers from the combined elevation and land mask:
 *   1. FlowDirection    – D8 direction index (0-7) per pixel, -1 = no downhill neighbor
 *   2. FlowAccumulation – upstream drainage area per pixel
 *   3. RiverMap         – normalized river strength [0,1] (wider rivers = higher)
 *   4. LakeMap          – binary lake mask (1 = lake, 0 = not)
 *
 * All output arrays are Resolution*Resolution in size.
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UHydrologyGenerator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store settings, derive the actual seed, and record the texture resolution.
	 *
	 * @param InSettings       Hydrology configuration (thresholds, jitter, etc.)
	 * @param GlobalSeed       Pipeline-wide seed used when Settings.Seed == 0
	 * @param TextureResolution Width/height of the square output maps in pixels
	 */
	void Initialize(const FHydrologySettings& InSettings, int32 GlobalSeed, int32 TextureResolution);

	/**
	 * Run the full hydrology generation pipeline.
	 *
	 * Algorithm:
	 *   1. ComputeFlowDirections  – D8 steepest-descent with jitter
	 *   2. ComputeFlowAccumulation – topological sort high→low
	 *   3. IdentifyRivers         – threshold + log-normalized strength
	 *   4. IdentifyLakes          – local-minima detection + BFS expansion
	 *
	 * @param Elevation  Combined elevation map [0,1] from Stage 2
	 * @param LandMask   Binary mask (1 = land, 0 = ocean) from Stage 1
	 * @return true on success
	 */
	bool Generate(const TArray<float>& Elevation, const TArray<uint8>& LandMask);

	/** River strength per pixel, range [0,1]. Wider rivers = higher value. */
	const TArray<float>& GetRiverMap() const { return RiverMap; }

	/** Binary lake mask (1 = lake, 0 = not). */
	const TArray<uint8>& GetLakeMap() const { return LakeMap; }

	/** Upstream drainage area per pixel. */
	const TArray<float>& GetFlowAccumulation() const { return FlowAccumulation; }

	/** D8 direction index (0-7) per pixel, -1 = no downhill neighbor (local minimum). */
	const TArray<int32>& GetFlowDirection() const { return FlowDirection; }

	/** The actual seed used for generation (derived or explicit). */
	int32 GetSeed() const { return ActualSeed; }

private:

	//--------------------------------------------------------------------------
	// Configuration
	//--------------------------------------------------------------------------
	FHydrologySettings Settings;
	int32 ActualSeed = 0;
	int32 Resolution = 0;

	//--------------------------------------------------------------------------
	// Output buffers (Resolution * Resolution elements each)
	//--------------------------------------------------------------------------

	/** D8 flow direction per pixel (0-7), -1 = local minimum. */
	TArray<int32> FlowDirection;

	/** Upstream drainage area per pixel. */
	TArray<float> FlowAccumulation;

	/** Normalized river strength, 0-1. */
	TArray<float> RiverMap;

	/** Binary lake mask. */
	TArray<uint8> LakeMap;

	//--------------------------------------------------------------------------
	// Internal helpers
	//--------------------------------------------------------------------------

	/**
	 * D8 steepest-descent algorithm with noise jitter.
	 * For each land pixel, find the 8-connected neighbor with the steepest
	 * downhill slope. Small jitter prevents perfectly straight rivers.
	 */
	void ComputeFlowDirections(const TArray<float>& Elevation, const TArray<uint8>& LandMask);

	/** Topological accumulation: sort land pixels high→low, pass flow downstream. */
	void ComputeFlowAccumulation();

	/** Threshold flow accumulation and log-normalize to produce river strength. */
	void IdentifyRivers();

	/** Mark local minima with sufficient accumulation as lakes, then BFS expand. */
	void IdentifyLakes(const TArray<float>& Elevation, const TArray<uint8>& LandMask);
};
