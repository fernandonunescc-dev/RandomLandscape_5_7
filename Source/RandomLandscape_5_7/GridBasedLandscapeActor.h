// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "LandscapeNoiseGenerator.h"
#include "LandscapeGridManager.h"
#include "BiomeClassifier.h"
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
	/** Total landscape size in meters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Configuration")
	float MapSize = 100.0f;

	/** Size of each grid cell in meters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Configuration")
	float GridScale = 10.0f;

	// ===== Height Configuration =====
	/** Maximum height variation in Unreal units (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Height")
	float MaxHeightVariation = 10000.0f;

	/** Offset applied to all heights */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Height")
	float HeightOffset = 0.0f;

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
};
