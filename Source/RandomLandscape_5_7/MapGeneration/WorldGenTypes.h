// WorldGenTypes.h
// Settings structs for each stage of the geology-driven world generation pipeline.
// Pipeline: Landmass → Uplift → Hydrology → Erosion → Climate → Biomes → Refinement → Mesh

#pragma once

#include "CoreMinimal.h"
#include "WorldGenTypes.generated.h"

// Forward declaration for EBiomeType used in per-biome data tables
// (EBiomeType is defined in BiomeTypes.h; avoid circular include)

// ==================== Terrain Validation Types ====================

/** Severity level for terrain plausibility issues. */
UENUM(BlueprintType)
enum class EValidationSeverity : uint8
{
	Info     UMETA(DisplayName = "Info"),
	Warning  UMETA(DisplayName = "Warning"),
	Error    UMETA(DisplayName = "Error")
};

/** A single terrain plausibility issue found during validation. */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FTerrainValidationError
{
	GENERATED_BODY()

	/** Which check produced this error (e.g. "UphillRiver", "RidgeLake") */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	FString CheckName;

	/** How severe the issue is */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	EValidationSeverity Severity = EValidationSeverity::Warning;

	/** Human-readable explanation of what went wrong */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	FString Description;

	/** Number of pixels or sites affected */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	int32 AffectedPixelCount = 0;

	/** Fraction of relevant pixels affected (0-1) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	float AffectedPercentage = 0.0f;

	/** Suggested parameter tweak to fix the issue */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	FString SuggestedFix;

	/** Debug map name to inspect for this issue */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	FString DebugMapToInspect;
};

/** Aggregate result of all terrain plausibility checks. */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FTerrainValidationResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	TArray<FTerrainValidationError> Errors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	int32 TotalChecksRun = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	int32 ErrorCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	int32 WarningCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Validation")
	int32 InfoCount = 0;
};

