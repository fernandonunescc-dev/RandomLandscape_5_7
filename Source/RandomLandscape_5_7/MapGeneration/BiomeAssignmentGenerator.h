#pragma once

#include "CoreMinimal.h"
#include "BiomeTypes.h"
#include "WorldGenTypes.h"
#include "BiomeAssignmentGenerator.generated.h"

/**
 * Stage 6: Biome Assignment — rule-based classification from climate + terrain.
 *
 * Takes elevation, temperature, moisture, land mask, and volcanic centers from
 * earlier stages and produces a Resolution*Resolution biome map where each
 * pixel stores an EBiomeType encoded as int32.
 *
 * Classification priority:
 *   1. Ocean        (LandMask == 0)
 *   2. Volcanic     (near volcanic center AND Elevation > 0.3)
 *   3. Mountain     (Elevation > MountainElevationThreshold)
 *   4. Ice          (Temperature < IceTemperatureThreshold AND Moisture > IceMoistureThreshold)
 *   5. Snow         (Temperature < SnowTemperatureThreshold)
 *   6. Desert       (Temperature > DesertTemperatureThreshold AND Moisture < DesertMoistureThreshold)
 *   7. Forest       (Moisture > ForestMoistureThreshold AND Temperature > ForestTemperatureThreshold)
 *   8. Land         (default grassland)
 */
UCLASS(BlueprintType)
class RANDOMLANDSCAPE_5_7_API UBiomeAssignmentGenerator : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Store settings and allocate the biome map buffer.
	 *
	 * @param InSettings        Biome classification thresholds
	 * @param TextureResolution Width/height of the square map in pixels
	 */
	void Initialize(const FBiomeAssignmentSettings& InSettings, int32 TextureResolution);

	/**
	 * Classify every pixel into a biome type.
	 *
	 * @param Elevation       Per-pixel elevation [0,1]
	 * @param Temperature     Per-pixel temperature [0,1]
	 * @param Moisture        Per-pixel moisture [0,1]
	 * @param LandMask        Binary mask (1 = land, 0 = ocean)
	 * @param VolcanicCenters Volcanic hotspot positions in normalized [0,1] space
	 * @return true on success
	 */
	bool Generate(const TArray<float>& Elevation, const TArray<float>& Temperature,
		const TArray<float>& Moisture, const TArray<uint8>& LandMask,
		const TArray<FVector2D>& VolcanicCenters);

	/** Biome map — stores static_cast<int32>(EBiomeType) per pixel. */
	const TArray<int32>& GetBiomeMap() const { return BiomeMap; }

private:

	FBiomeAssignmentSettings Settings;
	int32 Resolution = 0;
	TArray<int32> BiomeMap;
};
