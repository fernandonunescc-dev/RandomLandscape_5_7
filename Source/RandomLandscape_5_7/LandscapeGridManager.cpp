// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeGridManager.h"
#include "Math/UnrealMathUtility.h"

FLandscapeGridManager::FLandscapeGridManager()
{
}

void FLandscapeGridManager::Initialize(float InMapSize, float InGridScale)
{
	MapSize = InMapSize;
	GridScale = InGridScale;
	LoadedCells.Empty();
}

FGridCellData* FLandscapeGridManager::GetOrCreateGridCell(int32 InGridX, int32 InGridY)
{
	int64 CacheKey = GetCacheKey(InGridX, InGridY);

	// Check if cell already exists
	if (LoadedCells.Contains(CacheKey))
	{
		FGridCellData& CellData = LoadedCells[CacheKey];
		CellData.LastAccessTime = FPlatformTime::Seconds();
		return &CellData;
	}

	// Evict LRU cell if we've hit the limit
	if (LoadedCells.Num() >= MaxCachedCells)
	{
		EvictLRUCell();
	}

	// Create new cell
	FGridCellData NewCell;
	NewCell.GridX = InGridX;
	NewCell.GridY = InGridY;
	NewCell.LastAccessTime = FPlatformTime::Seconds();
	NewCell.bIsDataValid = false;

	LoadedCells.Add(CacheKey, NewCell);
	return &LoadedCells[CacheKey];
}

FGridCellData* FLandscapeGridManager::GetGridCell(int32 InGridX, int32 InGridY)
{
	int64 CacheKey = GetCacheKey(InGridX, InGridY);

	if (LoadedCells.Contains(CacheKey))
	{
		FGridCellData& CellData = LoadedCells[CacheKey];
		CellData.LastAccessTime = FPlatformTime::Seconds();
		return &CellData;
	}

	return nullptr;
}

void FLandscapeGridManager::UnloadGridCell(int32 InGridX, int32 InGridY)
{
	int64 CacheKey = GetCacheKey(InGridX, InGridY);
	LoadedCells.Remove(CacheKey);
}

void FLandscapeGridManager::StreamCells(int32 InCenterGridX, int32 InCenterGridY, int32 InLoadRadius)
{
	// Mark cells to keep
	TSet<int64> CellsToKeep;

	for (int32 Y = InCenterGridY - InLoadRadius; Y <= InCenterGridY + InLoadRadius; ++Y)
	{
		for (int32 X = InCenterGridX - InLoadRadius; X <= InCenterGridX + InLoadRadius; ++X)
		{
			CellsToKeep.Add(GetCacheKey(X, Y));
		}
	}

	// Unload cells outside radius
	TArray<int64> CellKeysToRemove;
	for (const auto& Pair : LoadedCells)
	{
		if (!CellsToKeep.Contains(Pair.Key))
		{
			CellKeysToRemove.Add(Pair.Key);
		}
	}

	for (int64 Key : CellKeysToRemove)
	{
		LoadedCells.Remove(Key);
	}
}

void FLandscapeGridManager::GetLoadedCells(TArray<FGridCellData*>& OutCells)
{
	OutCells.Empty();
	for (auto& Pair : LoadedCells)
	{
		OutCells.Add(&Pair.Value);
	}
}

void FLandscapeGridManager::Clear()
{
	LoadedCells.Empty();
}

void FLandscapeGridManager::GetCacheStats(int32& OutCellsLoaded, int32& OutCellsMax) const
{
	OutCellsLoaded = LoadedCells.Num();
	OutCellsMax = MaxCachedCells;
}

void FLandscapeGridManager::EvictLRUCell()
{
	double OldestTime = FPlatformTime::Seconds();
	int64 OldestKey = 0;
	bool bFoundCell = false;

	for (const auto& Pair : LoadedCells)
	{
		if (Pair.Value.LastAccessTime < OldestTime)
		{
			OldestTime = Pair.Value.LastAccessTime;
			OldestKey = Pair.Key;
			bFoundCell = true;
		}
	}

	if (bFoundCell && LoadedCells.Num() > 0)
	{
		LoadedCells.Remove(OldestKey);
	}
}
