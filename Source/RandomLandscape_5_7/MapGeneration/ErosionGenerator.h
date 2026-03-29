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
 * Output: ErodedElevation — a modified elevation map of Resolution*Resolution size.
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
	 *   2. ApplyRiverIncision  — carve canyons along river paths
	 *   3. ApplyThermalErosion — smooth steep slopes via material transfer
	 *
	 * @param InputElevation Combined elevation map [0,1] from Stage 2
	 * @param RiverMap       Normalized river strength [0,1] from Stage 3
	 * @param UpliftMap      Uplift intensity [0,1] from Stage 2
	 * @param LandMask       Binary mask (255 = land, 0 = ocean) from Stage 1
	 * @return true on success
	 */
	bool Generate(const TArray<float>& InputElevation, const TArray<float>& RiverMap,
		const TArray<float>& UpliftMap, const TArray<uint8>& LandMask);

	/** Eroded elevation per pixel, range [0,1]. */
	const TArray<float>& GetErodedElevation() const { return ErodedElevation; }

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
	// Output buffer (Resolution * Resolution elements)
	//--------------------------------------------------------------------------

	/** Elevation after river incision and thermal erosion. */
	TArray<float> ErodedElevation;

	//--------------------------------------------------------------------------
	// Internal helpers
	//--------------------------------------------------------------------------

	/**
	 * Carve canyons along river paths.
	 * For each land pixel where the river map is positive, lower the elevation
	 * proportionally to river strength.  High-uplift areas receive a deeper cut
	 * via the CanyonDepthMultiplier.
	 */
	void ApplyRiverIncision(const TArray<float>& RiverMap, const TArray<float>& UpliftMap,
		const TArray<uint8>& LandMask);

	/**
	 * Iterative thermal weathering.
	 * For each pass, examine every land pixel's 8 neighbors.  If the height
	 * difference exceeds ThermalErosionThreshold, move a fraction of the excess
	 * material from the higher pixel to the lower one.  Uses a double-buffered
	 * approach to avoid order-dependent artifacts.
	 */
	void ApplyThermalErosion(const TArray<uint8>& LandMask);
};
