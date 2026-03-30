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

	// ==================== Pipeline Global Settings ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global")
	int32 GlobalSeed = 42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "64", ClampMax = "4096"))
	int32 TextureResolution = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global")
	EMapType MapType = EMapType::Continent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global")
	EMapSize MapSize = EMapSize::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "5.0", ClampMax = "75.0"))
	float LandCoveragePercent = 50.0f;

	/** Maximum terrain height in world units (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pipeline|Global", meta = (ClampMin = "1000.0", ClampMax = "200000.0"))
	float MaxMapHeight = 50000.0f;

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

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void ClearAll();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Pipeline|Actions")
	void ValidateTerrain();

	// ==================== Validation Result ====================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Debug|Validation")
	FTerrainValidationResult LastValidationResult;

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
	TArray<float> CachedSlopeMap;
	/** Flat array: pixel i → [i*8 .. i*8+7], one weight per EBiomeType. */
	TArray<float> CachedBiomeBlendWeights;
	TArray<FVector2D> CachedVolcanicCenters;

	// ==================== Helpers ====================

	UTexture2D* CreateGrayscaleDebugTexture(int32 Res, const TArray<float>& Data);
	UTexture2D* CreateColorDebugTexture(int32 Res, const TArray<FColor>& PixelData);

	/** Copy global pipeline settings into LandmassSettings before generation */
	void SyncLandmassSettings();

	/** Build terrain mesh from final elevation and biome data */
	void BuildTerrainMesh();
};
