#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.h"
#include "ErosionGenerator.generated.h"

/**
 * Stage 4: Erosion — river incision (canyon formation) and thermal erosion.
 *
 * Takes the combined elevation from Stage 2, plus the river and uplift maps
 * from Stage 3, and carves canyons along river paths while smoothing steep
 * slopes via thermal weathering.
 *
 * Outputs:
 *   - ErodedElevation — modified elevation map [0,1]
 *   - CanyonMask      — binary mask (1 = canyon pixel, 0 = not)
 *   - ErosionDeltaMap  — signed elevation change per pixel (pre − post)
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UErosionGenerator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store settings, derive the actual seed, and record the texture resolution.
	 *
	 * @param InSettings        Erosion configuration (incision strength, thermal passes, etc.)
	 * @param GlobalSeed        Pipeline-wide seed used when Settings.Seed == 0
	 * @param TextureResolution Width/height of the square output map in pixels
	 */
	void Initialize(const FErosionSettings& InSettings, int32 GlobalSeed, int32 TextureResolution);

	/**
	 * Run the full erosion pipeline.
	 *
	 * Algorithm:
	 *   1. Copy InputElevation into the output buffer
	 *   2. ApplyRiverIncision  — carve canyons along river paths (with width expansion)
	 *   3. ApplyThermalErosion — smooth steep slopes via material transfer
	 *                            (canyon walls protected by CanyonWallSteepness)
	 *   4. ComputeErosionDelta — subtract final from original elevation
	 *
	 * @param InputElevation Combined elevation map [0,1] from Stage 2
	 * @param RiverMap       Normalized river strength [0,1] from Stage 3
	 * @param UpliftMap      Uplift intensity [0,1] from Stage 2
	 * @param LandMask       Binary mask (1 = land, 0 = ocean) from Stage 1
	 * @return true on success
	 */
	bool Generate(const TArray<float>& InputElevation, const TArray<float>& RiverMap,
		const TArray<float>& UpliftMap, const TArray<uint8>& LandMask);

	/** Eroded elevation per pixel, range [0,1]. */
	const TArray<float>& GetErodedElevation() const { return ErodedElevation; }

	/** Binary canyon mask (1 = canyon, 0 = not). */
	const TArray<uint8>& GetCanyonMask() const { return CanyonMask; }

	/** Per-pixel elevation change (pre-erosion minus post-erosion).
	 *  Positive values = material was removed; negative = deposited. */
	const TArray<float>& GetErosionDeltaMap() const { return ErosionDeltaMap; }

	/** The actual seed used for generation (derived or explicit). */
	int32 GetSeed() const { return ActualSeed; }

private:

	//--------------------------------------------------------------------------
	// Configuration
	//--------------------------------------------------------------------------
	FErosionSettings Settings;
	int32 ActualSeed = 0;
	int32 Resolution = 0;

	//--------------------------------------------------------------------------
	// Output buffers (Resolution * Resolution elements each)
	//--------------------------------------------------------------------------

	/** Elevation after river incision and thermal erosion. */
	TArray<float> ErodedElevation;

	/** Binary canyon mask: 1 where river incision lowered terrain. */
	TArray<uint8> CanyonMask;

	/** Signed elevation change per pixel (pre − post). */
	TArray<float> ErosionDeltaMap;

	//--------------------------------------------------------------------------
	// Internal helpers
	//--------------------------------------------------------------------------

	/**
	 * Carve canyons along river paths with width expansion.
	 * For each land pixel where the river map is positive, lower the elevation
	 * proportionally to river strength.  High-uplift areas receive a deeper cut
	 * via the CanyonDepthMultiplier.  The carving is then expanded outward by
	 * CanyonWidth pixels with linear taper to form a realistic valley profile.
	 * All carved pixels are marked in CanyonMask.
	 */
	void ApplyRiverIncision(const TArray<float>& RiverMap, const TArray<float>& UpliftMap,
		const TArray<uint8>& LandMask);

	/**
	 * Iterative thermal weathering.
	 * For each pass, examine every land pixel's 8 neighbors.  If the height
	 * difference exceeds ThermalErosionThreshold, move a fraction of the excess
	 * material from the higher pixel to the lower one.  Uses a double-buffered
	 * approach to avoid order-dependent artifacts.
	 *
	 * Canyon-adjacent pixels (CanyonMask == 1) have their erosion rate scaled
	 * down by CanyonWallSteepness to preserve steep canyon walls.
	 */
	void ApplyThermalErosion(const TArray<uint8>& LandMask);
};
