// Copyright Fernando Araujo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeTypes.generated.h"

/** Unreal units conversion (1 meter = 100 Unreal units/cm) */
constexpr float METERS_TO_UU = 100.0f;

/**
 * Simple landscape resolution configuration.
 * 
 * RESOLUTION GUIDE (1-100):
 *   1-25:  Low - Large terrain features, good performance
 *   25-50: Medium - Balanced detail and performance
 *   50-75: High - Detailed terrain
 *   75-100: Ultra - Maximum detail, smaller maps recommended
 */
USTRUCT(BlueprintType)
struct FLandscapeResolutionConfig
{
	GENERATED_BODY()

	/**
	 * Total map size in meters (e.g., 500 = 500m x 500m square).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Size",
		meta = (UIMin = "10", UIMax = "10000", ClampMin = "1", ClampMax = "100000"))
	float MapSizeMeters = 500.0f;

	/**
	 * Maximum terrain height in meters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Size",
		meta = (UIMin = "1", UIMax = "1000", ClampMin = "0.1", ClampMax = "10000"))
	float MaxHeightMeters = 50.0f;

	/**
	 * Resolution quality (1-100). Higher = more vertices/detail.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detail",
		meta = (UIMin = "1", UIMax = "100", ClampMin = "1", ClampMax = "100"))
	int32 Resolution = 50;

	// ===== Computed Properties =====
	
	/** Get vertices per meter based on resolution */
	float GetVerticesPerMeter() const
	{
		return (Resolution / 50.0f) * 0.5f;
	}

	/** Get total vertices per side of the entire map */
	int32 GetTotalVerticesPerSide() const
	{
		int32 Verts = FMath::RoundToInt(MapSizeMeters * GetVerticesPerMeter());
		return FMath::Clamp(Verts, 4, 4096);
	}

	/** Get total triangles in the map */
	int32 GetTotalTriangles() const
	{
		int32 VertsPerSide = GetTotalVerticesPerSide();
		return (VertsPerSide - 1) * (VertsPerSide - 1) * 2;
	}

	/** Get chunk size in meters (for async generation) */
	float GetChunkSizeMeters() const
	{
		float VertsPerMeter = GetVerticesPerMeter();
		if (VertsPerMeter <= 0.0f) return MapSizeMeters;
		
		float IdealChunkSize = 100.0f / VertsPerMeter;
		IdealChunkSize = FMath::Clamp(IdealChunkSize, 10.0f, MapSizeMeters);
		return IdealChunkSize;
	}

	/** Get number of chunks per side */
	int32 GetChunksPerSide() const
	{
		return FMath::Max(1, FMath::CeilToInt(MapSizeMeters / GetChunkSizeMeters()));
	}

	/** Get vertices per chunk side */
	int32 GetVerticesPerChunkSide() const
	{
		float ChunkSize = GetChunkSizeMeters();
		int32 Verts = FMath::RoundToInt(ChunkSize * GetVerticesPerMeter());
		return FMath::Clamp(Verts, 4, 256);
	}

	// Legacy compatibility
	float GetTotalMapSizeMeters() const { return MapSizeMeters; }
	int32 GetTotalChunks() const { return GetChunksPerSide() * GetChunksPerSide(); }
};

/**
 * Data for a single generated mesh section.
 */
struct FLandscapeSectionData
{
	/** Section coordinates within the chunk */
	int32 LocalX = 0;
	int32 LocalY = 0;
	
	/** Global section coordinates */
	int32 GlobalX = 0;
	int32 GlobalY = 0;

	/** Generated height values */
	TArray<float> HeightMap;

	/** Computed mesh data */
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;

	/** Min/max heights in this section (Unreal units) */
	float MinHeight = 0.0f;
	float MaxHeight = 0.0f;

	/** Is data fully generated and ready for mesh creation? */
	bool bIsGenerated = false;
};

/**
 * Data for a chunk containing sections.
 */
struct FLandscapeChunkData
{
	/** Chunk coordinates */
	int32 ChunkX = 0;
	int32 ChunkY = 0;

	/** Sections within this chunk */
	TArray<FLandscapeSectionData> Sections;

	/** Is this chunk fully generated? */
	bool bIsGenerated = false;

	/** Time of last access (for LRU caching) */
	double LastAccessTime = 0.0;
};
