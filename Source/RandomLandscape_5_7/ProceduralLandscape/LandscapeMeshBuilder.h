// Copyright Fernando Araujo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeTypes.h"

/**
 * Configuration for circular island generation.
 * Creates a roughly circular island with smooth edge falloff.
 */
struct FIslandEdgeConfig
{
	/** Is island mode enabled? */
	bool bEnabled = false;
	
	/** 
	 * (Not used in circular mode - kept for compatibility)
	 */
	float FalloffDistance = 5000.0f;
	
	/** Sea level height (Unreal units) */
	float SeaLevel = 0.0f;
	
	/** Depth of skirt below sea level (Unreal units) */
	float SkirtDepth = 5000.0f;
	
	/** 
	 * Edge falloff curve exponent.
	 * Controls how sharply the terrain drops at edges:
	 * < 1.0 = Gentle, gradual slopes
	 * = 1.0 = Smooth natural falloff
	 * > 1.0 = Terrain stays high longer, then drops more sharply
	 * = 2.0 = Nice cliff-like edges
	 */
	float FalloffExponent = 1.5f;
	
	/** Total map size (Unreal units) - needed to calculate edge distances */
	float MapSizeUU = 50000.0f;
	
	/** Map center position (Unreal units) */
	FVector2D MapCenter = FVector2D::ZeroVector;
	
	/** Maximum terrain height (Unreal units) */
	float MaxTerrainHeight = 5000.0f;
	
	/** 
	 * Controls the size of the island's flat "core" area (0-1).
	 * This determines where the falloff starts:
	 * 
	 * 0.3 = Large flat center (70%), small edge falloff zone (30%)
	 * 0.5 = Medium balance - falloff covers half the radius
	 * 0.7 = Small flat center (30%), gradual falloff across most of island
	 * 1.0 = Falloff starts from the very center (dome shape)
	 */
	float IslandShapeStrength = 0.5f;
};

/**
 * Edge flags for a chunk indicating which edges are at the map boundary.
 */
struct FChunkEdgeFlags
{
	bool bIsLeftEdge = false;
	bool bIsRightEdge = false;
	bool bIsBottomEdge = false;
	bool bIsTopEdge = false;
	
	bool HasAnyEdge() const 
	{ 
		return bIsLeftEdge || bIsRightEdge || bIsBottomEdge || bIsTopEdge; 
	}
};

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
	 * @param EdgeConfig Optional island edge configuration
	 * @param EdgeFlags Which edges of this chunk are at map boundary
	 */
	static void BuildSectionMesh(
		FLandscapeSectionData& SectionData,
		float SectionWorldX,
		float SectionWorldY,
		float SectionSizeUU,
		int32 VerticesPerSide,
		const FIslandEdgeConfig& EdgeConfig = FIslandEdgeConfig(),
		const FChunkEdgeFlags& EdgeFlags = FChunkEdgeFlags());

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

private:
	/**
	 * Apply island edge falloff to a height value based on distance from map edge.
	 */
	static float ApplyEdgeFalloff(
		float OriginalHeight,
		float WorldX,
		float WorldY,
		const FIslandEdgeConfig& Config);

	/**
	 * Add skirt geometry around the edges of a chunk.
	 */
	static void AddEdgeSkirt(
		FLandscapeSectionData& SectionData,
		float SectionWorldX,
		float SectionWorldY,
		float SectionSizeUU,
		int32 VerticesPerSide,
		const FIslandEdgeConfig& EdgeConfig,
		const FChunkEdgeFlags& EdgeFlags);
};
