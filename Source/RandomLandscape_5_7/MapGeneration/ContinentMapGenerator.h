// ContinentMapGenerator.h
// Generator for Continent-type maps

#pragma once

#include "CoreMinimal.h"
#include "MapGeneratorBase.h"
#include "BiomeTypes.h"
#include "ContinentMapGenerator.generated.h"

class UTexture2D;

/** Represents a seed point for Voronoi-based biome distribution */
struct FBiomeSeedPoint
{
	FVector2D Position;
	int32 BiomeIndex;
	float Weight; // Used to adjust region size
	
	FBiomeSeedPoint() : Position(FVector2D::ZeroVector), BiomeIndex(0), Weight(1.0f) {}
	FBiomeSeedPoint(const FVector2D& InPos, int32 InBiome, float InWeight) 
		: Position(InPos), BiomeIndex(InBiome), Weight(InWeight) {}
};

/**
 * Map generator for Continent-type maps.
 * Uses a two-pass approach:
 * 1. Generate continent shape (land mask)
 * 2. Distribute biomes based on actual land pixel count
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

	/** Set the biome configuration */
	void SetBiomeSettings(const FContinentBiomeSettings& InBiomeSettings);

	/** Get the biome settings */
	const FContinentBiomeSettings& GetBiomeSettings() const { return BiomeSettings; }

	/** Get the generated preview texture (nullptr if not yet generated) */
	UFUNCTION(BlueprintPure, Category = "Map Generation")
	UTexture2D* GetPreviewTexture() const { return PreviewTexture; }

	/** Set the random seed for generation */
	void SetSeed(int32 InSeed) { Seed = InSeed; }

	/** Get the biome map (index per pixel, -1 = ocean) */
	const TArray<int32>& GetBiomeMap() const { return BiomeMap; }

	/** Get the land mask (true = land, false = ocean) */
	const TArray<bool>& GetLandMask() const { return LandMask; }

	/** Get the texture resolution */
	int32 GetTextureResolution() const { return TextureResolution; }

protected:
	/** Pass 1: Generate the land mask (which pixels are land vs ocean) */
	void GenerateLandMask();

	/** Pass 2: Generate biome seed points and assign biomes to land pixels */
	void AssignBiomesToLand();

	/** Pass 3: Generate the final preview texture with biome colors */
	void GeneratePreviewTexture();

	/** Simple noise function for organic shapes */
	float Noise2D(float X, float Y) const;
	
	/** Fractal Brownian Motion for more natural terrain */
	float FBM(float X, float Y, int32 Octaves, float Persistence) const;

	/** Get continent mask value (0 = ocean, 1 = land) with organic edges */
	float GetContinentMask(float NormX, float NormY) const;

	/** Biome configuration for this continent */
	UPROPERTY()
	FContinentBiomeSettings BiomeSettings;

	/** Preview texture showing biome distribution */
	UPROPERTY()
	TObjectPtr<UTexture2D> PreviewTexture;

	/** Land mask - true = land, false = ocean */
	TArray<bool> LandMask;

	/** Biome index for each pixel (-1 = ocean) */
	TArray<int32> BiomeMap;

	/** List of all land pixel coordinates (for seed point placement) */
	TArray<FIntPoint> LandPixels;

	/** Voronoi seed points for biome regions */
	TArray<FBiomeSeedPoint> BiomeSeedPoints;

	/** Texture resolution (based on MapResolution setting) */
	int32 TextureResolution = 256;

	/** Random seed for generation */
	int32 Seed = 0;

	/** Random stream for deterministic generation */
	FRandomStream RandomStream;
};
