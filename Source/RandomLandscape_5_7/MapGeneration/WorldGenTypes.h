// WorldGenTypes.h
// Settings structs for each stage of the geology-driven world generation pipeline.
// Pipeline: Landmass → Uplift → Hydrology → Erosion → Climate → Biomes → Refinement → Mesh

#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.generated.h"

/**
 * Stage 2: Uplift / geology control map settings.
 * Drives mountains, plateaus, and volcanic hotspots from tectonic-style noise.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FUpliftSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Uplift", meta = (ClampMin = "0"))
	int32 Seed = 0;

	// --- Base elevation from coastline distance ---

	/** How many pixels inland before elevation reaches maximum gradient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "10", ClampMax = "256"))
	int32 CoastlineGradientWidth = 80;

	/** Noise frequency modulating the base elevation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float BaseNoiseFrequency = 1.5f;

	/** Noise octaves for base elevation modulation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "1", ClampMax = "8"))
	int32 BaseNoiseOctaves = 4;

	/** Noise persistence for base elevation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float BaseNoisePersistence = 0.45f;

	// --- Mountain ridges (ridged noise) ---

	/** Frequency of mountain ridge features */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float MountainRidgeFrequency = 2.5f;

	/** Amplitude of mountain ridges relative to total height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MountainRidgeAmplitude = 0.6f;

	/** Sharpness of mountain peaks (higher = sharper ridges) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float MountainSharpness = 2.0f;

	/** Octaves for ridged mountain noise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MountainOctaves = 5;

	// --- Volcanic hotspots ---

	/** Number of volcanic hotspots to place */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0", ClampMax = "10"))
	int32 VolcanicHotspotCount = 2;

	/** Radius of volcanic influence (normalized 0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.02", ClampMax = "0.3"))
	float VolcanicRadius = 0.08f;

	/** Peak height of volcanoes relative to max (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float VolcanicPeakHeight = 0.85f;

	/** Crater depth as fraction of peak */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.0", ClampMax = "0.8"))
	float CraterDepth = 0.3f;

	/** Crater radius as fraction of volcano radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CraterRadiusFraction = 0.2f;
};

/**
 * Stage 3: Hydrology settings — rivers and lakes from drainage simulation.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FHydrologySettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hydrology", meta = (ClampMin = "0"))
	int32 Seed = 0;

	/** Minimum flow accumulation to classify as a river (fraction of total land pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers", meta = (ClampMin = "0.0001", ClampMax = "0.05"))
	float RiverThreshold = 0.005f;

	/** How much noise to add to flow direction (prevents perfectly straight rivers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers", meta = (ClampMin = "0.0", ClampMax = "0.02"))
	float FlowJitter = 0.002f;

	/** Lake fill — if a local minimum catches flow above this, mark as lake */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lakes", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float LakeThreshold = 0.01f;
};

/**
 * Stage 4: Erosion settings — river incision and thermal erosion.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FErosionSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Erosion", meta = (ClampMin = "0"))
	int32 Seed = 0;

	/** How strongly rivers carve into terrain (creates valleys/canyons) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Incision", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RiverIncisionStrength = 0.3f;

	/** Canyon depth multiplier in high-uplift areas */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Incision", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float CanyonDepthMultiplier = 1.5f;

	/** Number of thermal erosion passes (smooths steep slopes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermal Erosion", meta = (ClampMin = "0", ClampMax = "20"))
	int32 ThermalErosionPasses = 3;

	/** Maximum slope angle before thermal erosion kicks in (0-1, as height difference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermal Erosion", meta = (ClampMin = "0.001", ClampMax = "0.2"))
	float ThermalErosionThreshold = 0.02f;

	/** How much material moves per thermal pass */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermal Erosion", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ThermalErosionRate = 0.3f;
};

/**
 * Stage 5: Climate settings — derived from terrain geometry.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FClimateSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate", meta = (ClampMin = "0"))
	int32 Seed = 0;

	/** Y coordinate of the equator in normalized [0,1] space (0.5 = map center) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EquatorPosition = 0.5f;

	/** Temperature at the equator (0-1 normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EquatorialTemperature = 0.95f;

	/** Temperature at the poles (0-1 normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PolarTemperature = 0.05f;

	/** How much temperature drops per unit of elevation (lapse rate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ElevationLapseRate = 0.4f;

	/** How quickly moisture decays inland from the coast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.001", ClampMax = "0.1"))
	float MoistureDecayRate = 0.015f;

	/** Strength of rain shadow effect behind mountains */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float RainShadowStrength = 1.0f;

	/** Prevailing wind direction (0=East, 0.25=North, 0.5=West, 0.75=South) as fraction of 2*PI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindDirection = 0.0f;

	/** How much rivers/lakes boost local moisture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WaterMoistureBoost = 0.3f;

	/** Noise frequency for climate variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ClimateNoiseFrequency = 1.0f;

	/** Noise amplitude for climate variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float ClimateNoiseAmplitude = 0.08f;
};

/**
 * Stage 6: Biome assignment thresholds — rule-based classification from climate + terrain.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FBiomeAssignmentSettings
{
	GENERATED_BODY()

	/** Elevation above which terrain is classified as Mountain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float MountainElevationThreshold = 0.65f;

	/** Temperature below which Snow/Tundra is assigned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float SnowTemperatureThreshold = 0.25f;

	/** Temperature below which Ice/Glacier is assigned (colder than snow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float IceTemperatureThreshold = 0.12f;

	/** Minimum moisture required for Ice classification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IceMoistureThreshold = 0.3f;

	/** Temperature above which Desert is possible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float DesertTemperatureThreshold = 0.6f;

	/** Moisture below which Desert is assigned (when hot enough) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DesertMoistureThreshold = 0.25f;

	/** Moisture above which Forest is assigned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.2", ClampMax = "1.0"))
	float ForestMoistureThreshold = 0.5f;

	/** Minimum temperature for Forest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.1", ClampMax = "0.6"))
	float ForestTemperatureThreshold = 0.3f;

	/** Radius around volcanic hotspots classified as Volcanic biome (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "5", ClampMax = "100"))
	int32 VolcanicRadiusPixels = 30;
};

/**
 * Stage 7: Terrain refinement settings — biome-local detail noise on top of macro terrain.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FTerrainRefinementSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "0"))
	int32 Seed = 0;

	/** Overall scale of biome detail noise (0 = no detail, 1 = full) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DetailScale = 0.08f;

	/** Noise frequency for detail features */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "0.5", ClampMax = "20.0"))
	float DetailFrequency = 4.0f;

	/** Noise octaves for detail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "1", ClampMax = "6"))
	int32 DetailOctaves = 3;

	/** Desert dune frequency */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desert", meta = (ClampMin = "1.0", ClampMax = "20.0"))
	float DuneFrequency = 8.0f;

	/** Desert dune height relative to detail scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desert", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DuneHeight = 0.5f;

	/** Mountain extra ridge detail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountain", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MountainDetailScale = 0.12f;

	/** Number of smoothing passes on the final elevation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post-Process", meta = (ClampMin = "0", ClampMax = "10"))
	int32 FinalSmoothingPasses = 2;
};
