// Copyright Fernando Araujo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeTypes.h"

/**
 * Utility class for building procedural meshes from landscape data.
 * Generates vertices, triangles, normals, and UVs.
 * Thread-safe - all methods can be called from background threads.
 */
class RANDOMLANDSCAPE_5_7_API FLandscapeMeshBuilder
{
public:
	/**
	 * Build mesh data for a landscape section from height data.
	 * 
	 * @param SectionData Section containing height map to build mesh from
	 * @param SectionWorldX World X position of section origin (Unreal units)
	 * @param SectionWorldY World Y position of section origin (Unreal units)
	 * @param SectionSizeUU Size of section in Unreal units
	 * @param VerticesPerSide Number of vertices per side (e.g., 32)
	 */
	static void BuildSectionMesh(
		FLandscapeSectionData& SectionData,
		float SectionWorldX,
		float SectionWorldY,
		float SectionSizeUU,
		int32 VerticesPerSide);

	/**
	 * Calculate smooth normals for a mesh.
	 * Uses central differencing for accurate normals.
	 */
	static void CalculateNormals(
		const TArray<FVector>& Vertices,
		int32 VerticesPerSide,
		TArray<FVector>& OutNormals);

	/**
	 * Generate triangle indices for a grid mesh.
	 */
	static void GenerateTriangleIndices(
		int32 VerticesPerSide,
		TArray<int32>& OutTriangles);

	/**
	 * Generate UVs for a grid mesh (0-1 range per section).
	 */
	static void GenerateUVs(
		int32 VerticesPerSide,
		TArray<FVector2D>& OutUVs);
};
