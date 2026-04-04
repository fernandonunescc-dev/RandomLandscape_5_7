// VoxelWorldActor.h
// Voxel world generation pipeline orchestrator.
// Uses FastNoise2 for 3D terrain generation and Marching Cubes for mesh extraction.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "VoxelTypes.h"
#include "VoxelWorldActor.generated.h"

class UVoxelGenerator;

/**
 * 3D voxel world generation actor.
 *
 * Pipeline:
 *   1. Noise → 3D density field (FastNoise2 SIMD-accelerated)
 *   2. Material assignment (height/depth rules)
 *   3. Marching Cubes → ProceduralMeshComponent per chunk
 *
 * Place in the level and click Generate to build voxel terrain.
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API AVoxelWorldActor : public AActor
{
	GENERATED_BODY()

public:
	AVoxelWorldActor();

	// ==================== Settings ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Settings")
	int32 GlobalSeed = 42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Settings")
	FVoxelGenerationSettings VoxelSettings;

	/** Density iso-level for the marching cubes surface extraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Mesh", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float IsoLevel = 0.0f;

	/** Material applied to chunk meshes. Use a material that reads Vertex Color for biome-colored terrain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Mesh")
	TObjectPtr<UMaterialInterface> VoxelMaterial = nullptr;

	/** Auto-regenerate when properties change in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel|Settings")
	bool bAutoRegenerate = false;

	// ==================== Debug ====================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	int32 TotalChunks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	int32 EmptyChunks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	int32 SolidChunks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	int32 MeshedChunks = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	int32 TotalVertices = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	int32 TotalTriangles = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	FVector WorldSizeCm = FVector::ZeroVector;

	// ==================== Debug Textures ====================

	/** Top-down heightfield preview. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	TObjectPtr<UTexture2D> Debug_Heightfield = nullptr;

	/** Cross-section slice at the midpoint of the Z axis. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel|Debug")
	TObjectPtr<UTexture2D> Debug_CrossSection = nullptr;

	// ==================== Actions ====================

	/** Generate the full voxel world: noise → density → mesh. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Voxel|Actions")
	void GenerateVoxelWorld();

	/** Clear all generated data and meshes. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Voxel|Actions")
	void ClearAll();

	/** Randomize seed and regenerate. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Voxel|Actions")
	void Randomize();

	// ==================== Editor Auto-Regen ====================

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Root component for chunk meshes. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> VoxelRoot;

	/** One ProceduralMeshComponent per meshed chunk. */
	UPROPERTY()
	TArray<TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;

	/** Build mesh sections from generated voxel data. */
	void BuildChunkMeshes(UVoxelGenerator* Generator);

	/** Create debug textures from generator data. */
	void CreateDebugTextures(UVoxelGenerator* Generator);

	/** Helper: create a transient texture from pixel data. */
	UTexture2D* CreateColorTexture(int32 Res, const TArray<FColor>& Pixels);
};
