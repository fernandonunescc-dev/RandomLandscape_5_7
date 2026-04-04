#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.h"
#include "UpliftGenerator.generated.h"

/**
 * Stage 2: Uplift / Geology Control Map Generator
 *
 * Produces three elevation layers from the binary land mask:
 *   1. BaseElevation   – coastline distance gradient modulated by FBM noise
 *   2. UpliftMap        – tectonic mountain ridges (ridged FBM) + volcanic hotspots
 *   3. CombinedElevation – sum of base + uplift, clamped to [0,1]
 *
 * All output arrays are Resolution*Resolution in size and normalized to [0,1].
 * Ocean pixels are always 0.
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UUpliftGenerator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store settings, derive the actual seed, and record the texture resolution.
	 *
	 * @param InSettings       Uplift configuration (noise frequencies, mountain params, etc.)
	 * @param GlobalSeed       Pipeline-wide seed used when Settings.Seed == 0
	 * @param TextureResolution Width/height of the square output maps in pixels
	 */
	void Initialize(const FUpliftSettings& InSettings, int32 GlobalSeed, int32 TextureResolution);

	/**
	 * Run the full uplift generation pipeline.
	 *
	 * Algorithm:
	 *   1. GenerateBaseElevation   – coastline distance + FBM
	 *   2. GenerateUpliftMap       – ridged mountain ranges
	 *   3. GenerateVolcanicHotspots – cone/crater features
	 *   4. CombineElevation        – merge all layers
	 *
	 * @param LandMask  Binary mask (1 = land, 0 = ocean) from Stage 1
	 * @return true on success
	 */
	bool Generate(const TArray<uint8>& LandMask);

	/** Base elevation layer (coastline gradient * noise), range [0,1]. */
	const TArray<float>& GetBaseElevation() const { return BaseElevation; }

	/** Tectonic uplift layer (mountain ridges + volcanic), range [0,1]. */
	const TArray<float>& GetUpliftMap() const { return UpliftMap; }

	/** Final combined elevation (base + uplift, clamped), range [0,1]. */
	const TArray<float>& GetCombinedElevation() const { return CombinedElevation; }

	/** Plateau mask per pixel [0,1] — 1.0 means fully plateau. */
	const TArray<float>& GetPlateauMap() const { return PlateauMap; }

	/** Normalized positions of volcanic hotspot centers. */
	const TArray<FVector2D>& GetVolcanicCenters() const { return VolcanicCenters; }

	/** The actual seed used for generation (derived or explicit). */
	int32 GetSeed() const { return ActualSeed; }

private:

	//--------------------------------------------------------------------------
	// Configuration
	//--------------------------------------------------------------------------
	FUpliftSettings Settings;
	int32 ActualSeed = 0;
	int32 Resolution = 0;

	//--------------------------------------------------------------------------
	// Output buffers (Resolution * Resolution elements each)
	//--------------------------------------------------------------------------

	/** Per-pixel distance (in pixels) from the nearest ocean cell. */
	TArray<float> CoastlineDistance;

	/** Coastline distance + noise, 0-1. */
	TArray<float> BaseElevation;

	/** Tectonic uplift zones, 0-1. */
	TArray<float> UpliftMap;

	/** Base + uplift, 0-1. */
	TArray<float> CombinedElevation;

	/** Plateau mask, 0-1 (1 = fully plateau). */
	TArray<float> PlateauMap;

	/** Normalized positions of volcanic centers. */
	TArray<FVector2D> VolcanicCenters;

	//--------------------------------------------------------------------------
	// Internal helpers
	//--------------------------------------------------------------------------

	/**
	 * BFS from ocean pixels outward.
	 * Ocean pixels get distance 0; land pixels receive the shortest distance
	 * in pixels to the nearest ocean cell.
	 *
	 * @param LandMask      Binary mask (1 = land)
	 * @param OutDistances   Output array sized Resolution*Resolution
	 */
	void ComputeCoastlineDistance(const TArray<uint8>& LandMask, TArray<float>& OutDistances);

	/** Compute BaseElevation from coastline distance modulated by FBM noise. */
	void GenerateBaseElevation(const TArray<uint8>& LandMask);

	/** Compute ridged-FBM mountain ranges into UpliftMap. */
	void GenerateUpliftMap(const TArray<uint8>& LandMask);

	/** Place volcanic hotspot cones (with craters) and add them to UpliftMap. */
	void GenerateVolcanicHotspots(const TArray<uint8>& LandMask);

	/** Compute rolling hills via FBM and add to UpliftMap. */
	void GenerateHills(const TArray<uint8>& LandMask);

	/** Compute plateau mask and flatten qualifying terrain. */
	void GeneratePlateaus(const TArray<uint8>& LandMask);

	/** Merge BaseElevation and UpliftMap into CombinedElevation. */
	void CombineElevation();
};
