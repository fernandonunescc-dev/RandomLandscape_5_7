// BiomeDataGenerationActor.h
// Manual editor-only actor for Landmass + Biome + Heightmap preview generation

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandmassGenerator.h"
#include "BiomeMapGenerator.h"
#include "HeightmapGenerator.h"
#include "LandscapeMeshActor.h"
#include "BiomeDataGenerationActor.generated.h"

/**
 * Editor-placeable actor that exposes landmass, biome, and heightmap generation settings.
 * Provides manual buttons to generate preview textures - nothing runs automatically.
 *
 * Three generation layers:
 *   Layer 1 - Landmass: Black/white texture (white=land, black=ocean)
 *   Layer 2 - Biomes: Color-coded biome distribution texture
 *   Layer 3 - Heightmaps: Per-biome grayscale height textures (white=tallest, black=lowest)
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API ABiomeDataGenerationActor : public AActor
{
	GENERATED_BODY()

public:
	ABiomeDataGenerationActor();

	// ==================== Input Settings ====================

	/** Landmass generation settings (map type, size, seed, resolution, coverage, domain warp, post-processing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Landmass")
	FLandmassSettings LandmassSettings;

	/** Biome layout settings (seed, layers with colors, terrain settings, spread type) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Biomes")
	FBiomeLayoutSettings BiomeLayoutSettings;

	/** Heightmap generation settings (seed, resolution, blend radius) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Heightmaps")
	FHeightmapSettings HeightmapSettings;

	// ==================== Output (Read-Only) ====================

	/** Generated black/white landmass preview (white = land, black = ocean) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Landmass")
	TObjectPtr<UTexture2D> LandmassPreviewTexture;

	/** Generated biome distribution preview (colored by biome type) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Biomes")
	TObjectPtr<UTexture2D> BiomePreviewTexture;

	/** Generated per-biome heightmap textures */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Heightmaps")
	TArray<FBiomeHeightmapResult> HeightmapResults;

	/** Actual seed used for landmass generation (may differ from input if input was 0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Landmass")
	int32 ActualLandmassSeedUsed = 0;

	/** Actual seed used for biome generation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Biomes")
	int32 ActualBiomeSeedUsed = 0;

	/** Actual seed used for heightmap generation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Heightmaps")
	int32 ActualHeightmapSeedUsed = 0;

	/** Texture resolution used for generation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Landmass")
	int32 TextureResolutionUsed = 0;

	// ==================== Editor Buttons ====================

	/** Generate landmass mask and preview texture */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void GenerateLandmass();

	/** Generate biome distribution on existing landmass */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void GenerateBiomes();

	/** Generate per-biome heightmap textures from existing biome data */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void GenerateHeightmaps();

	/** Generate all three layers in sequence: landmass -> biomes -> heightmaps */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void GenerateAll();

	/** Clear all generated data and reset outputs */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void ClearGeneratedData();

	/** Generate procedural terrain mesh from existing heightmap data */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void GenerateMesh();

	// ==================== Output (Mesh) ====================

	/** Spawned landscape mesh actor (auto-managed) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Mesh")
	TObjectPtr<ALandscapeMeshActor> SpawnedMeshActor;

	// ==================== Data Accessors ====================

	/** Get cached land mask (1 = land, 0 = ocean) */
	const TArray<uint8>& GetCachedLandMask() const { return CachedLandMask; }

	/** Get cached biome map (stores EBiomeType as int32 per pixel) */
	const TArray<int32>& GetCachedBiomeMap() const { return CachedBiomeMap; }

private:
	/** Cached land mask from last GenerateLandmass() call */
	TArray<uint8> CachedLandMask;

	/** Cached biome map from last GenerateBiomes() call */
	TArray<int32> CachedBiomeMap;

	/** Create a transient BGRA8 texture and populate with pixel data */
	UTexture2D* CreatePreviewTexture(int32 Resolution, const TArray<FColor>& PixelData);

	/** Build biome preview texture from cached data */
	void BuildBiomePreviewTexture();
};
