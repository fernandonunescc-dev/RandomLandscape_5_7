// ProceduralMapActor.h
// Main actor for procedural map generation

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapTypes.h"
#include "BiomeTypes.h"
#include "ProceduralMapActor.generated.h"

class UMapGeneratorBase;
class UTexture2D;

/**
 * Actor that manages procedural map generation.
 * Place this in your level and configure the properties to generate different map types.
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API AProceduralMapActor : public AActor
{
	GENERATED_BODY()

public:
	AProceduralMapActor();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

	/**
	 * Generate the map using current settings
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Map Generation")
	void GenerateMap();

	/**
	 * Clear the generated map
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Map Generation")
	void ClearMap();

	/**
	 * Get the generated preview texture (shows biome distribution)
	 */
	UFUNCTION(BlueprintPure, Category = "Map Generation")
	UTexture2D* GetPreviewTexture() const;

public:
	// ==================== General Settings ====================
	
	/** The type of map to generate (Continent or Archipelago) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General")
	EMapType MapType = EMapType::Continent;

	/** 
	 * The size of the map in meters.
	 * X = Width, Y = Depth, Z = Maximum Height
	 * (Internally converted to Unreal Units: 1 meter = 100 UU)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General", meta = (ClampMin = "1.0"))
	FVector MapSizeInMeters = FVector(100.0f, 100.0f, 10.0f);

	/** 
	 * Resolution scale (1-100) where 100 is maximum vertices/triangles per chunk.
	 * Higher values = more detail but higher performance cost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MapResolution = 50;

	/**
	 * Random seed for map generation. 
	 * Use 0 for a random seed each time, or set a specific value for reproducible results.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General")
	int32 Seed = 0;

	// ==================== Continent Biome Settings ====================
	
	/** 
	 * Biome configuration for Continent map type.
	 * Configure the percentage and color for each biome.
	 * Land biome percentages should add up to 100%.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Continent Biomes", meta = (EditCondition = "MapType == EMapType::Continent", EditConditionHides))
	FContinentBiomeSettings ContinentBiomeSettings;

	// ==================== Preview ====================
	
	/** 
	 * Preview texture showing the generated biome distribution.
	 * Updated after calling GenerateMap().
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Preview", Transient)
	TObjectPtr<UTexture2D> PreviewTexture;

	// ==================== Utility Functions ====================
	
	/** Get the map size in Unreal Units (centimeters) */
	UFUNCTION(BlueprintPure, Category = "Map Generation")
	FVector GetMapSizeInUnrealUnits() const { return MapSizeInMeters * MetersToUnrealUnits; }

	/** Normalize biome percentages to add up to 100% */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Map Generation|Continent Biomes")
	void NormalizeBiomePercentages();

	/** Get the total of all biome percentages */
	UFUNCTION(BlueprintPure, Category = "Map Generation|Continent Biomes")
	float GetTotalBiomePercentage() const { return ContinentBiomeSettings.GetTotalPercentage(); }

protected:
	/** The current map generator instance */
	UPROPERTY()
	TObjectPtr<UMapGeneratorBase> CurrentGenerator;

private:
	/** Create generator settings from current properties */
	FMapGenerationSettings CreateSettings() const;
};
