// MarchingCubes.h
// Isosurface extraction from a 3D density field using the Marching Cubes algorithm.
// Generates triangle mesh data suitable for UProceduralMeshComponent.

#pragma once

#include "CoreMinimal.h"
#include "VoxelTypes.h"

/**
 * Marching Cubes isosurface extraction.
 * Converts a 3D density field (FVoxelChunk) into triangle mesh data.
 *
 * Density > 0 is inside (solid), density <= 0 is outside (air).
 * The isosurface is extracted at the zero-crossing boundary.
 */
struct RANDOMLANDSCAPE_5_7_API FMarchingCubes
{
	/** Output mesh data from extraction. */
	struct FMeshData
	{
		TArray<FVector>    Vertices;
		TArray<int32>      Triangles;
		TArray<FVector>    Normals;
		TArray<FVector2D>  UVs;
		TArray<FColor>     VertexColors;
	};

	/**
	 * Extract an isosurface mesh from a voxel chunk.
	 *
	 * @param Chunk       The voxel chunk containing density values.
	 * @param VoxelSize   World-space size of each voxel in centimeters.
	 * @param IsoLevel    The density threshold for the surface (default 0).
	 * @param OutMesh     Output mesh data.
	 */
	static void Extract(const FVoxelChunk& Chunk, float VoxelSize, float IsoLevel, FMeshData& OutMesh);

	/**
	 * Compute smooth vertex normals from the mesh triangle data.
	 * Averages face normals at shared vertices.
	 */
	static void ComputeNormals(FMeshData& Mesh);

private:
	/** Linearly interpolate between two edge vertices based on density values. */
	static FVector InterpolateEdge(
		const FVector& P1, const FVector& P2,
		float V1, float V2, float IsoLevel);

	/** Classic Marching Cubes edge table (256 entries). */
	static const int32 EdgeTable[256];

	/** Classic Marching Cubes triangle table (256 × 16). */
	static const int32 TriTable[256][16];
};
