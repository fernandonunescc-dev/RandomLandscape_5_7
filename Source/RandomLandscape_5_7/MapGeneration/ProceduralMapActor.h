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
	
	/** Called when the actor is constructed (including in editor) */
	virtual void OnConstruction(const FTransform& Transform) override;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Core", meta = (ClampMin = "10", ClampMax = "100000", UIMin = "10", UIMax = "100000"))
	int32 MapSizeInMeters = 1000;

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

	/** 
	 * Generate all biome textures: mask textures and high-res height maps directly from FastNoise2.
	 * This is the main function that generates everything needed for mesh generation.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Mesh")
	void GenerateBiomeTextures();


	/** Time in seconds to generate all biome mask/height textures */
	UPROPERTY(VisibleAnywhere, Category = "Mesh")
	float MeshTexturesDuration = 0.0f;

	/** Time in seconds to generate the terrain mesh */
	UPROPERTY(VisibleAnywhere, Category = "Mesh")
	float MeshGenerationDuration = 0.0f;

	/** 
	 * Mesh detail level (1-100). Higher values = more vertices/triangles.
	 * 1 = Low detail (faster), 100 = Maximum detail (slower).
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 MeshDetailLevel = 50;

	/** 
	 * Global minimum height in meters for height map normalization.
	 * This defines the "black" point in height map textures.
	 * Should be set to accommodate the lowest biome (e.g., ocean floor).
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh|Height Range", meta = (ClampMin = "-500.0", ClampMax = "0.0"))
	float GlobalMinHeightInMeters = -50.0f;

	/** 
	 * Global maximum height in meters for height map normalization.
	 * This defines the "white" point in height map textures.
	 * Should be set to accommodate the highest biome (e.g., mountain peaks).
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh|Height Range", meta = (ClampMin = "0.0", ClampMax = "1000.0"))
	float GlobalMaxHeightInMeters = 200.0f;

	/**
	 * Use tileable noise generation (GenTileable2D) instead of regular grid (GenUniformGrid2D).
	 * Enable for seamless world wrapping where terrain edges connect smoothly.
	 * Disable for better variety in non-wrapping worlds.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh|Noise Settings")
	bool bUseTileableNoise = false;

	/**
	 * Resolution of the noise heightmap used for terrain generation.
	 * Higher values = more terrain detail but more memory usage.
	 * Common values: 1024 (1K), 2048 (2K), 4096 (4K), 8192 (8K)
	 * This is independent of mesh vertex count - allows high-detail noise sampling.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh|Noise Settings", meta = (ClampMin = "256", ClampMax = "8192", UIMin = "256", UIMax = "8192"))
	int32 HeightMapResolution = 4096;

	/** 
	 * Combined height map texture showing the full landmass terrain.
	 * Merges all individual biome height maps into a single unified texture.
	 * Grayscale where white = highest, black = lowest.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Mesh")
	TObjectPtr<UTexture2D> CombinedHeightMapTexture = nullptr;

	/** 
	 * Material to apply to the terrain mesh.
	 * Tip: Use a material with Vertex Color node to display biome colors.
	 */
	UPROPERTY(EditAnywhere, Category = "Mesh")
	TObjectPtr<UMaterialInterface> TerrainMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Mesh|Ocean")
	FBiomeMeshSettings OceanMeshSettings;

	UPROPERTY(EditAnywhere, Category = "Mesh|Forest")
	FBiomeMeshSettings ForestMeshSettings;

	UPROPERTY(EditAnywhere, Category = "Mesh|Mountain")
	FBiomeMeshSettings MountainMeshSettings;

	UPROPERTY(EditAnywhere, Category = "Mesh|Desert")
	FBiomeMeshSettings DesertMeshSettings;

	UPROPERTY(EditAnywhere, Category = "Mesh|Snow")
	FBiomeMeshSettings SnowMeshSettings;


	UPROPERTY(EditAnywhere, Category = "Mesh|Volcanic")
	FBiomeMeshSettings VolcanicMeshSettings;


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

	/** Generate the combined height map texture from all biomes */
	void GenerateCombinedHeightMapTexture();


	/** Get mesh settings for a specific biome type */
	FBiomeMeshSettings* GetMeshSettingsForBiome(EBiomeType BiomeType);
	const FBiomeMeshSettings* GetMeshSettingsForBiome(EBiomeType BiomeType) const;

	/** 
	 * Pre-generate all biome heightmaps using GenUniformGrid2D for efficiency.
	 * Called before mesh generation to batch-compute all noise values.
	 */
	void PreGenerateBiomeHeightMaps(int32 Resolution);

	/** 
	 * Sample height from pre-generated heightmaps with biome blending.
	 * Much faster than computing noise per-vertex.
	 */
	float SamplePreGeneratedHeight(float NormX, float NormY, int32 TextureRes,
		const TArray<int32>& BiomeMap, const TArray<bool>& LandMask) const;

	/** Random stream for terrain generation */
	FRandomStream TerrainRandomStream;

	// Internal settings (not exposed in editor)
	int32 ChunksPerSide = 10;
	int32 VerticesPerChunkSide = 100;

	/** Cached pre-generated heightmaps per biome (generated using GenUniformGrid2D) */
	TMap<EBiomeType, TArray<float>> CachedBiomeHeightMaps;
	
	/** Resolution of the cached heightmaps */
	int32 CachedHeightMapResolution = 0;

	// Internal pointers - keep as UPROPERTY to avoid GC warnings
	UPROPERTY()
	TObjectPtr<UTexture2D> PreviewTexture;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;


	// Utility functions
	void NormalizeBiomePercentages();
};
