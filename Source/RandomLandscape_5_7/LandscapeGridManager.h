// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BiomeClassifier.h"
#include "Containers/Map.h"

/**
 * Stores cached data for a single grid cell
 */
struct FGridCellData
{
	int32 GridX;
	int32 GridY;
	
	// Height data (normalized -1 to 1)
	TArray<float> HeightMap;
	
	// Biome classification per vertex
	TArray<EBiomeType> BiomeMap;
	
	// Min/max height for this cell
	float MinHeight = 0.0f;
	float MaxHeight = 0.0f;
	
	// Whether this cell's data is valid
	bool bIsDataValid = false;
	
	// Time when this cell was last accessed (for LRU cache eviction)
	double LastAccessTime = 0.0;
};

/**
 * Manages grid cells and their cached height/biome data
 * Provides streaming and caching functionality for performance
 */
class FLandscapeGridManager
{
public:
	FLandscapeGridManager();

	/**
	 * Initialize the grid manager with map parameters
	 */
	void Initialize(float InMapSize, float InGridScale);

	/**
	 * Get or create a grid cell's data
	 */
	FGridCellData* GetOrCreateGridCell(int32 InGridX, int32 InGridY);

	/**
	 * Get existing grid cell data without creating
	 */
	FGridCellData* GetGridCell(int32 InGridX, int32 InGridY);

	/**
	 * Unload a grid cell from memory
	 */
	void UnloadGridCell(int32 InGridX, int32 InGridY);

	/**
	 * Unload all grid cells outside a certain radius from a center point
	 * @param InCenterGridX Center X in grid coordinates
	 * @param InCenterGridY Center Y in grid coordinates
	 * @param InLoadRadius How many cells away to keep loaded
	 */
	void StreamCells(int32 InCenterGridX, int32 InCenterGridY, int32 InLoadRadius);

	/**
	 * Get all currently loaded grid cells
	 */
	void GetLoadedCells(TArray<FGridCellData*>& OutCells);

	/**
	 * Clear all cached data
	 */
	void Clear();

	/**
	 * Get statistics about cached cells
	 */
	void GetCacheStats(int32& OutCellsLoaded, int32& OutCellsMax) const;

private:
	float MapSize = 100.0f;
	float GridScale = 10.0f;
	
	// Cache of loaded grid cells
	TMap<int64, FGridCellData> LoadedCells;
	
	// Maximum cells to keep in memory at once
	int32 MaxCachedCells = 25;	// 5x5 grid area

	/**
	 * Convert 2D grid coordinates to a single cache key
	 */
	int64 GetCacheKey(int32 InGridX, int32 InGridY) const
	{
		return ((int64)InGridX << 32) | (uint32)InGridY;
	}

	/**
	 * Evict least recently used cell when cache is full
	 */
	void EvictLRUCell();
};
