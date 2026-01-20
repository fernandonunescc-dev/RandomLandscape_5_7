// Copyright Fernando Araujo. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeTypes.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"

class FLandscapeNoiseService;

/**
 * Manages asynchronous generation of landscape chunks.
 * Queues generation tasks and tracks their completion for main-thread mesh creation.
 * 
 * Generation Flow:
 * 1. Main thread queues chunks via QueueChunkGeneration()
 * 2. Background threads generate height/biome data
 * 3. Main thread polls GetCompletedChunks() each tick
 * 4. Main thread creates meshes from completed data
 */
class RANDOMLANDSCAPE_5_7_API FLandscapeAsyncGenerator
{
public:
	FLandscapeAsyncGenerator();
	~FLandscapeAsyncGenerator();

	/**
	 * Initialize the generator with required services.
	 * @param InNoiseService Pointer to noise service (must remain valid during generation)
	 * @param InResolution Resolution configuration
	 */
	void Initialize(FLandscapeNoiseService* InNoiseService, const FLandscapeResolutionConfig& InResolution);

	/**
	 * Queue a chunk for background generation.
	 * Safe to call from main thread only.
	 * @param ChunkX Chunk X coordinate
	 * @param ChunkY Chunk Y coordinate
	 * @param MapOriginX Map origin X in meters (world position offset)
	 * @param MapOriginY Map origin Y in meters (world position offset)
	 */
	void QueueChunkGeneration(int32 ChunkX, int32 ChunkY, float MapOriginX, float MapOriginY);

	/**
	 * Queue all chunks in a grid for generation.
	 * @param ChunksPerSide Number of chunks per side
	 * @param MapOriginX Map origin X in meters
	 * @param MapOriginY Map origin Y in meters
	 */
	void QueueAllChunks(int32 ChunksPerSide, float MapOriginX, float MapOriginY);

	/**
	 * Get chunks that have completed generation.
	 * Call from main thread only. Removes chunks from internal completed list.
	 * @param OutChunks Array to receive completed chunk data
	 * @return Number of chunks returned
	 */
	int32 GetCompletedChunks(TArray<FLandscapeChunkData>& OutChunks);

	/**
	 * Check if any chunks are still being generated.
	 */
	bool IsGenerating() const { return PendingCount.GetValue() > 0; }

	/**
	 * Get number of pending chunks.
	 */
	int32 GetPendingCount() const { return PendingCount.GetValue(); }

	/**
	 * Cancel all pending generation (best effort - may not stop in-flight tasks).
	 */
	void CancelAllPending();

	/**
	 * Wait for all pending generation to complete (blocking).
	 * Use sparingly - primarily for shutdown.
	 */
	void WaitForCompletion();

private:
	/** Noise service for height generation (not owned) */
	FLandscapeNoiseService* NoiseService = nullptr;

	/** Resolution configuration */
	FLandscapeResolutionConfig Resolution;

	/** Thread-safe list of completed chunks awaiting main thread pickup */
	FCriticalSection CompletedChunksLock;
	TArray<FLandscapeChunkData> CompletedChunks;

	/** Counter for pending generation tasks */
	FThreadSafeCounter PendingCount;

	/** Flag to cancel pending work */
	FThreadSafeBool bCancelRequested;
};