/**
 * Per-biome terrain refinement profile.
 * Describes the noise character applied during Stage 7 for a single biome type.
 * Stored as an array in FTerrainRefinementSettings, indexed by biome.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FPerBiomeTerrainProfile
{
	GENERATED_BODY()

	/** Noise frequency multiplier for this biome (relative to global DetailFrequency) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Profile", meta = (ClampMin = "0.1", ClampMax = "10.0", Tooltip = "Multiplier applied to the global DetailFrequency for this biome. Values above 1 add higher-frequency detail (e.g. rocky crags); values below 1 produce smoother, broader features."))
	float FrequencyMultiplier = 1.0f;

	/** Noise amplitude multiplier for this biome (relative to global DetailScale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Profile", meta = (ClampMin = "0.0", ClampMax = "5.0", Tooltip = "Multiplier applied to the global DetailScale for this biome. Increase to make this biome's detail noise more prominent; set to 0 to disable biome-specific detail entirely."))
	float AmplitudeMultiplier = 1.0f;

	/** Extra octaves added on top of the global DetailOctaves for this biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Profile", meta = (ClampMin = "0", ClampMax = "4", Tooltip = "Additional noise octaves layered on top of the global DetailOctaves for this biome. More octaves add finer micro-detail (e.g. small rocks on mountain slopes)."))
	int32 ExtraOctaves = 0;

	/** If true, use ridged FBM instead of standard FBM */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Profile", meta = (Tooltip = "When enabled, this biome uses ridged FBM noise instead of standard FBM. Ridged noise creates sharper, more jagged features suited to mountains or volcanic terrain."))
	bool bUseRidgedNoise = false;

	/** Ridged noise sharpness (only used when bUseRidgedNoise = true) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome Profile", meta = (ClampMin = "0.5", ClampMax = "5.0", EditCondition = "bUseRidgedNoise", Tooltip = "Power exponent for the ridged noise. Higher values sharpen ridges into knife-edge peaks; lower values produce softer, rounded ridges. Only takes effect when Use Ridged Noise is enabled."))
	float RidgedSharpness = 2.0f;
};

/**
 * Per-biome material and foliage spawn rules.
 * Describes base material, tint, and foliage density for a single biome type.
 * These are data-only descriptors used by downstream rendering / spawn systems.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FBiomeMaterialRules
{
	GENERATED_BODY()

	/** Base material asset path (soft reference to avoid hard dependency) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (Tooltip = "Soft reference to the base material asset for this biome. Used by downstream rendering systems to apply the correct surface material."))
	FSoftObjectPath BaseMaterial;

	/** Tint color applied to the base material in this biome */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (Tooltip = "Color tint blended with the base material in this biome. Use white for no tinting; adjust hue/saturation for seasonal or environmental color shifts."))
	FLinearColor Tint = FLinearColor::White;

	/** Roughness override for the material (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "PBR roughness override for the biome surface. 0 = mirror-smooth (ice, wet rock), 1 = fully rough (dirt, sand). Mid-range values suit grass or packed earth."))
	float Roughness = 0.7f;

	/** Foliage density (instances per sq. meter).  0 = no foliage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "50.0", Tooltip = "Number of foliage instances placed per square metre in this biome. Set to 0 to disable foliage. Dense forest might use 5-15; sparse grassland 1-3."))
	float FoliageDensity = 0.0f;

	/** Foliage mesh asset path (soft reference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (Tooltip = "Soft reference to the static mesh used for foliage instances in this biome (e.g. tree, bush, grass clump)."))
	FSoftObjectPath FoliageMesh;

	/** Minimum slope at which foliage can spawn (0-1, as dot product with up) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Minimum terrain slope (as dot product with the up vector) at which foliage is allowed to spawn. 0 = flat ground only can spawn; increase to allow foliage on steeper terrain."))
	float FoliageMinSlope = 0.0f;

	/** Maximum slope at which foliage can spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Foliage", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Maximum terrain slope (as dot product with the up vector) at which foliage is still placed. Above this angle the surface is considered too steep for vegetation."))
	float FoliageMaxSlope = 0.8f;
};

/**
 * Stage 2: Uplift / geology control map settings.
 * Drives mountains, plateaus, and volcanic hotspots from tectonic-style noise.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FUpliftSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Uplift", meta = (ClampMin = "0", Tooltip = "Per-stage random seed. Leave at 0 to derive automatically from the global world seed. Set a specific value to lock this stage's output while experimenting with other stages."))
	int32 Seed = 0;

	// --- Base elevation from coastline distance ---

	/** How many pixels inland before elevation reaches maximum gradient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "10", ClampMax = "256", Tooltip = "Distance in pixels from the coastline before terrain reaches full inland height. Larger values produce wider, gentler coastal plains; smaller values create steep cliffs right at the shore."))
	int32 CoastlineGradientWidth = 100;

	/** How much the coastal gradient varies around the island (0 = uniform, 1 = full variation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Controls how much the coastline steepness varies around the island. At 0 the coast is uniform everywhere. At 1 some sides may have steep cliffs while others have gentle beaches. The variation is driven by low-frequency noise so it changes gradually."))
	float CoastalVariation = 0.6f;

	/** Noise frequency for coastal variation (lower = larger zones of similar steepness) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "0.2", ClampMax = "5.0", Tooltip = "Frequency of the noise that drives coastal variation. Lower values create a few large zones of similar steepness (e.g. one steep side, one gentle side). Higher values create more frequent changes around the shoreline."))
	float CoastalVariationFrequency = 0.8f;

	/** Noise frequency modulating the base elevation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "0.1", ClampMax = "10.0", Tooltip = "Controls how rapidly the base terrain varies across the map. Higher values add more frequent elevation changes over shorter distances; lower values produce broad, smooth landmass shapes."))
	float BaseNoiseFrequency = 1.5f;

	/** Noise octaves for base elevation modulation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "1", ClampMax = "8", Tooltip = "Number of FBM noise layers added together for the base elevation. More octaves add finer detail on top of the large-scale shape but increase generation time slightly."))
	int32 BaseNoiseOctaves = 4;

	/** Noise persistence for base elevation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base Elevation", meta = (ClampMin = "0.1", ClampMax = "1.0", Tooltip = "How much influence each successive noise octave has relative to the previous one. Lower values produce smoother terrain; higher values make fine detail almost as prominent as large features."))
	float BaseNoisePersistence = 0.35f;

	// --- Mountain ridges (ridged noise) ---

	/** Frequency of mountain ridge features */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.5", ClampMax = "10.0", Tooltip = "Controls the spacing of tectonic-style mountain ridges. Higher values create denser, narrower ridge lines; lower values produce wider, more spread-out mountain ranges."))
	float MountainRidgeFrequency = 2.5f;

	/** Amplitude of mountain ridges relative to total height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Maximum height of mountain ridges as a proportion of the total elevation range. At 0 mountains are disabled entirely; at 1 ridges can reach the full map height. Ridges fade toward the coast automatically."))
	float MountainRidgeAmplitude = 0.35f;

	/** Sharpness of mountain peaks (higher = sharper ridges) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.5", ClampMax = "5.0", Tooltip = "Power exponent applied to the ridged noise. Higher values produce sharper, more defined peaks and deeper valleys between them; lower values yield softer, rounded mountain tops."))
	float MountainSharpness = 1.5f;

	/** Octaves for ridged mountain noise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "1", ClampMax = "8", Tooltip = "Number of ridged-noise layers for mountain generation. More octaves add smaller-scale ridge detail on top of the main mountain shape."))
	int32 MountainOctaves = 4;

	/** How much of the island interior is covered by mountains (lower = confined to center, higher = spread everywhere) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta = (ClampMin = "0.1", ClampMax = "2.0", Tooltip = "Controls the footprint of mountain ridges across the landmass. Low values (e.g. 0.2) confine mountains to the highest central peaks. High values (e.g. 1.5-2.0) let ridges extend all the way to the coast. At the default (0.5) mountains fade through the mid-elevation band."))
	float MountainCoverage = 0.5f;

	// --- Hills (rolling FBM terrain) ---

	/** Frequency of hill features */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hills", meta = (ClampMin = "0.5", ClampMax = "10.0", Tooltip = "Controls how frequently rolling hills appear across the terrain. Higher values produce many small hills; lower values create fewer, broader undulations."))
	float HillFrequency = 3.0f;

	/** Amplitude of hill features relative to total height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hills", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Maximum height contribution of hills as a proportion of total elevation. Set to 0 to disable hills. Increase for more pronounced rolling terrain between the mountains."))
	float HillAmplitude = 0.2f;

	/** Octaves for hill noise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hills", meta = (ClampMin = "1", ClampMax = "6", Tooltip = "Number of FBM noise layers for hill generation. More octaves add finer bumps and texture to the rolling hills."))
	int32 HillOctaves = 3;

	// --- Plateaus (flat-topped elevated areas) ---

	/** Frequency of plateau noise regions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plateaus", meta = (ClampMin = "0.5", ClampMax = "8.0", Tooltip = "Controls the size and spacing of potential plateau zones. Lower values produce a few large plateau regions; higher values create many smaller ones scattered across the map."))
	float PlateauNoiseFrequency = 1.8f;

	/** Noise threshold above which terrain becomes a plateau (higher = rarer) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plateaus", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Noise values above this threshold are flattened into plateaus. A higher threshold means fewer, more isolated plateaus; a lower threshold produces widespread mesa-like terrain."))
	float PlateauThreshold = 0.55f;

	/** How flat plateau tops are (0 = no flattening, 1 = perfectly flat) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plateaus", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Strength of the flattening applied to plateau tops. At 0 no flattening occurs (plateaus are effectively disabled). At 1 the tops are perfectly flat, creating sharp mesa edges."))
	float PlateauFlatness = 0.7f;

	/** Elevation of plateau tops (0-1 range) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Plateaus", meta = (ClampMin = "0.1", ClampMax = "0.8", Tooltip = "Target normalised elevation for the flat top of each plateau (0 = sea level, 1 = maximum map height). Plateaus only form in the mid-elevation band (0.15–0.8) to avoid flattening coastlines or peaks."))
	float PlateauElevation = 0.45f;

	// --- Volcanic hotspots ---

	/** Number of volcanic hotspots to place */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0", ClampMax = "10", Tooltip = "How many volcanic cones to scatter across the landmass. Set to 0 to disable volcanoes entirely. Each volcano is placed randomly on land and generates a cone with an optional crater."))
	int32 VolcanicHotspotCount = 0;

	/** Radius of volcanic influence (normalized 0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.02", ClampMax = "0.3", Tooltip = "Size of each volcano's influence area as a fraction of the total map width. A value of 0.08 means the volcano affects roughly 8 percent of the map. Larger values create massive shield-style volcanoes."))
	float VolcanicRadius = 0.08f;

	/** Peak height of volcanoes relative to max (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.1", ClampMax = "1.0", Tooltip = "Normalised peak elevation of each volcano cone (0 = sea level, 1 = maximum map height). Higher values create towering volcanic peaks that dominate the surrounding terrain."))
	float VolcanicPeakHeight = 0.65f;

	/** Crater depth as fraction of peak */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.0", ClampMax = "0.8", Tooltip = "How deep the summit crater is, expressed as a fraction of the volcano's peak height. Set to 0 for a solid cone with no crater. Values above 0.5 create deep calderas."))
	float CraterDepth = 0.3f;

	/** Crater radius as fraction of volcano radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Volcanic", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Radius of the summit crater bowl as a fraction of the full volcano radius. At 0 there is no visible crater. Larger values widen the crater opening at the top of the cone."))
	float CraterRadiusFraction = 0.3f;
};

/**
 * Stage 3: Hydrology settings — rivers and lakes from drainage simulation.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FHydrologySettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hydrology", meta = (ClampMin = "0", Tooltip = "Per-stage random seed. Leave at 0 to derive automatically from the global world seed. Set a specific value to lock this stage's output while experimenting with other stages."))
	int32 Seed = 0;

	/** Minimum flow accumulation to classify as a river (fraction of total land pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers", meta = (ClampMin = "0.0001", ClampMax = "0.05", Tooltip = "Fraction of total land pixels that must flow through a cell before it is marked as a river. Lower values produce dense river networks; higher values create only major rivers."))
	float RiverThreshold = 0.005f;

	/** How much noise to add to flow direction (prevents perfectly straight rivers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rivers", meta = (ClampMin = "0.0", ClampMax = "0.02", Tooltip = "Small random offset added to the downhill flow direction at each pixel. Prevents rivers from running in perfectly straight lines. Higher values create more meandering rivers but too much can cause unrealistic paths."))
	float FlowJitter = 0.002f;

	/** Lake fill — if a local minimum catches flow above this, mark as lake */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lakes", meta = (ClampMin = "0.001", ClampMax = "0.1", Tooltip = "Flow accumulation threshold for forming lakes in terrain depressions. Pixels in local minima with flow above this fraction of total land area are filled as lakes. Lower values create more lakes; higher values only fill major basins."))
	float LakeThreshold = 0.01f;

	/** Minimum elevation drop along a river pixel to mark as waterfall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waterfalls", meta = (ClampMin = "0.01", ClampMax = "0.3", Tooltip = "Minimum normalised elevation drop between a river pixel and its downstream neighbour to classify as a waterfall. Lower values mark gentler rapids; higher values only flag dramatic falls."))
	float WaterfallMinElevationDrop = 0.05f;
};

/**
 * Stage 4: Erosion settings — river incision, canyon formation, and thermal erosion.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FErosionSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Erosion", meta = (ClampMin = "0", Tooltip = "Per-stage random seed. Leave at 0 to derive automatically from the global world seed. Set a specific value to lock this stage's output while experimenting with other stages."))
	int32 Seed = 0;

	/** How strongly rivers carve into terrain (creates valleys/canyons) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Incision", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Strength of river-driven terrain carving. Higher values cut deeper valleys along river paths. At 0 rivers leave the terrain untouched; at 1 they carve aggressively, creating pronounced V-shaped valleys."))
	float RiverIncisionStrength = 0.3f;

	/** Canyon depth multiplier in high-uplift areas */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "River Incision", meta = (ClampMin = "0.0", ClampMax = "3.0", Tooltip = "Extra depth multiplier applied where the uplift map indicates high tectonic activity. Creates deeper canyons in mountainous terrain while leaving lowland rivers as shallow valleys."))
	float CanyonDepthMultiplier = 1.5f;

	/** Canyon half-width in pixels.  River incision is spread this many pixels
	 *  outward from each river pixel, tapering linearly to zero at the edge.
	 *  0 = only river pixels are carved (no width expansion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canyon", meta = (ClampMin = "0", ClampMax = "16", Tooltip = "Half-width of canyon carving in pixels. Incision is spread outward from each river pixel with a linear taper. 0 carves only the river pixel itself; higher values widen the canyon floor and walls."))
	int32 CanyonWidth = 3;

	/** How much canyon walls are protected from thermal erosion.
	 *  0 = no protection (thermal erosion treats canyon walls normally).
	 *  1 = full protection (canyon-adjacent pixels never erode thermally).
	 *  Intermediate values scale the erosion rate down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Canyon", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Protection factor preventing thermal erosion from smoothing canyon walls. At 0 canyon walls erode normally, losing their sharp profile. At 1 canyon walls are fully preserved, maintaining steep cliff faces."))
	float CanyonWallSteepness = 0.7f;

	/** Number of thermal erosion passes (smooths steep slopes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermal Erosion", meta = (ClampMin = "0", ClampMax = "20", Tooltip = "Number of thermal erosion iterations. Each pass moves material from steep slopes to lower neighbours, smoothing out abrupt cliffs. More passes produce a more weathered, rounded landscape."))
	int32 ThermalErosionPasses = 5;

	/** Maximum slope angle before thermal erosion kicks in (0-1, as height difference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermal Erosion", meta = (ClampMin = "0.001", ClampMax = "0.2", Tooltip = "Normalised height difference between neighbours above which thermal erosion activates. Slopes steeper than this threshold shed material downhill. Lower values erode gentler slopes; higher values only erode very steep terrain."))
	float ThermalErosionThreshold = 0.02f;

	/** How much material moves per thermal pass */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermal Erosion", meta = (ClampMin = "0.01", ClampMax = "1.0", Tooltip = "Fraction of excess slope material moved per erosion pass. Higher rates produce faster, more dramatic smoothing; lower rates give subtle weathering over many passes."))
	float ThermalErosionRate = 0.3f;

	/** Minimum elevation for thermal erosion to apply (0 = erode everywhere, 0.3 = only above 30% elevation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thermal Erosion", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Normalised elevation floor below which thermal erosion is skipped. At 0, erosion smooths the entire landscape. Increase this value to confine weathering to high-altitude mountain terrain, preventing lowlands and coastal areas from being affected."))
	float ThermalErosionMinElevation = 0.0f;
};

/**
 * Stage 5: Climate settings — derived from terrain geometry.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FClimateSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climate", meta = (ClampMin = "0", Tooltip = "Per-stage random seed. Leave at 0 to derive automatically from the global world seed. Set a specific value to lock this stage's output while experimenting with other stages."))
	int32 Seed = 0;

	/** Y coordinate of the equator in normalized [0,1] space (0.5 = map center) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Normalised Y position of the equator on the map (0 = top edge, 0.5 = centre, 1 = bottom edge). Temperature is highest here and decreases toward the poles."))
	float EquatorPosition = 0.5f;

	/** Temperature at the equator (0-1 normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Normalised base temperature at the equator. 1.0 is the hottest possible; lower values simulate cooler climates globally."))
	float EquatorialTemperature = 0.95f;

	/** Temperature at the poles (0-1 normalized) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Normalised base temperature at the poles (map edges farthest from the equator). Lower values create colder polar regions with more snow and ice biomes."))
	float PolarTemperature = 0.05f;

	/** How much temperature drops per unit of elevation (lapse rate) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Temperature", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Temperature reduction per unit of normalised elevation (adiabatic lapse rate). Higher values make mountaintops significantly colder than lowlands, encouraging snow biomes at altitude."))
	float ElevationLapseRate = 0.3f;

	/** How quickly moisture decays inland from the coast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.001", ClampMax = "0.1", Tooltip = "Rate at which moisture decreases with distance from the coastline. Higher values dry out the interior faster, creating arid inland zones. Lower values allow moisture to penetrate deep inland."))
	float MoistureDecayRate = 0.015f;

	/** Strength of rain shadow effect behind mountains */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "2.0", Tooltip = "How strongly mountain ranges block moisture carried by prevailing winds. At 0 mountains have no rain-shadow effect; at 2 the leeward side is extremely dry, encouraging desert biomes behind mountain ranges."))
	float RainShadowStrength = 1.0f;

	/** Prevailing wind direction (0=East, 0.25=North, 0.5=West, 0.75=South) as fraction of 2*PI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Direction of the prevailing wind as a fraction of a full circle (0 = East, 0.25 = North, 0.5 = West, 0.75 = South). Determines which side of mountains receives rain and which side falls in the rain shadow."))
	float WindDirection = 0.0f;

	/** How much rivers/lakes boost local moisture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moisture", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Moisture bonus applied to pixels near rivers and lakes. Higher values create lush green corridors along waterways, potentially shifting nearby biomes from dry to wet."))
	float WaterMoistureBoost = 0.3f;

	/** Noise frequency for climate variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.1", ClampMax = "5.0", Tooltip = "Frequency of the noise field that adds local climate variation on top of the latitude-based temperature/moisture model. Higher values create more patchy climate zones."))
	float ClimateNoiseFrequency = 1.0f;

	/** Noise amplitude for climate variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise", meta = (ClampMin = "0.0", ClampMax = "0.3", Tooltip = "Amplitude of the climate variation noise. Higher values make local temperature and moisture deviate more from the smooth latitude model, creating unexpected biome pockets."))
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.3", ClampMax = "1.0", Tooltip = "Normalised elevation above which terrain is classified as Mountain biome. Lower values extend mountains further down the elevation range; higher values restrict mountains to the tallest peaks."))
	float MountainElevationThreshold = 0.72f;

	/** Temperature below which Snow/Tundra is assigned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Normalised temperature below which terrain is classified as Snow or Tundra. Higher values expand snow coverage to warmer areas; lower values restrict snow to the coldest peaks and poles."))
	float SnowTemperatureThreshold = 0.18f;

	/** Temperature below which Ice/Glacier is assigned (colder than snow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.3", Tooltip = "Normalised temperature below which terrain is classified as Ice or Glacier (must be colder than the Snow threshold). Extremely cold areas with sufficient moisture form permanent ice sheets."))
	float IceTemperatureThreshold = 0.12f;

	/** Minimum moisture required for Ice classification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Minimum normalised moisture required for ice/glacier formation. Even very cold terrain stays as tundra or bare rock if moisture is below this threshold, simulating dry polar deserts."))
	float IceMoistureThreshold = 0.3f;

	/** Temperature above which Desert is possible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.3", ClampMax = "1.0", Tooltip = "Normalised temperature above which Desert biome becomes possible (if moisture is also low enough). Lower values allow deserts in cooler regions; higher values restrict deserts to the hottest zones."))
	float DesertTemperatureThreshold = 0.6f;

	/** Moisture below which Desert is assigned (when hot enough) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Normalised moisture below which Desert biome is assigned (when the temperature threshold is also met). Higher values expand desert coverage into semi-arid zones."))
	float DesertMoistureThreshold = 0.25f;

	/** Moisture above which Forest is assigned */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.2", ClampMax = "1.0", Tooltip = "Normalised moisture above which Forest biome is assigned (when temperature and precipitation thresholds are also met). Lower values allow forests in drier areas; higher values restrict forests to the wettest zones."))
	float ForestMoistureThreshold = 0.5f;

	/** Minimum temperature for Forest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.1", ClampMax = "0.6", Tooltip = "Minimum normalised temperature required for Forest classification. Below this threshold, cold but moist terrain becomes Tundra instead of Forest."))
	float ForestTemperatureThreshold = 0.3f;

	/** Radius around volcanic hotspots classified as Volcanic biome (pixels) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "5", ClampMax = "100", Tooltip = "Radius in pixels around each volcanic hotspot centre that is classified as Volcanic biome. Overrides other biome rules within this zone. Larger values create wider volcanic wastelands."))
	int32 VolcanicRadiusPixels = 30;

	/** Slope above which terrain may shift from Forest→Mountain (0-1, as elevation difference between neighbors) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Normalised slope threshold above which terrain is reclassified from Forest to Mountain/Rocky. Prevents unrealistically dense forest on near-vertical cliff faces."))
	float SteepSlopeThreshold = 0.15f;

	/** Minimum precipitation to count as wet enough for Forest override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Minimum normalised precipitation required for the Forest biome override. Areas with enough moisture but insufficient rainfall become Grassland instead of Forest."))
	float ForestPrecipitationThreshold = 0.3f;

	/** Distance-to-water (pixels) below which moisture bonus is applied.
	 *  Pixels within this range receive a moisture nudge, potentially shifting Desert→Land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proximity", meta = (ClampMin = "0", ClampMax = "64", Tooltip = "Radius in pixels around rivers and lakes within which a moisture bonus is applied. Pixels inside this zone may shift from Desert to Grassland or from Grassland to Forest."))
	int32 WaterProximityRadiusPixels = 16;

	/** Moisture bonus added for pixels within WaterProximityRadiusPixels of a river/lake */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Water Proximity", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Normalised moisture bonus added to pixels within the water proximity radius. Higher values create wider green corridors along rivers in otherwise dry terrain."))
	float WaterProximityMoistureBonus = 0.15f;

	// --- Terrain archetype thresholds (used by the three-tier classification) ---

	/** Elevation slope threshold separating Plains from Hills (as max 8-connected elevation diff) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype Thresholds", meta = (ClampMin = "0.01", ClampMax = "0.3", Tooltip = "Normalised slope above which low-elevation terrain is classified as Hills rather than Plains. Lower values create more hill coverage; higher values restrict hills to noticeably rolling terrain."))
	float HillSlopeThreshold = 0.04f;

	/** Minimum plateau mask strength for Plateaus archetype classification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype Thresholds", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Minimum value in the PlateauMap required for a pixel to be classified as the Plateaus terrain archetype. Higher values restrict plateaus to only the most well-defined flat-topped regions."))
	float PlateauArchetypeThreshold = 0.3f;

	// --- Surface overlay thresholds ---

	/** Moisture above which Wetlands surface overlay is assigned (when near water) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Overlay Thresholds", meta = (ClampMin = "0.2", ClampMax = "1.0", Tooltip = "Normalised effective moisture above which terrain near water is classified with a Wetlands surface overlay. Wetlands require both high moisture and water proximity."))
	float WetlandsMoistureThreshold = 0.7f;

	/** Maximum water distance (pixels) for Wetlands classification */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Overlay Thresholds", meta = (ClampMin = "0", ClampMax = "64", Tooltip = "Maximum distance in pixels from a river or lake for a pixel to be eligible for Wetlands classification. Combined with the WetlandsMoistureThreshold."))
	int32 WetlandsMaxWaterDistance = 12;

	/** Temperature above which DesertScrub surface overlay is assigned (when not fully arid) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Overlay Thresholds", meta = (ClampMin = "0.2", ClampMax = "1.0", Tooltip = "Normalised temperature above which terrain with moderate moisture receives a DesertScrub surface overlay instead of bare Desert. DesertScrub is the semi-arid fringe around true deserts."))
	float DesertScrubTemperatureThreshold = 0.55f;

	/** Moisture range for DesertScrub: below DesertMoistureThreshold but above this value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Overlay Thresholds", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Normalised moisture below which DesertScrub is assigned instead of Grassland (when temperature is above DesertScrubTemperatureThreshold). Must be less than or equal to DesertMoistureThreshold + margin."))
	float DesertScrubMoistureMax = 0.35f;

	/** Biome blend radius in pixels for producing smooth blending weights at boundaries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blending", meta = (ClampMin = "0", ClampMax = "32", Tooltip = "Radius in pixels over which biome boundaries are blended. Larger values produce smoother, more gradual biome transitions; 0 creates hard biome edges."))
	int32 BiomeBlendRadius = 8;

	// --- Target Percentage Control ---

	/** Enable target-percentage biome distribution. Overrides threshold-based
	    classification so each biome covers approximately its target share of
	    land area. Volcanic and Ocean assignments are not affected.
	    Land not claimed by any target becomes Grassland. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Percentages", meta = (Tooltip = "Enable percentage-based biome distribution. When enabled, the system ranks pixels by affinity and assigns biomes to achieve the configured target coverage percentages. When disabled, raw threshold-based classification is used. Volcanic and Ocean are always unaffected. Land not claimed by any biome target becomes Grassland."))
	bool bEnableBiomeTargets = true;

	/** Target Forest coverage as a percentage of total land area */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Percentages", meta = (ClampMin = "0.0", ClampMax = "60.0", EditCondition = "bEnableBiomeTargets", Tooltip = "Desired Forest coverage as a percentage of total land pixels. The most forest-suitable pixels (high moisture, warm temperature, adequate precipitation) are selected up to this target. Remaining land becomes Grassland."))
	float TargetForestPercent = 25.0f;

	/** Target Desert coverage as a percentage of total land area */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Percentages", meta = (ClampMin = "0.0", ClampMax = "40.0", EditCondition = "bEnableBiomeTargets", Tooltip = "Desired Desert coverage as a percentage of total land pixels. The hottest and driest pixels are selected up to this target."))
	float TargetDesertPercent = 15.0f;

	/** Target Snow/Tundra coverage as a percentage of total land area (includes Ice) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Target Percentages", meta = (ClampMin = "0.0", ClampMax = "30.0", EditCondition = "bEnableBiomeTargets", Tooltip = "Desired Snow and Tundra coverage as a percentage of total land pixels. The coldest pixels are selected. Within this budget, pixels that are cold and moist enough are promoted to Ice/Glacier."))
	float TargetSnowPercent = 10.0f;

	// --- Spatial Smoothing ---

	/** Number of majority-vote smoothing passes applied to the biome map.
	    Each pass replaces a pixel with the most common biome in its
	    neighbourhood, eliminating thin stripe artefacts along threshold
	    boundaries. 0 = no smoothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spatial Smoothing", meta = (ClampMin = "0", ClampMax = "8", Tooltip = "Number of majority-vote smoothing passes applied to the biome map after classification. Each pass replaces every pixel with the most common biome in a 5×5 neighbourhood window, eliminating thin stripe artefacts that form along elevation or temperature contour lines. Higher values produce broader, more cohesive biome regions. 0 disables smoothing."))
	int32 BiomeSmoothingPasses = 4;

	// --- Cluster Filtering ---

	/** Minimum contiguous biome region size in pixels. Patches smaller than this
	    are absorbed into the most common surrounding biome. 0 = no filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cluster Filtering", meta = (ClampMin = "0", ClampMax = "512", Tooltip = "Minimum contiguous area in pixels for a biome patch to survive. Smaller patches are absorbed into the dominant neighbouring biome, producing cleaner, more readable biome boundaries. Set to 0 to disable."))
	int32 MinBiomeClusterSize = 200;

	/** Per-biome material & foliage spawn rules (8 entries, indexed by EBiomeType) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material Rules", meta = (Tooltip = "Array of material and foliage spawn rules, one per biome type (indexed by EBiomeType). Controls the visual appearance and vegetation of each biome."))
	TArray<FBiomeMaterialRules> BiomeMaterialRules;
};

/**
 * Stage 7: Terrain refinement settings — biome-local detail noise on top of macro terrain.
 */
