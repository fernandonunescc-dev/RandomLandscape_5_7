// TerrainRefinementGenerator.h
// Stage 7: Terrain Refinement — biome-local detail noise on top of macro terrain.
//
// Takes the eroded elevation from Stage 4, the biome map from Stage 6, and
// the land mask from Stage 1.  Each biome receives a distinct noise layer
// (FBM, ridged FBM, dune patterns, etc.) that adds fine-grained topographic
// detail without altering the large-scale landforms.
//
// Output: FinalElevation — a refined elevation map of Resolution*Resolution size.

#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include "WorldGenTypes.h"
#include "TerrainRefinementGenerator.generated.h"

/**
 * Stage 7: Terrain Refinement generator.
 *
 * Adds biome-specific detail noise on top of the macro terrain produced by
 * earlier pipeline stages.  Each biome type uses a decorrelated seed and a
 * noise profile tuned to its expected surface character:
 *
 *   - Ocean:    no modification (stays at 0)
 *   - Land:     very gentle FBM hills
 *   - Forest:   moderate FBM with extra octave for uneven ground
 *   - Desert:   sinusoidal dune pattern modulated by noise
 *   - Snow:     moderate FBM (similar to forest)
 *   - Ice:      very subtle flat noise
 *   - Mountain: ridged FBM for sharper peaks
 *   - Volcanic: gentle noise (volcano shape comes from uplift)
 *
 * After detail is applied, a configurable number of 3×3 box-blur passes
 * smooth the result on land pixels only.
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UTerrainRefinementGenerator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store settings, derive the actual seed, and record the texture resolution.
	 *
	 * @param InSettings        Refinement configuration (detail scale, dune params, etc.)
	 * @param GlobalSeed        Pipeline-wide seed used when Settings.Seed == 0
	 * @param TextureResolution Width/height of the square output map in pixels
	 */
	void Initialize(const FTerrainRefinementSettings& InSettings, int32 GlobalSeed, int32 TextureResolution);

	/**
	 * Run the terrain refinement pipeline.
	 *
	 * Algorithm:
	 *   1. Copy ErodedElevation into FinalElevation
	 *   2. For each land pixel, add biome-specific detail noise
	 *   3. Clamp result to [0, 1]
	 *   4. Apply smoothing passes
	 *
	 * @param ErodedElevation Elevation map [0,1] from Stage 4
	 * @param BiomeMap        Per-pixel biome ID map from Stage 6
	 * @param LandMask        Binary mask (255 = land, 0 = ocean) from Stage 1
	 * @return true on success
	 */
	bool Generate(const TArray<float>& ErodedElevation, const TArray<int32>& BiomeMap, const TArray<uint8>& LandMask);

	/** Refined elevation per pixel, range [0,1]. */
	const TArray<float>& GetFinalElevation() const { return FinalElevation; }

	/** The actual seed used for generation (derived or explicit). */
	int32 GetSeed() const { return ActualSeed; }

private:

	//--------------------------------------------------------------------------
	// Configuration
	//--------------------------------------------------------------------------
	FTerrainRefinementSettings Settings;
	int32 ActualSeed = 0;
	int32 Resolution = 0;

	//--------------------------------------------------------------------------
	// Output buffer (Resolution * Resolution elements)
	//--------------------------------------------------------------------------

	/** Elevation after biome detail noise and smoothing. */
	TArray<float> FinalElevation;

	//--------------------------------------------------------------------------
	// Internal helpers
	//--------------------------------------------------------------------------

	/**
	 * Compute the biome-specific detail noise contribution for a given
	 * normalized position.  Each biome type uses a different noise profile
	 * and a decorrelated seed offset.
	 *
	 * @param NormX  Horizontal position normalised to [0, 1]
	 * @param NormY  Vertical position normalised to [0, 1]
	 * @param Biome  The biome type at this location
	 * @return Signed detail offset to add to the base elevation
	 */
	float ComputeBiomeDetail(float NormX, float NormY, EBiomeType Biome) const;

	/**
	 * 3×3 box-blur smoothing on land pixels.
	 * Uses a double-buffered swap to avoid order-dependent artifacts.
	 * Ocean pixels are left at 0.
	 *
	 * @param LandMask Binary mask (255 = land, 0 = ocean)
	 */
	void ApplySmoothing(const TArray<uint8>& LandMask);
};
