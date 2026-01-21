// Copyright Fernando Araujo. All Rights Reserved.

#include "LandscapeAsyncGenerator.h"
#include "LandscapeNoiseService.h"
#include "LandscapeMeshBuilder.h"
#include "Async/Async.h"

// ============================================================================
// Constructor / Destructor
// ============================================================================

FLandscapeAsyncGenerator::FLandscapeAsyncGenerator()
{
}

FLandscapeAsyncGenerator::~FLandscapeAsyncGenerator()
{
	CancelAllPending();
	WaitForCompletion();
}

// ============================================================================
// Initialization
// ============================================================================

void FLandscapeAsyncGenerator::Initialize(FLandscapeNoiseService* InNoiseService, const FLandscapeResolutionConfig& InResolution, const FIslandEdgeConfig& InEdgeConfig)
{
	NoiseService = InNoiseService;
	Resolution = InResolution;
	EdgeConfig = InEdgeConfig;
	bCancelRequested = false;
}

// ============================================================================
// Chunk Queuing
// ============================================================================

void FLandscapeAsyncGenerator::QueueChunkGeneration(int32 ChunkX, int32 ChunkY, float MapOriginX, float MapOriginY, int32 ChunksPerSide)
{
	if (!NoiseService)
	{
		UE_LOG(LogTemp, Error, TEXT("LandscapeAsyncGenerator: NoiseService not initialized!"));
		return;
	}

	// Increment pending count before launching task
	PendingCount.Increment();

	// Capture all needed data by value for the async task
	FLandscapeResolutionConfig CapturedResolution = Resolution;
	FIslandEdgeConfig CapturedEdgeConfig = EdgeConfig;
	FLandscapeNoiseService* CapturedNoiseService = NoiseService;
	FThreadSafeBool* CapturedCancelFlag = &bCancelRequested;
	FCriticalSection* CapturedLock = &CompletedChunksLock;
	TArray<FLandscapeChunkData>* CapturedCompletedChunks = &CompletedChunks;
	FThreadSafeCounter* CapturedPendingCount = &PendingCount;

	// Determine which edges of this chunk are at map boundary
	FChunkEdgeFlags EdgeFlags;
	EdgeFlags.bIsLeftEdge = (ChunkX == 0);
	EdgeFlags.bIsRightEdge = (ChunkX == ChunksPerSide - 1);
	EdgeFlags.bIsBottomEdge = (ChunkY == 0);
	EdgeFlags.bIsTopEdge = (ChunkY == ChunksPerSide - 1);

	// Launch async task
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, 
		[CapturedResolution, CapturedEdgeConfig, EdgeFlags, CapturedNoiseService, CapturedCancelFlag, CapturedLock, 
		 CapturedCompletedChunks, CapturedPendingCount, ChunkX, ChunkY, MapOriginX, MapOriginY]()
	{
		// Check for cancellation
		if (*CapturedCancelFlag)
		{
			CapturedPendingCount->Decrement();
			return;
		}

		// Create chunk data
		FLandscapeChunkData ChunkData;
		ChunkData.ChunkX = ChunkX;
		ChunkData.ChunkY = ChunkY;

		const float ChunkSizeMeters = CapturedResolution.GetChunkSizeMeters();
		const int32 VerticesPerSide = CapturedResolution.GetVerticesPerChunkSide();

		// Calculate chunk world position
		const float ChunkWorldX = MapOriginX + ChunkX * ChunkSizeMeters;
		const float ChunkWorldY = MapOriginY + ChunkY * ChunkSizeMeters;

		// Single section per chunk in simplified model
		ChunkData.Sections.SetNum(1);

		// Check cancellation
		if (*CapturedCancelFlag)
		{
			CapturedPendingCount->Decrement();
			return;
		}

		FLandscapeSectionData& Section = ChunkData.Sections[0];

		Section.LocalX = 0;
		Section.LocalY = 0;
		Section.GlobalX = ChunkX;
		Section.GlobalY = ChunkY;

		// Generate height data
		CapturedNoiseService->GenerateHeightMapRegion(
			ChunkWorldX,
			ChunkWorldY,
			ChunkSizeMeters,
			VerticesPerSide,
			Section.HeightMap
		);

		// Build mesh data (vertices, triangles, normals, UVs)
		// Note: Positions in Unreal units for mesh
		const float ChunkSizeUU = ChunkSizeMeters * METERS_TO_UU;
		const float ChunkWorldXUU = ChunkWorldX * METERS_TO_UU;
		const float ChunkWorldYUU = ChunkWorldY * METERS_TO_UU;

		FLandscapeMeshBuilder::BuildSectionMesh(
			Section,
			ChunkWorldXUU,
			ChunkWorldYUU,
			ChunkSizeUU,
			VerticesPerSide,
			CapturedEdgeConfig,
			EdgeFlags
		);

		ChunkData.bIsGenerated = true;
		ChunkData.LastAccessTime = FPlatformTime::Seconds();

		// Add to completed list (thread-safe)
		{
			FScopeLock Lock(CapturedLock);
			CapturedCompletedChunks->Add(MoveTemp(ChunkData));
		}

		CapturedPendingCount->Decrement();
	});
}

void FLandscapeAsyncGenerator::QueueAllChunks(int32 ChunksPerSide, float MapOriginX, float MapOriginY)
{
	for (int32 ChunkY = 0; ChunkY < ChunksPerSide; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < ChunksPerSide; ++ChunkX)
		{
			QueueChunkGeneration(ChunkX, ChunkY, MapOriginX, MapOriginY, ChunksPerSide);
		}
	}
}

// ============================================================================
// Completion Handling
// ============================================================================

int32 FLandscapeAsyncGenerator::GetCompletedChunks(TArray<FLandscapeChunkData>& OutChunks)
{
	FScopeLock Lock(&CompletedChunksLock);
	
	const int32 Count = CompletedChunks.Num();
	if (Count > 0)
	{
		OutChunks = MoveTemp(CompletedChunks);
		CompletedChunks.Empty();
	}
	
	return Count;
}

// ============================================================================
// Cancellation and Waiting
// ============================================================================

void FLandscapeAsyncGenerator::CancelAllPending()
{
	bCancelRequested = true;
}

void FLandscapeAsyncGenerator::WaitForCompletion()
{
	// Busy-wait with sleep (not ideal but simple)
	while (PendingCount.GetValue() > 0)
	{
		FPlatformProcess::Sleep(0.01f);  // 10ms
	}
	
	bCancelRequested = false;
}