USTRUCT(BlueprintType)
struct RANDOMLANDSCAPE_5_7_API FTerrainRefinementSettings
{
	GENERATED_BODY()

	/** Seed override (0 = derive from global seed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "0", Tooltip = "Per-stage random seed. Leave at 0 to derive automatically from the global world seed. Set a specific value to lock this stage's output while experimenting with other stages."))
	int32 Seed = 0;

	/** Overall scale of biome detail noise (0 = no detail, 1 = full) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Global amplitude of the biome-specific detail noise added on top of the macro terrain. 0 disables all detail; higher values add more fine-grained bumps and texture to every biome."))
	float DetailScale = 0.08f;

	/** Noise frequency for detail features */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "0.5", ClampMax = "20.0", Tooltip = "Base noise frequency for biome detail features. Higher values add smaller, more tightly packed detail; lower values produce broader undulations. Per-biome FrequencyMultiplier scales this."))
	float DetailFrequency = 4.0f;

	/** Noise octaves for detail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refinement", meta = (ClampMin = "1", ClampMax = "6", Tooltip = "Base number of noise octaves for biome detail. More octaves add finer layers of detail on top of the large-scale noise. Per-biome ExtraOctaves adds to this."))
	int32 DetailOctaves = 3;

	/** Desert dune frequency */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desert", meta = (ClampMin = "1.0", ClampMax = "20.0", Tooltip = "Noise frequency specifically for desert sand dune features. Higher values create more frequent, tightly packed dune ridges; lower values produce broad, rolling dune fields."))
	float DuneFrequency = 8.0f;

	/** Desert dune height relative to detail scale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Desert", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Height of desert dune features as a fraction of the global DetailScale. 0 disables dunes; 1 makes dune crests as tall as the full detail amplitude."))
	float DuneHeight = 0.5f;

	/** Mountain extra ridge detail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountain", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Additional ridged noise amplitude applied exclusively to Mountain biome pixels. Adds craggy, jagged detail on top of the macro mountain shape."))
	float MountainDetailScale = 0.08f;

	/** Number of smoothing passes on the final elevation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post-Process", meta = (ClampMin = "0", ClampMax = "10", Tooltip = "Number of box-blur smoothing passes applied to the final elevation map after all detail is added. Softens any remaining sharp edges between biomes. 0 skips smoothing entirely."))
	int32 FinalSmoothingPasses = 3;

	/** Per-biome terrain refinement profiles (8 entries, indexed by EBiomeType).
	 *  If empty, hard-coded defaults are used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Per-Biome", meta = (Tooltip = "Array of per-biome terrain profiles (indexed by EBiomeType). Each entry customises the noise character for a single biome. Leave empty to use hard-coded defaults."))
	TArray<FPerBiomeTerrainProfile> BiomeProfiles;
};
