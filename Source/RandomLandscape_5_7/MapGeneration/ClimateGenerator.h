#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.h"
#include "ClimateGenerator.generated.h"

/**
 * Stage 5: Climate — temperature, moisture, and precipitation computed from
 * terrain geometry.
 *
 * Takes elevation, land mask, river map, and lake map from earlier stages and
 * produces three Resolution*Resolution maps:
 *   - Temperature  (0 = cold,  1 = hot)
 *   - Moisture     (0 = dry,   1 = wet)
 *   - Precipitation(0 = none,  1 = heavy)
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UClimateGenerator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store settings, derive the actual seed, and record the texture resolution.
	 *
	 * @param InSettings        Climate configuration (equator position, lapse rate, etc.)
	 * @param GlobalSeed        Pipeline-wide seed used when Settings.Seed == 0
	 * @param TextureResolution Width/height of the square output map in pixels
	 */
	void Initialize(const FClimateSettings& InSettings, int32 GlobalSeed, int32 TextureResolution);

	/**
	 * Run the full climate pipeline.
	 *
	 * Algorithm:
	 *   1. ComputeTemperature  — latitude + elevation lapse + noise
	 *   2. ComputeMoisture     — coast distance + rain shadow + water boost
	 *   3. ComputePrecipitation— combine moisture and temperature
	 *
	 * @param Elevation Combined/eroded elevation [0,1]
	 * @param LandMask  Binary mask (255 = land, 0 = ocean)
	 * @param RiverMap  Normalized river strength [0,1]
	 * @param LakeMap   Binary mask (1 = lake, 0 = not lake)
	 * @return true on success
	 */
	bool Generate(const TArray<float>& Elevation, const TArray<uint8>& LandMask,
		const TArray<float>& RiverMap, const TArray<uint8>& LakeMap);

	/** Temperature per pixel, range [0,1] (0 = cold, 1 = hot). */
	const TArray<float>& GetTemperature() const { return Temperature; }

	/** Moisture per pixel, range [0,1] (0 = dry, 1 = wet). */
	const TArray<float>& GetMoisture() const { return Moisture; }

	/** Precipitation per pixel, range [0,1] (0 = none, 1 = heavy). */
	const TArray<float>& GetPrecipitation() const { return Precipitation; }

	/** The actual seed used for generation (derived or explicit). */
	int32 GetSeed() const { return ActualSeed; }

private:

	//--------------------------------------------------------------------------
	// Configuration
	//--------------------------------------------------------------------------
	FClimateSettings Settings;
	int32 ActualSeed = 0;
	int32 Resolution = 0;

	//--------------------------------------------------------------------------
	// Output buffers (Resolution * Resolution elements)
	//--------------------------------------------------------------------------
	TArray<float> Temperature;
	TArray<float> Moisture;
	TArray<float> Precipitation;

	//--------------------------------------------------------------------------
	// Internal helpers
	//--------------------------------------------------------------------------

	/**
	 * Latitude-based temperature with elevation lapse rate and noise perturbation.
	 */
	void ComputeTemperature(const TArray<float>& Elevation, const TArray<uint8>& LandMask);

	/**
	 * Exponential decay from coast, rain shadow behind mountains, and water body
	 * boost from rivers/lakes.
	 */
	void ComputeMoisture(const TArray<float>& Elevation, const TArray<uint8>& LandMask,
		const TArray<float>& RiverMap, const TArray<uint8>& LakeMap);

	/**
	 * Combine moisture and temperature: warmer + wetter = more precipitation.
	 */
	void ComputePrecipitation();
};
