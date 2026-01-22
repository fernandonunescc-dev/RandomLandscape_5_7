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
	 * The length and width of the map in meters (square map).
	 * Height is determined independently for each biome.
	 * (Internally converted to Unreal Units: 1 meter = 100 UU)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|General", meta = (ClampMin = "10", ClampMax = "10000"))
	int32 MapSizeInMeters = 100;

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

	// ==================== Terrain Mesh ====================
	
	/** The procedural mesh component for the terrain */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Terrain")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/** 
	 * Number of chunks per side to divide the terrain into.
	 * More chunks = higher resolution possible.
	 * Total chunks = ChunksPerSide^2 (e.g., 10 = 100 chunks)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Terrain", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "100"))
	int32 ChunksPerSide = 10;

	/** 
	 * Number of vertices per side for EACH chunk.
	 * Higher values = smoother terrain within each chunk.
	 * Total vertices per chunk = VerticesPerChunkSide^2
	 * Total map vertices = (ChunksPerSide * VerticesPerChunkSide)^2
	 * Example: 10 chunks x 100 verts = 1000 verts per side = 1M total vertices
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Terrain", meta = (ClampMin = "8", ClampMax = "255", UIMin = "8", UIMax = "255"))
	int32 VerticesPerChunkSide = 100;

	/** 
	 * Material to use for terrain. Should use Vertex Color node to display biome colors.
	 * If not set, a default material will be created.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Terrain")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	// ==================== Utility Functions ====================
	
	/** Get the map size (length/width) in Unreal Units (centimeters) */
	UFUNCTION(BlueprintPure, Category = "Map Generation")
	float GetMapSizeInUnrealUnits() const { return static_cast<float>(MapSizeInMeters) * MetersToUnrealUnits; }

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

	/** Random stream for terrain generation */
	FRandomStream TerrainRandomStream;
};
