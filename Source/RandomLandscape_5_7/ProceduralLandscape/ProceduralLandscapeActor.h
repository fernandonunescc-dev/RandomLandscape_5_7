// Copyright Fernando Araujo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "LandscapeTypes.h"
#include "LandscapeNoiseService.h"
#include "LandscapeAsyncGenerator.h"
#include "ProceduralLandscapeActor.generated.h"

/**
 * Simple procedural landscape actor.
 * Generates a terrain mesh using fractal noise.
 * 
 * Configuration:
 *   - MapSizeMeters: How big the terrain is
 *   - MaxHeightMeters: How tall the terrain can be
 *   - Resolution: How detailed the mesh is (1-100)
 *   - NoiseFrequency: How "zoomed in" the noise is (lower = larger features)
 *   - NoiseOctaves: How much detail (more = more detail)
 *   - TerrainSeed: Random seed (same seed = same terrain)
 */
UCLASS()
class RANDOMLANDSCAPE_5_7_API AProceduralLandscapeActor : public AActor
{
	GENERATED_BODY()

public:
	AProceduralLandscapeActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;

	// =========================================================================
	// MAP CONFIGURATION
	// =========================================================================
	
	/** Map size and resolution settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Map")
	FLandscapeResolutionConfig Resolution;

	// =========================================================================
	// NOISE CONFIGURATION
	// =========================================================================

	/** Random seed for terrain generation. Same seed = same terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise")
	int32 TerrainSeed = 1337;

	/** 
	 * Noise frequency. Lower values = larger terrain features.
	 * Try: 0.001 (huge hills), 0.005 (medium), 0.01 (small bumps)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise",
		meta = (UIMin = "0.0001", UIMax = "0.1", ClampMin = "0.00001", ClampMax = "1.0"))
	float NoiseFrequency = 0.003f;

	/** 
	 * Number of noise octaves. More octaves = more detail.
	 * Try: 3 (smooth), 5 (normal), 8 (very detailed)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise",
		meta = (UIMin = "1", UIMax = "10", ClampMin = "1", ClampMax = "12"))
	int32 NoiseOctaves = 5;

	/** Frequency multiplier between octaves. Usually 2.0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise",
		meta = (UIMin = "1.5", UIMax = "3.0", ClampMin = "1.0", ClampMax = "4.0"))
	float NoiseLacunarity = 2.0f;

	/** Amplitude multiplier between octaves. Usually 0.5 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise",
		meta = (UIMin = "0.3", UIMax = "0.7", ClampMin = "0.1", ClampMax = "1.0"))
	float NoiseGain = 0.5f;

	/**
	 * Height distribution exponent. Controls how rare tall peaks are.
	 * 1.0 = Uniform (all heights equally likely)
	 * 2.0 = Most terrain is low hills, tall peaks are rarer
	 * 3.0 = Mostly flat with occasional dramatic mountains
	 * 4.0+ = Very flat with rare extreme peaks
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Noise",
		meta = (UIMin = "1.0", UIMax = "5.0", ClampMin = "0.5", ClampMax = "10.0"))
	float HeightExponent = 1.0f;

	// =========================================================================
	// ISLAND EDGE SETTINGS
	// =========================================================================

	/** 
	 * Enable island mode - terrain will be circular with edges dropping 
	 * down to below sea level, creating an island surrounded by water.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Island")
	bool bIslandMode = true;

	/**
	 * Controls where the edge falloff begins (0-1).
	 * This determines the size of the flat "core" area vs the sloping edges:
	 * 
	 * 0.3 = Large flat center (70%), narrow edge falloff
	 * 0.5 = Balanced - falloff covers half the island radius
	 * 0.7 = Small flat center, gradual slopes across most of island
	 * 1.0 = Dome shape - falloff starts from center
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Island",
		meta = (UIMin = "0.2", UIMax = "1.0", ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bIslandMode"))
	float IslandDepth = 0.5f;

	/**
	 * (Legacy - not used in current circular mode)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Island",
		meta = (UIMin = "1", UIMax = "200", ClampMin = "0", ClampMax = "500", EditCondition = "bIslandMode"))
	float EdgeFalloffDistance = 50.0f;

	/**
	 * Height of the sea level / water surface (in meters).
	 * Terrain below this level appears as water.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Island",
		meta = (UIMin = "-100", UIMax = "100", ClampMin = "-1000", ClampMax = "1000", EditCondition = "bIslandMode"))
	float SeaLevel = 0.0f;

	/**
	 * Depth of the skirt below sea level (in meters).
	 * The skirt creates vertical walls around the map edges to close the mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Island",
		meta = (UIMin = "1", UIMax = "100", ClampMin = "0.1", ClampMax = "500", EditCondition = "bIslandMode"))
	float SkirtDepth = 50.0f;

	/**
	 * Controls the edge falloff curve shape.
	 * < 1.0 = Gentle gradual slopes
	 * = 1.0 = Smooth natural falloff  
	 * = 1.5 = Nice balanced cliffs (recommended)
	 * > 2.0 = Terrain stays high, then drops sharply at edges
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Island",
		meta = (UIMin = "0.5", UIMax = "3.0", ClampMin = "0.1", ClampMax = "5.0", EditCondition = "bIslandMode"))
	float EdgeFalloffExponent = 1.5f;

	// =========================================================================
	// MATERIAL
	// =========================================================================

	/** Material to apply to the terrain */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Material")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	// =========================================================================
	// GENERATION SETTINGS
	// =========================================================================

