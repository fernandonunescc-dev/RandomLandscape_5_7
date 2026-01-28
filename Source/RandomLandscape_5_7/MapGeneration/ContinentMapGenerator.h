// ContinentMapGenerator.h
// Generator for Continent-type maps
// Version: 01.28.2026.01.00

#pragma once

#include "CoreMinimal.h"
#include "MapGeneratorBase.h"
#include "BiomeTypes.h"
#include "LandmassGenerator.h"
#include "BiomeMapGenerator.h"
#include "ContinentMapGenerator.generated.h"

class UTexture2D;
class ULandmassGenerator;
class UBiomeMapGenerator;

/**
 * Map generator for Continent-type maps.
 * Uses a two-pass approach:
 * 1. Generate continent shape (land mask) via LandmassGenerator
 * 2. Distribute biomes via BiomeMapGenerator
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API UContinentMapGenerator : public UMapGeneratorBase
{
	GENERATED_BODY()

public:
	UContinentMapGenerator();
	virtual ~UContinentMapGenerator() = default;

	//~ Begin UMapGeneratorBase Interface
	virtual void Initialize(const FMapGenerationSettings& InSettings) override;
	virtual bool Generate() override;
	//~ End UMapGeneratorBase Interface

	/** Set external BiomeGenerator (source of truth for biome settings) */
	void SetBiomeGenerator(UBiomeMapGenerator* InGenerator) { ExternalBiomeGenerator = InGenerator; }

	/** Set the biome configuration (DEPRECATED - use SetBiomeGenerator instead) */
	void SetBiomeSettings(const FContinentBiomeSettings& InBiomeSettings);

	/** Set the mesh generation settings */
	void SetMeshSettings(const FMeshGenerationSettings& InMeshSettings);

	/** Get the biome settings (DEPRECATED - use BiomeGenerator->LayoutSettings) */
	const FContinentBiomeSettings& GetBiomeSettings() const { return BiomeSettings; }

	/** Get the mesh settings (includes generated mask textures) */
	const FMeshGenerationSettings& GetMeshSettings() const { return MeshSettings; }

	/** Get the generated preview texture (nullptr if not yet generated) */
	UFUNCTION(BlueprintPure, Category = "Map Generation")
	UTexture2D* GetPreviewTexture() const { return PreviewTexture; }

	/** Set the random seed for landmass generation */
	void SetSeed(int32 InSeed) { Seed = InSeed; }

	/** Set the random seed for biome distribution (DEPRECATED - use BiomeGenerator->LayoutSettings.BiomeSeed) */
	void SetBiomeSeed(int32 InSeed) { BiomeSeed = InSeed; }

	/** Generate only the landmass (land/water mask) */
	bool GenerateLandmassOnly();

	/** Generate biomes on existing landmass (must call GenerateLandmassOnly first) */
	bool GenerateBiomesOnly();

	/** Generate individual mask textures for each biome (biome color on its area, black elsewhere) */
	void GenerateBiomeMaskTextures();

	/** Get the biome map (index per pixel, -1 = ocean) */
	const TArray<int32>& GetBiomeMap() const { return BiomeMap; }

	/** Get the land mask (1 = land, 0 = ocean) */
	const TArray<uint8>& GetLandMask() const { return LandMask; }

	/** Get the texture resolution */
	int32 GetTextureResolution() const { return TextureResolution; }

	/** Get the active BiomeGenerator (external or internal) */
	UBiomeMapGenerator* GetActiveBiomeGenerator() const;

protected:
	/** External BiomeGenerator (set via SetBiomeGenerator, source of truth) */
	UPROPERTY()
	TObjectPtr<UBiomeMapGenerator> ExternalBiomeGenerator;

	/** Landmass generator - handles land/ocean mask generation */
	UPROPERTY()
	TObjectPtr<ULandmassGenerator> LandmassGenerator;

	/** Internal Biome map generator - used if no external generator set */
	UPROPERTY()
	TObjectPtr<UBiomeMapGenerator> BiomeGenerator;

	/** Random seed for landmass generation */
	int32 Seed = 0;

	/** Random seed for biome distribution (DEPRECATED - use BiomeGenerator->LayoutSettings.BiomeSeed) */
	int32 BiomeSeed = 0;

	/** Random stream for landmass generation */
	FRandomStream RandomStream;

	/** Random stream for biome distribution */
	FRandomStream BiomeRandomStream;

	/** Texture resolution (based on MapResolution setting) */
	int32 TextureResolution = 256;


	/** Pass 1: Generate the land mask (which pixels are land vs ocean) */
	void GenerateLandMask();

	/** Pass 2: Generate biome seed points and assign biomes to land pixels */
	void AssignBiomesToLand();

	/** Pass 3: Generate the final preview texture with biome colors */
	void GeneratePreviewTexture();


	/** Biome configuration for this continent (DEPRECATED - use ExternalBiomeGenerator->LayoutSettings) */
	UPROPERTY()
	FContinentBiomeSettings BiomeSettings;

	/** Mesh generation settings (stores generated mask textures) */
	UPROPERTY()
	FMeshGenerationSettings MeshSettings;

	/** Preview texture showing biome distribution */
	UPROPERTY()
	TObjectPtr<UTexture2D> PreviewTexture;

	/** Land mask - 1 = land, 0 = ocean (uint8 for consistency with LandmassGenerator) */
	TArray<uint8> LandMask;

	/** Biome index for each pixel (-1 = ocean) */
	TArray<int32> BiomeMap;

	/** List of all land pixel coordinates (for seed point placement) */
	TArray<FIntPoint> LandPixels;
};
