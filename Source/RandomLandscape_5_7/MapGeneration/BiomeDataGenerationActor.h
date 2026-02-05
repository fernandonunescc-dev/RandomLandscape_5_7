// BiomeDataGenerationActor.h
// Manual editor-only actor for Landmass + Biome preview generation
// Version: 02.04.2026.23.55

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LandmassGenerator.h"
#include "BiomeMapGenerator.h"
#include "BiomeDataGenerationActor.generated.h"

/**
 * Editor-placeable actor that exposes landmass and biome generation settings.
 * Provides manual buttons to generate preview textures - nothing runs automatically.
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API ABiomeDataGenerationActor : public AActor
{
	GENERATED_BODY()

public:
	ABiomeDataGenerationActor();

	// ==================== Input Settings ====================

	/** Landmass generation settings (seed, resolution, coverage, domain warp, post-processing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Landmass")
	FLandmassSettings LandmassSettings;

	/** Biome layout settings (seed, layers, colors) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generation|Biomes")
	FBiomeLayoutSettings BiomeLayoutSettings;

	// ==================== Output (Read-Only) ====================

	/** Generated black/white landmass preview (white = land, black = ocean) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Landmass")
	TObjectPtr<UTexture2D> LandmassPreviewTexture;

	/** Generated biome distribution preview (colored by biome type) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Biomes")
	TObjectPtr<UTexture2D> BiomePreviewTexture;

	/** Actual seed used for landmass generation (may differ from input if input was 0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Landmass")
	int32 ActualLandmassSeedUsed = 0;

	/** Actual seed used for biome generation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generation|Biomes")
	int32 ActualBiomeSeedUsed = 0;

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

	/** Generate both landmass and biomes in sequence */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void GenerateAll();

	/** Clear all generated data and reset outputs */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Generation|Actions")
	void ClearGeneratedData();

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
