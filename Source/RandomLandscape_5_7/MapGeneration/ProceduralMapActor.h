// ProceduralMapActor.h
// Main actor for procedural map generation

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapTypes.h"
#include "BiomeTypes.h"
#include "ProceduralMeshComponent.h"
#include "ProceduralMapActor.generated.h"

class UMapGeneratorBase;
class UTexture2D;
class UContinentMapGenerator;

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

#if WITH_EDITOR
	/** Called when a property is changed in the editor */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	virtual void Tick(float DeltaTime) override;

	// ================ Map Generation ================
	/**
	 * Generate the complete map (landmass + biomes + terrain)
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Actions")
	void GenerateMap();

	/**
	 * Clear the generated map
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Actions")
	void ClearMap();

	/** Total time in seconds for the entire generation process */
	UPROPERTY(VisibleAnywhere, Category="Core")
	float TotalDuration = 0.0f;

	/**
	 * Get the generated preview texture (shows biome distribution)
	 */
	UFUNCTION(BlueprintPure)
	UTexture2D* GetPreviewTexture() const;

public:
	// ==================== General Settings ====================
	
	/** The type of map to generate (Continent or Archipelago) */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General")
	EMapType MapType = EMapType::Continent;

	/** 
	 * The length and width of the map in meters (square map).
	 * Height is determined independently for each biome.
	 * (Internally converted to Unreal Units: 1 meter = 100 UU)
	 */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General", meta = (ClampMin = "10", ClampMax = "10000"))
	int32 MapSizeInMeters = 100;

	/** 
	 * Resolution scale (1-100) where 100 is maximum vertices/triangles per chunk.
	 * Higher values = more detail but higher performance cost.
	 */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MapResolution = 50;

public:
	// ================ Landmass Settings ================
	/**
	 * Generate only the black/white landmass texture
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Landmass")
	void GenerateLandmass();

	/** Seed for landmass shape generation. 0 = random. */
	UPROPERTY(EditAnywhere, Category = "Landmass")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, Category = "Landmass", meta = (ClampMin = "5.0", ClampMax = "75.0", UIMin = "5.0", UIMax = "75.0"))
	float LandmassPercentage = 50.0f; // 5..75 percent of land coverage

	UPROPERTY(VisibleAnywhere, Category = "Landmass")
	TObjectPtr<UTexture2D> LandmassTexture = nullptr;

	/** Time in seconds to generate just the black/white landmass texture */
	UPROPERTY(VisibleAnywhere, Category = "Landmass")
	float LandmassDuration = 0.0f;

	// ================ Biome Settings ================
	/**
	 * Generate biomes on top of the existing landmass.
	 * Must generate landmass first.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Biomes")
	void GenerateBiomes();

	/** Seed for biome distribution. 0 = random. Separate from landmass seed so you can keep the same island shape with different biome layouts. */
	UPROPERTY(EditAnywhere, Category = "Biomes")
	int32 BiomeSeed = 0;

	/** The generated biome map texture (colored by biome) */
	UPROPERTY(VisibleAnywhere, Category = "Biomes")
	TObjectPtr<UTexture2D> BiomeTexture = nullptr;

	/** Time in seconds to generate the biome distribution */
	UPROPERTY(VisibleAnywhere, Category = "Biomes")
	float BiomeDuration = 0.0f;

	/** Biome configuration settings */
	UPROPERTY(EditAnywhere, Category = "Biomes")
	FContinentBiomeSettings BiomeSettings;

	// ================ Mesh Settings ================
	
	/** Generate the terrain mesh from biome data */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh")
	void GenerateMesh();

	/** Mesh generation settings for all biomes (including ocean) */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	FMeshGenerationSettings MeshSettings;

	// Individual biome texture generation buttons
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh|Generate Textures")
	void GenerateOceanTexture();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh|Generate Textures")
	void GenerateForestTexture();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh|Generate Textures")
	void GenerateMountainTexture();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh|Generate Textures")
	void GenerateDesertTexture();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh|Generate Textures")
	void GenerateSnowTexture();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh|Generate Textures")
	void GenerateVolcanicTexture();

	/** Generate all biome mask textures at once */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh|Generate Textures")
	void GenerateAllBiomeTextures();

protected:
	/** The current map generator instance */
	UPROPERTY()
	TObjectPtr<UMapGeneratorBase> CurrentGenerator;

	/** Generate the 3D terrain mesh from the biome map */
	void GenerateTerrainMesh(UContinentMapGenerator* Generator);

	/** Calculate terrain height using fractal Perlin noise with biome-specific settings */
	float CalculateTerrainHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig) const;

	/** 
	 * Calculate smoothly blended terrain height at a position.
	 * Uses bilinear interpolation of surrounding biome heights for smooth transitions.
	 */
	float CalculateBlendedTerrainHeight(float NormX, float NormY, int32 TextureRes,
		const TArray<int32>& BiomeMap, const TArray<bool>& LandMask) const;

	/**
	 * Get biome-blended color at a position with smooth transitions.
	 */
	FColor GetBlendedBiomeColor(float NormX, float NormY, int32 TextureRes,
		const TArray<int32>& BiomeMap, const TArray<bool>& LandMask) const;

private:
	/** Create generator settings from current properties */
	FMapGenerationSettings CreateSettings() const;

	/** Generate mask texture for a specific biome type */
	void GenerateBiomeMaskTexture(EBiomeType BiomeType);

	/** Random stream for terrain generation */
	FRandomStream TerrainRandomStream;

	// Internal settings (not exposed in editor)
	int32 ChunksPerSide = 10;
	int32 VerticesPerChunkSide = 100;

	// Internal pointers - keep as UPROPERTY to avoid GC warnings
	UPROPERTY()
	TObjectPtr<UTexture2D> PreviewTexture;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	// Utility functions
	void NormalizeBiomePercentages();
};
