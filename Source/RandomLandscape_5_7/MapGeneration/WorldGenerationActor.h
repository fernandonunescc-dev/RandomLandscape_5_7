// WorldGenerationActor.h
// Geology-driven world generation pipeline orchestrator

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "LandmassGenerator.h"
#include "BiomeTypes.h"
#include "WorldGenTypes.h"
#include "WorldGenerationActor.generated.h"

/**
 * Geology-driven world generation pipeline orchestrator.
 * Replaces BiomeDataGenerationActor with a dependency-driven approach.
 *
 * Pipeline stages (each depends on all previous stages):
 *   1. Landmass - Land/ocean mask
 *   2. Uplift/Geology - Base elevation + tectonic uplift + volcanic hotspots
 *   3. Hydrology - Rivers and lakes from drainage simulation
 *   4. Erosion - River incision (canyons) + thermal erosion
 *   5. Climate - Temperature, moisture, precipitation from terrain
 *   6. Biomes - Classification from climate + terrain rules
 *   7. Terrain Refinement - Biome-local detail on macro terrain
 *   8. Mesh - Procedural mesh from final data
 *
 * Every stage outputs debug textures for visualization.
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API AWorldGenerationActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldGenerationActor();

	// ==================== Quick Presets ====================

	/** When true, property changes auto-regenerate the affected pipeline stages and update the mesh. Use lower TextureResolution (128-256) for faster preview. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (Tooltip = "Enable live preview: property changes auto-regenerate the affected pipeline stages and update the mesh. Use a lower TextureResolution for faster feedback."))
	bool bAutoRegenerate = false;

	/** Add a volcanic hotspot. Toggles volcano generation with sensible defaults in the Uplift settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (Tooltip = "Add a volcanic hotspot to the island. Enables volcano generation with sensible defaults that you can then fine-tune in the Uplift settings."))
	bool bIncludeVolcano = true;

	/** Add tectonic mountain ridges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (Tooltip = "Add mountain ridges across the terrain. Enables mountain generation with sensible defaults."))
	bool bIncludeMountains = true;

	/** Add rolling hills. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (Tooltip = "Add rolling hills across the terrain. Disabling sets hill amplitude to zero."))
	bool bIncludeHills = true;

	/** Add flat-topped plateau regions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (Tooltip = "Add flat-topped plateau regions. Disabling sets plateau flatness to zero."))
	bool bIncludePlateaus = true;

	/** Generate rivers, lakes and waterfalls from drainage simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (Tooltip = "Generate rivers, lakes, and waterfalls from drainage simulation."))
	bool bIncludeRivers = true;

	/** Enable river-carved canyons in the erosion stage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (Tooltip = "Enable river-carved canyons during the erosion stage."))
	bool bIncludeCanyons = true;

	/** Overall terrain roughness: 0 = mostly flat with gentle hills, 1 = heavily mountainous. Adjusts mountain amplitude, hill amplitude, and base elevation noise in the background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Quick Presets", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "Master slider controlling how flat or mountainous the terrain is. At 0 the island is mostly flat coastal plains with gentle hills. At 1 the terrain is dominated by tall mountain ridges. This adjusts MountainRidgeAmplitude, HillAmplitude, BaseNoisePersistence, and CoastlineGradientWidth behind the scenes."))
	float TerrainRoughness = 0.3f;

	// ==================== Pipeline Global Settings ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global")
	int32 GlobalSeed = 42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 TextureResolution = 512;

	/** Target land area in square kilometres. The world dimensions adapt to fit the land + ocean padding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "0.01", ClampMax = "64.0", UIMin = "0.01", UIMax = "16.0"))
	float TargetLandAreaSqKm = 0.25f;

	/** Width of the ocean border around the generated land in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "20.0", ClampMax = "2000.0"))
	float OceanPaddingMeters = 200.0f;

	/** Maximum terrain height in meters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "10.0", ClampMax = "2000.0"))
	float MaxMapHeight = 100.0f;

	/** Ocean depth in meters — ocean pixels are placed this far below sea level */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float OceanLevel = 15.0f;

	/** Configurable sea level as a normalised elevation threshold [0,1].
	 *  Land pixels below this value are treated as near- or below-sea-level
	 *  terrain during classification.  Does NOT change the land/ocean mask —
	 *  that is determined by Stage 1 (Landmass).  Default 0 means the
	 *  land/ocean boundary IS sea level (traditional behaviour). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "0.0", ClampMax = "0.5", Tooltip = "Normalised sea-level elevation threshold. Land pixels below this elevation are considered near sea level for classification purposes. 0 means the land/ocean boundary is sea level (default behaviour)."))
	float SeaLevel = 0.0f;

	// ==================== Per-Stage Settings ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|1-Landmass")
	FLandmassSettings LandmassSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|2-Uplift")
	FUpliftSettings UpliftSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|3-Hydrology")
	FHydrologySettings HydrologySettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|4-Erosion")
	FErosionSettings ErosionSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|5-Climate")
	FClimateSettings ClimateSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|6-Biomes")
	FBiomeAssignmentSettings BiomeAssignmentSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|7-Refinement")
	FTerrainRefinementSettings RefinementSettings;

	// ==================== Debug Textures ====================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|1-Landmass")
	TObjectPtr<UTexture2D> Debug_Landmass = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|2-Uplift")
	TObjectPtr<UTexture2D> Debug_BaseElevation = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|2-Uplift")
	TObjectPtr<UTexture2D> Debug_UpliftMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|2-Uplift")
	TObjectPtr<UTexture2D> Debug_CombinedElevation = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|2-Uplift")
	TObjectPtr<UTexture2D> Debug_PlateauMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|3-Hydrology")
	TObjectPtr<UTexture2D> Debug_Rivers = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|3-Hydrology")
	TObjectPtr<UTexture2D> Debug_Lakes = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|3-Hydrology")
	TObjectPtr<UTexture2D> Debug_Waterfalls = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|3-Hydrology")
	TObjectPtr<UTexture2D> Debug_FlowAccumulation = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|4-Erosion")
	TObjectPtr<UTexture2D> Debug_ErodedElevation = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|4-Erosion")
	TObjectPtr<UTexture2D> Debug_CanyonMask = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|4-Erosion")
	TObjectPtr<UTexture2D> Debug_ErosionDelta = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|5-Climate")
	TObjectPtr<UTexture2D> Debug_Temperature = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|5-Climate")
	TObjectPtr<UTexture2D> Debug_Moisture = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|5-Climate")
	TObjectPtr<UTexture2D> Debug_Precipitation = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|6-Biomes")
	TObjectPtr<UTexture2D> Debug_BiomeMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|6-Biomes")
	TObjectPtr<UTexture2D> Debug_SlopeMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|6-Biomes")
	TObjectPtr<UTexture2D> Debug_BiomeBlendWeights = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|6-Biomes")
	TObjectPtr<UTexture2D> Debug_TerrainArchetypeMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|6-Biomes")
	TObjectPtr<UTexture2D> Debug_SurfaceOverlayMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|6-Biomes")
	TObjectPtr<UTexture2D> Debug_GeneratedFeatureMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|7-Refinement")
	TObjectPtr<UTexture2D> Debug_FinalElevation = nullptr;

	// ==================== Debug Info ====================

	UPROPERTY(VisibleAnywhere, Category = "Debug|Info")
	int32 ActualSeedUsed = 0;

	UPROPERTY(VisibleAnywhere, Category = "Debug|Info")
	int32 ResolutionUsed = 0;

	// ==================== Mesh Component ====================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	// ==================== Editor Buttons ====================

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Step1_GenerateLandmass();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Step2_GenerateUplift();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Step3_GenerateHydrology();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Step4_GenerateErosion();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Step5_GenerateClimate();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Step6_GenerateBiomes();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Step7_GenerateRefinement();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void GenerateAll();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void GenerateMesh();

	/** Pick a new random seed, run all pipeline stages, and build the mesh. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void Randomize();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void ClearAll();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void ValidateTerrain();

	// ==================== Validation Result ====================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|Validation")
	FTerrainValidationResult LastValidationResult;

	// ==================== Auto-Regeneration ====================

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	// ==================== Cached Pipeline Data ====================

	TArray<uint8> CachedLandMask;
	TArray<float> CachedBaseElevation;
	TArray<float> CachedUpliftMap;
	TArray<float> CachedCombinedElevation;
	TArray<float> CachedRiverMap;
	TArray<float> CachedErodedElevation;
	TArray<uint8> CachedCanyonMask;
	TArray<float> CachedErosionDeltaMap;
	TArray<float> CachedTemperature;
	TArray<float> CachedMoisture;
	TArray<float> CachedPrecipitation;
	TArray<float> CachedFinalElevation;
	TArray<uint8> CachedLakeMap;
	TArray<uint8> CachedWaterfallMap;
	TArray<float> CachedFlowAccumulation;
	TArray<int32> CachedFlowDirection;
	TArray<float> CachedPlateauMap;
	TArray<int32> CachedBiomeMap;
	TArray<int32> CachedTerrainArchetypeMap;
	TArray<int32> CachedSurfaceOverlayMap;
	TArray<int32> CachedGeneratedFeatureMap;
	TArray<float> CachedSlopeMap;
	/** Flat array: pixel i → [i*8 .. i*8+7], one weight per EBiomeType. */
	TArray<float> CachedBiomeBlendWeights;
	TArray<FVector2D> CachedVolcanicCenters;

	/** World size in centimeters computed by the landmass generator */
	float CachedWorldSizeCm = 100000.0f;

	// ==================== Helpers ====================

	UTexture2D* CreateGrayscaleDebugTexture(int32 Res, const TArray<float>& Data);
	UTexture2D* CreateColorDebugTexture(int32 Res, const TArray<FColor>& PixelData);

	/** Copy global pipeline settings into LandmassSettings before generation */
	void SyncLandmassSettings();

	/** Build terrain mesh from the given elevation data, optionally colored by biome map */
	void BuildTerrainMesh(const TArray<float>& Elevation, const TArray<int32>* BiomeMap);

#if WITH_EDITOR
	/** Re-run pipeline from the given stage (1-7) through the end, then rebuild the mesh */
	void RegenerateFromStage(int32 StageIndex);

	/** Apply quick preset defaults to the underlying settings structs */
	void ApplyPreset_Volcano(bool bEnable);
	void ApplyPreset_Mountains(bool bEnable);
	void ApplyPreset_Hills(bool bEnable);
	void ApplyPreset_Plateaus(bool bEnable);
	void ApplyPreset_Rivers(bool bEnable);
	void ApplyPreset_Canyons(bool bEnable);
	void ApplyTerrainRoughness(float Roughness);
#endif
};
