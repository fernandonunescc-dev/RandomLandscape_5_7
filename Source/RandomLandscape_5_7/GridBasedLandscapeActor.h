// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "LandscapeNoiseGenerator.h"
#include "LandscapeGridManager.h"
#include "BiomeClassifier.h"
#include "Async/AsyncWork.h"
#include "Async/ParallelFor.h"
#include "GridBasedLandscapeActor.generated.h"

/**
 * Grid-based procedural landscape actor with per-cell height maps and biome support
 * Generates detailed terrain with multiple biome types (plains, mountains, cliffs, canyons, oceans, lakes)
 */
UCLASS()
class RANDOMLANDSCAPE_5_7_API AGridBasedLandscapeActor : public AActor
{
	GENERATED_BODY()

public:
	AGridBasedLandscapeActor();

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;

	// Constants
	static constexpr float METERS_TO_UNREAL_UNITS = 100.0f;
	static constexpr int32 VERTICES_PER_CELL = 1024;	// 32x32 grid per cell

	// ===== Map Configuration =====
	/** Total landscape size in cells (grid count per side, e.g., 10 = 10x10 grid) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Configuration", meta = (UIMin = "1", UIMax = "100", ClampMin = "1", ClampMax = "100"))
	int32 MapGridCount = 10;

	/** Size of each grid cell in meters (e.g., 10 = 10m x 10m cells) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Configuration", meta = (UIMin = "1", UIMax = "100", ClampMin = "1", ClampMax = "100"))
	int32 CellSizeMeters = 10;

	// ===== Height Configuration =====
	/** Maximum height variation in Unreal units (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Height")
	float MaxHeightVariation = 10000.0f;

	/** Offset applied to all heights */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Height")
	float HeightOffset = 0.0f;

	// ===== Noise Configuration (exposed for tweaking) =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise")
	float PrimaryNoiseFrequency = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise")
	int32 PrimaryNoiseOctaves = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise")
	float SecondaryNoiseFrequency = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise")
	int32 SecondaryNoiseOctaves = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise")
	float TertiaryNoiseFrequency = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise")
	int32 TertiaryNoiseOctaves = 2;

	// ===== Water Configuration =====
	/** Height at which water level is placed (ocean/lakes below this get water planes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Water")
	float WaterLevel = 0.0f;

	/** Material for water planes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Water")
	UMaterialInterface* WaterMaterial;

	/** Whether to generate water planes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Water")
	bool bGenerateWaterPlanes = true;

	// ===== Rendering Configuration =====
	/** Material to use for terrain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Rendering")
	UMaterialInterface* TerrainMaterial;

	/** Biome-specific materials (map of biome type name to material) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Rendering")
	TMap<FString, UMaterialInterface*> BiomeMaterials;

	// ===== Streaming Configuration =====
	/** Number of cells to load around the player camera (e.g., 2 = 5x5 grid) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Streaming")
	int32 StreamingLoadRadius = 2;

	/** Maximum cells to keep in memory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Streaming")
	int32 MaxCachedCells = 25;

	// ===== Optimization Settings =====
	/** Generate terrain on background thread (keeps UI/gameplay responsive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Optimization")
	bool bAsyncGeneration = false;

	/** Number of cells to generate per frame on main thread (if not async) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Optimization", 
		meta = (UIMin = "1", UIMax = "100", ClampMin = "1", ClampMax = "100"))
	int32 CellsPerFrame = 20;

	// ===== Debug Options =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Debug")
	bool bDebugVisualizeBiomes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Debug")
	bool bDebugShowStats = false;

	// ===== Blueprint Functions =====
	UFUNCTION(BlueprintCallable, Category = "Landscape|Generation")
	void GenerateLandscape();

	UFUNCTION(BlueprintCallable, Category = "Landscape|Generation")
	void ClearLandscape();

	UFUNCTION(BlueprintCallable, Category = "Landscape|Streaming")
	void UpdateStreamingAroundPoint(const FVector& InCenterPoint);

	UFUNCTION(BlueprintCallable, Category = "Landscape|Debug")
	void PrintDebugStats();

protected:
	// ===== Internal Components =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape")
	TObjectPtr<USceneComponent> RootScene;

	// Map of grid coordinates to procedural mesh components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape")
	TMap<FString, UProceduralMeshComponent*> GridMeshes;

	// Water plane actors (managed separately)
	UPROPERTY()
	TArray<AActor*> WaterPlanes;

	// ===== Internal Systems =====
	FLandscapeNoiseGenerator NoiseGenerator;
	FLandscapeGridManager GridManager;
	FBiomeClassifier BiomeClassifier;

	// ===== Property Change Tracking =====
	int32 CachedMapGridCount = 10;
	int32 CachedCellSizeMeters = 10;
	float CachedMaxHeightVariation = 10000.0f;
	float CachedHeightOffset = 0.0f;
	float CachedPrimaryNoiseFrequency = 0.001f;
	int32 CachedPrimaryNoiseOctaves = 3;
	float CachedSecondaryNoiseFrequency = 0.01f;
	int32 CachedSecondaryNoiseOctaves = 4;
	float CachedTertiaryNoiseFrequency = 0.05f;
	int32 CachedTertiaryNoiseOctaves = 2;
	bool bHasTerrainChanged = true;

	// ===== Async Generation State =====
	TArray<TPair<int32, int32>> PendingCellsToGenerate;	// Queue of cells waiting to be generated
	bool bIsGeneratingAsync = false;						// Currently generating cells on background thread
	FThreadSafeBool bAsyncGenerationComplete = false;		// Background thread finished?

	// ===== Internal Methods =====
private:
	/**
	 * Generate all visible grid cells
	 */
	void GenerateLandscapeInternal();

	/**
	 * Generate a single grid cell's mesh
	 */
	void GenerateGridCell(int32 InGridX, int32 InGridY);

	/**
	 * Create mesh section for a cell from height map data
	 */
	void CreateCellMesh(FGridCellData* InCellData, UProceduralMeshComponent* InMeshComponent);

	/**
	 * Generate water planes for cells below water level
	 */
	void GenerateWaterPlanes();

	/**
	 * Get or create a mesh component for a grid cell
	 */
	UProceduralMeshComponent* GetOrCreateCellMesh(int32 InGridX, int32 InGridY);

	/**
	 * Convert world position to grid cell coordinates
	 */
	void GetGridCellFromWorldPosition(const FVector& InWorldPos, int32& OutGridX, int32& OutGridY);

	/**
	 * Apply biome-specific material based on height
	 */
	void ApplyBiomeMaterial(UProceduralMeshComponent* InMesh, EBiomeType InBiomeType);

	/**
	 * Update noise generator settings from properties
	 */
	void UpdateNoiseSettings();

	/**
	 * Get grid cell key for the map
	 */
	FString GetGridCellKey(int32 InGridX, int32 InGridY) const;

	/**
	 * Check if terrain-relevant properties have changed
	 * Returns true if regeneration is needed
	 */
	bool HasTerrainPropertiesChanged();

private:
	/**
	 * Queue cells for generation (either async or progressive on main thread)
	 */
	void QueueCellsForGeneration();

	/**
	 * Process queued cells on main thread (for non-async generation)
	 */
	void ProcessQueuedCells();

	/**
	 * Start async generation on background thread
	 */
	void StartAsyncGeneration();

	/**
	 * Called when async generation completes - applies results to main thread
	 */
	void OnAsyncGenerationComplete();
};
