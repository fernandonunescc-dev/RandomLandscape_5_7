// ContinentMapGenerator.h
// Generator for Continent-type maps

#pragma once

#include "CoreMinimal.h"
#include "MapGeneratorBase.h"
#include "BiomeTypes.h"
#include "ContinentMapGenerator.generated.h"

class UTexture2D;

/**
 * Map generator for Continent-type maps.
 * Creates large connected landmasses with biome distribution using noise.
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

protected:
	/** Generate the preview texture from biome data */
	void GeneratePreviewTexture();

	/** Simple noise function for organic shapes */
	float Noise2D(float X, float Y) const;
	
	/** Fractal Brownian Motion for more natural terrain */
	float FBM(float X, float Y, int32 Octaves, float Persistence) const;

	/** Get continent mask value (0 = ocean, 1 = land) with organic edges */
	float GetContinentMask(float NormX, float NormY) const;

	/** Get biome at a given position based on noise */
	int32 GetBiomeAtPosition(float NormX, float NormY, float ContinentMask) const;

	/** Biome configuration for this continent */
	UPROPERTY()
	FContinentBiomeSettings BiomeSettings;

	/** Preview texture showing biome distribution */
	UPROPERTY()
	TObjectPtr<UTexture2D> PreviewTexture;

	/** Texture resolution (based on MapResolution setting) */
	int32 TextureResolution = 256;

	/** Random seed for generation */
	int32 Seed = 0;
};
