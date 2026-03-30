#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.h"
#include "TerrainValidator.generated.h"

/**
 * Terrain plausibility validator.
 *
 * Runs post-generation checks on cached pipeline data to detect
 * implausible artifacts such as uphill rivers, ridge-top lakes,
 * abrupt biome seams, and unsupported geological features.
 *
 * Each check populates one or more FTerrainValidationError entries
 * with severity, suggested parameter fixes, and the debug map to inspect.
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UTerrainValidator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store the texture resolution used by all maps.
	 * Must be called before Validate().
	 */
	void Initialize(int32 TextureResolution);

	/**
	 * Run all plausibility checks against the cached pipeline data.
	 *
	 * @param LandMask            Binary (1 = land) from Stage 1
	 * @param CombinedElevation   [0,1] from Stage 2
	 * @param UpliftMap           [0,1] from Stage 2
	 * @param FlowDirection       D8 index (0-7, -1 = sink) from Stage 3
	 * @param RiverMap            [0,1] river strength from Stage 3
	 * @param LakeMap             Binary lake mask from Stage 3
	 * @param WaterfallMap        Binary waterfall mask from Stage 3
	 * @param CanyonMask          Binary canyon mask from Stage 4
	 * @param ErodedElevation     [0,1] from Stage 4
	 * @param Temperature         [0,1] from Stage 5
	 * @param Moisture            [0,1] from Stage 5
	 * @param BiomeMap            Biome index per pixel from Stage 6
	 * @param SlopeMap            [0,1] slope per pixel from Stage 6
	 * @param VolcanicCenters     Normalized volcanic hotspot positions from Stage 2
	 * @return Aggregated validation result
	 */
	FTerrainValidationResult Validate(
		const TArray<uint8>& LandMask,
		const TArray<float>& CombinedElevation,
		const TArray<float>& UpliftMap,
		const TArray<int32>& FlowDirection,
		const TArray<float>& RiverMap,
		const TArray<uint8>& LakeMap,
		const TArray<uint8>& WaterfallMap,
		const TArray<uint8>& CanyonMask,
		const TArray<float>& ErodedElevation,
		const TArray<float>& Temperature,
		const TArray<float>& Moisture,
		const TArray<int32>& BiomeMap,
		const TArray<float>& SlopeMap,
		const TArray<FVector2D>& VolcanicCenters);

private:

	int32 Resolution = 0;

	// Individual check methods — each appends to OutErrors

	void CheckUphillRivers(
		const TArray<float>& Elevation, const TArray<int32>& FlowDirection,
		const TArray<float>& RiverMap, const TArray<uint8>& LandMask,
		TArray<FTerrainValidationError>& OutErrors);

	void CheckRidgeLakes(
		const TArray<uint8>& LakeMap, const TArray<float>& Elevation,
		const TArray<float>& SlopeMap, const TArray<FVector2D>& VolcanicCenters,
		TArray<FTerrainValidationError>& OutErrors);

	void CheckAbruptBiomeSeams(
		const TArray<int32>& BiomeMap, const TArray<float>& Temperature,
		const TArray<float>& Moisture, const TArray<uint8>& LandMask,
		TArray<FTerrainValidationError>& OutErrors);

	void CheckUnsupportedMountains(
		const TArray<int32>& BiomeMap, const TArray<float>& UpliftMap,
		const TArray<uint8>& LandMask,
		TArray<FTerrainValidationError>& OutErrors);

	void CheckCanyonsWithoutRivers(
		const TArray<uint8>& CanyonMask, const TArray<float>& RiverMap,
		TArray<FTerrainValidationError>& OutErrors);

	void CheckWaterfallElevationDrop(
		const TArray<uint8>& WaterfallMap, const TArray<float>& Elevation,
		const TArray<int32>& FlowDirection,
		TArray<FTerrainValidationError>& OutErrors);

	void CheckIsolatedWetBiomes(
		const TArray<int32>& BiomeMap, const TArray<float>& Moisture,
		const TArray<float>& RiverMap, const TArray<uint8>& LakeMap,
		const TArray<uint8>& LandMask,
		TArray<FTerrainValidationError>& OutErrors);

	void CheckVolcanoPlacement(
		const TArray<FVector2D>& VolcanicCenters, const TArray<uint8>& LandMask,
		TArray<FTerrainValidationError>& OutErrors);
};