	/** Use background threads for generation (recommended for runtime) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Generation")
	bool bUseAsyncGeneration = true;

	/** Auto-regenerate in editor when properties change */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Generation")
	bool bAutoRegenerateInEditor = true;

	// =========================================================================
	// DEBUG
	// =========================================================================

	/** Show generation stats on screen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape|Debug")
	bool bDebugShowStats = false;

	// =========================================================================
	// BLUEPRINT FUNCTIONS
	// =========================================================================

	/** Regenerate the entire landscape */
	UFUNCTION(BlueprintCallable, Category = "Landscape")
	void RegenerateLandscape();

	/** Clear all generated meshes */
	UFUNCTION(BlueprintCallable, Category = "Landscape")
	void ClearLandscape();

	/** Get total map size in meters */
	UFUNCTION(BlueprintCallable, Category = "Landscape")
	float GetMapSizeMeters() const { return Resolution.GetTotalMapSizeMeters(); }

	/** Get total triangle count */
	UFUNCTION(BlueprintCallable, Category = "Landscape")
	int32 GetTotalTriangleCount() const { return Resolution.GetTotalTriangles(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Landscape")
	TObjectPtr<USceneComponent> RootScene;

	/** Map of chunk coordinates to their mesh components */
	UPROPERTY()
	TMap<FIntPoint, UProceduralMeshComponent*> ChunkMeshes;

private:
	/** Noise generation service */
	TUniquePtr<FLandscapeNoiseService> NoiseService;

	/** Async generation manager */
	TUniquePtr<FLandscapeAsyncGenerator> AsyncGenerator;

	/** Cached settings for change detection */
	FLandscapeResolutionConfig CachedResolution;
	int32 CachedSeed = 0;
	float CachedNoiseFrequency = 0.0f;
	int32 CachedNoiseOctaves = 0;
	float CachedHeightExponent = 0.0f;
	
	/** Cached island settings */
	bool CachedIslandMode = false;
	float CachedIslandDepth = 0.0f;
	float CachedEdgeFalloffDistance = 0.0f;
	float CachedSeaLevel = 0.0f;
	float CachedSkirtDepth = 0.0f;
	float CachedEdgeFalloffExponent = 0.0f;

	/** Is generation currently in progress? */
	bool bIsGenerating = false;

	/** Number of chunks created so far */
	int32 ChunksCreated = 0;

	// =========================================================================
	// INTERNAL METHODS
	// =========================================================================

	void InitializeServices();
	void StartGeneration();
	void GenerateSynchronous();
	void ProcessCompletedChunks();
	void CreateChunkMesh(const FLandscapeChunkData& ChunkData);
	bool HasSettingsChanged() const;
	void CacheCurrentSettings();
	FVector2D GetMapOriginMeters() const;
	void PrintDebugStats();
};
