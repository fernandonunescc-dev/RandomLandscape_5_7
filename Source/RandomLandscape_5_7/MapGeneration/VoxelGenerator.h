// VoxelGenerator.h
// 3D voxel terrain generator using FastNoise2.
// Produces a density field + material assignment for each chunk.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelTypes.h"
#include "VoxelGenerator.generated.h"

/**
 * Generates 3D voxel terrain using FastNoise2's SIMD-accelerated noise.
 *
 * Pipeline:
 *   1. Build 2D heightfield via FN2 FBM (with optional domain warp + ridged mountains)
 *   2. Fill 3D density field: density = heightfield(x,y) - z  (solid below surface)
 *   3. Optionally carve caves/overhangs with 3D FN2 noise
 *   4. Assign materials based on depth from surface, height, biome rules
 *   5. Tag chunks as empty/solid for culling
 */
UCLASS()
class RANDOMLANDSCAPE_5_7_API UVoxelGenerator : public UObject
{
	GENERATED_BODY()

public:
	/** Initialise with settings and a global seed. */
	void Initialize(const FVoxelGenerationSettings& InSettings, int32 InSeed);

	/** Generate all chunks. Returns false on failure. */
	bool Generate();

	/** Access generated chunks (valid after Generate()). */
	const TArray<FVoxelChunk>& GetChunks() const { return Chunks; }
	TArray<FVoxelChunk>& GetChunksMutable() { return Chunks; }

	/** Total world dimensions in voxels. */
	FIntVector GetWorldVoxelDimensions() const;

	/** Total world size in centimeters. */
	FVector GetWorldSizeCm() const;

	/** Look up the chunk that contains a given voxel coordinate. Returns nullptr if out of bounds. */
	const FVoxelChunk* GetChunkAtVoxel(int32 VX, int32 VY, int32 VZ) const;

private:
	FVoxelGenerationSettings Settings;
	int32 Seed = 0;
	TArray<FVoxelChunk> Chunks;

	/** Total voxel counts per axis. */
	int32 TotalVoxelsX = 0;
	int32 TotalVoxelsY = 0;
	int32 TotalVoxelsZ = 0;

	/** 2D heightfield (TotalVoxelsX × TotalVoxelsY), values in [0,1] representing fraction of world height. */
	TArray<float> Heightfield;

	/** Generate the 2D heightfield using FN2. */
	void GenerateHeightfield();

	/** Fill a single chunk's density field from the heightfield + 3D features. */
	void FillChunkDensity(FVoxelChunk& Chunk);

	/** Assign materials to a single chunk based on density, height, depth-from-surface. */
	void AssignChunkMaterials(FVoxelChunk& Chunk);

	/** Tag a chunk as empty or solid. */
	void ClassifyChunk(FVoxelChunk& Chunk);

	/** Map chunk coordinate → index in the Chunks array. */
	int32 ChunkIndex(int32 CX, int32 CY, int32 CZ) const;
};
