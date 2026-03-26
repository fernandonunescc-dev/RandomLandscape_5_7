// LandscapeMeshActor.h
// Procedural mesh actor that builds terrain geometry from biome heightmap data

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "BiomeTypes.h"
#include "BiomeMapGenerator.h"
#include "HeightmapGenerator.h"
#include "LandscapeMeshActor.generated.h"

/**
 * Actor that generates a procedural terrain mesh from biome heightmap data.
 *
 * Takes the combined heightmap data (per-biome grayscale textures), the biome map,
 * and land mask to produce a single unified terrain mesh with per-vertex biome colors.
 *
 * The mesh is a grid whose resolution matches the texture resolution (or a downsampled
 * version for performance). Each vertex Z position is computed by compositing the
 * per-biome heightmaps, and vertex colors are assigned from biome layer colors.
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API ALandscapeMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeMeshActor();

	/** The procedural mesh component that holds the terrain geometry */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	/**
	 * Build the terrain mesh from biome generation data.
	 *
	 * @param BiomeMap       Per-pixel biome IDs (int32 of EBiomeType), size = Resolution^2
	 * @param LandMask       Binary land mask (1=land, 0=ocean), size = Resolution^2
	 * @param HeightmapResults Per-biome heightmap textures (grayscale: white=tallest)
	 * @param BiomeLayers    Biome layer settings (for colors)
	 * @param OceanColor     Color for ocean vertices
	 * @param Resolution     Texture resolution (width = height)
	 * @param WorldSizeCm    World-space size of the map in centimeters
	 * @param MaxMapHeight   Maximum terrain height in centimeters
	 */
	void BuildMesh(
		const TArray<int32>& BiomeMap,
		const TArray<uint8>& LandMask,
		const TArray<FBiomeHeightmapResult>& HeightmapResults,
		const TArray<FBiomeLayerSettings>& BiomeLayers,
		const FLinearColor& OceanColor,
		int32 Resolution,
		float WorldSizeCm,
		float MaxMapHeight);

	/** Clear the procedural mesh */
	void ClearMesh();

private:
	/** Read pixel data from a UTexture2D into a float array (grayscale 0-1) */
	bool ReadHeightmapTexture(UTexture2D* Texture, int32 Resolution, TArray<float>& OutHeights) const;
};
