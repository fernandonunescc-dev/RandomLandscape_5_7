#include "HydrologyGenerator.h"
#include "NoiseUtility.h"

//------------------------------------------------------------------------------
// D8 neighbor offsets: E, NE, N, NW, W, SW, S, SE
//------------------------------------------------------------------------------
static constexpr int32 DX8[] = {  1,  1,  0, -1, -1, -1,  0,  1 };
static constexpr int32 DY8[] = {  0, -1, -1, -1,  0,  1,  1,  1 };
static constexpr float DiagDist = 1.41421356f; // sqrt(2)
static constexpr float Dist8[] = { 1.f, DiagDist, 1.f, DiagDist, 1.f, DiagDist, 1.f, DiagDist };

//------------------------------------------------------------------------------
// Initialize: store settings, derive seed, record resolution
//------------------------------------------------------------------------------
void UHydrologyGenerator::Initialize(const FHydrologySettings& InSettings, int32 GlobalSeed, int32 TextureResolution)
{
	Settings = InSettings;

	ActualSeed = (Settings.Seed != 0)
		? Settings.Seed
		: WorldNoise::DeriveSeed(GlobalSeed, 3);

	Resolution = TextureResolution;

	UE_LOG(LogTemp, Log,
		TEXT("HydrologyGenerator initialized – Resolution: %d, Seed: %d, RiverThresh: %.4f, FlowJitter: %.4f"),
		Resolution, ActualSeed, Settings.RiverThreshold, Settings.FlowJitter);
}

//------------------------------------------------------------------------------
// Generate: run the full hydrology pipeline
//------------------------------------------------------------------------------
bool UHydrologyGenerator::Generate(const TArray<float>& Elevation, const TArray<uint8>& LandMask)
{
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("HydrologyGenerator::Generate – Resolution not set. Call Initialize first."));
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;

	FlowDirection.SetNum(TotalPixels);
	FlowAccumulation.SetNum(TotalPixels);
	RiverMap.SetNum(TotalPixels);
	LakeMap.SetNum(TotalPixels);

	UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator::Generate – starting (%d pixels)"), TotalPixels);

	ComputeFlowDirections(Elevation, LandMask);
	ComputeFlowAccumulation();
	IdentifyRivers();
	IdentifyLakes(Elevation, LandMask);

	UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator::Generate – complete"));
	return true;
}

//------------------------------------------------------------------------------
// ComputeFlowDirections (D8):
//   For each land pixel, find the 8-connected neighbor with the steepest
//   downhill slope.  Small noise jitter is added to the elevation before
//   comparing neighbors so that rivers do not follow perfectly straight lines.
//------------------------------------------------------------------------------
void UHydrologyGenerator::ComputeFlowDirections(const TArray<float>& Elevation, const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const float Jitter = Settings.FlowJitter;
	const float InvRes = 1.0f / FMath::Max(Resolution - 1, 1);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		FlowDirection[i] = -1; // default: no downhill neighbor

		if (LandMask[i] == 0)
		{
			continue;
		}

		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;

		// Jittered elevation for this pixel
		const float NormX = static_cast<float>(X) * InvRes;
		const float NormY = static_cast<float>(Y) * InvRes;
		const float CenterElev = Elevation[i]
			+ WorldNoise::Noise2D(NormX * 50.0f, NormY * 50.0f, ActualSeed) * Jitter;

		float SteepestSlope = 0.0f;
		int32 BestDir = -1;

		for (int32 Dir = 0; Dir < 8; ++Dir)
		{
			const int32 NX = X + DX8[Dir];
			const int32 NY = Y + DY8[Dir];

			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
			{
				continue;
			}

			const int32 NIdx = NY * Resolution + NX;

			// Jittered neighbor elevation
			const float NeighNormX = static_cast<float>(NX) * InvRes;
			const float NeighNormY = static_cast<float>(NY) * InvRes;
			const float NeighElev = Elevation[NIdx]
				+ WorldNoise::Noise2D(NeighNormX * 50.0f, NeighNormY * 50.0f, ActualSeed) * Jitter;

			const float Drop = CenterElev - NeighElev;

			if (Drop > 0.0f)
			{
				const float Slope = Drop / Dist8[Dir];

				if (Slope > SteepestSlope)
				{
					SteepestSlope = Slope;
					BestDir = Dir;
				}
			}
		}

		FlowDirection[i] = BestDir;
	}

	UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator – FlowDirections computed (jitter %.4f)"), Jitter);
}

//------------------------------------------------------------------------------
// ComputeFlowAccumulation:
//   Sort all land pixels by elevation (high to low).  Process in that order:
//   each pixel starts with 1.0 flow, then passes its accumulated total to
//   the downstream neighbor indicated by FlowDirection.
//------------------------------------------------------------------------------
void UHydrologyGenerator::ComputeFlowAccumulation()
{
	const int32 TotalPixels = Resolution * Resolution;

	// Initialize every pixel to 1.0 (itself as drainage area)
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		FlowAccumulation[i] = 1.0f;
	}

	// Build index list of all pixels that have a valid flow direction
	TArray<int32> SortedIndices;
	SortedIndices.Reserve(TotalPixels);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (FlowDirection[i] != -1)
		{
			SortedIndices.Add(i);
		}
	}

	// Sort by flow accumulation seed elevation — we reuse FlowAccumulation
	// later, so we need a separate elevation reference.  Instead, sort by
	// pixel index mapped back to original elevation.  We don't have the
	// elevation array here, but we can use a topological sort approach:
	// count in-degrees and process sources first... however the simpler
	// approach of sorting by descending elevation is standard.
	//
	// Since we don't store the elevation array as a member, we use a
	// topological ordering via in-degree counting which is equivalent.

	// Build in-degree array
	TArray<int32> InDegree;
	InDegree.SetNumZeroed(TotalPixels);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (FlowDirection[i] >= 0)
		{
			const int32 X = i % Resolution;
			const int32 Y = i / Resolution;
			const int32 NX = X + DX8[FlowDirection[i]];
			const int32 NY = Y + DY8[FlowDirection[i]];

			if (NX >= 0 && NX < Resolution && NY >= 0 && NY < Resolution)
			{
				InDegree[NY * Resolution + NX]++;
			}
		}
	}

	// Topological sort via Kahn's algorithm
	TQueue<int32> Queue;
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (InDegree[i] == 0 && FlowDirection[i] != -1)
		{
			Queue.Enqueue(i);
		}
		// Also enqueue pixels with FlowDirection == -1 and InDegree == 0
		// (isolated ocean pixels or flat land with no upstream) — they just
		// won't propagate, so we skip them.
	}

	while (!Queue.IsEmpty())
	{
		int32 Idx;
		Queue.Dequeue(Idx);

		if (FlowDirection[Idx] < 0)
		{
			continue;
		}

		const int32 X = Idx % Resolution;
		const int32 Y = Idx / Resolution;
		const int32 NX = X + DX8[FlowDirection[Idx]];
		const int32 NY = Y + DY8[FlowDirection[Idx]];

		if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
		{
			continue;
		}

		const int32 DownIdx = NY * Resolution + NX;
		FlowAccumulation[DownIdx] += FlowAccumulation[Idx];

		InDegree[DownIdx]--;
		if (InDegree[DownIdx] == 0)
		{
			Queue.Enqueue(DownIdx);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator – FlowAccumulation computed"));
}

//------------------------------------------------------------------------------
// IdentifyRivers:
//   Pixels whose accumulation exceeds RiverThreshold * maxAccum are rivers.
//   Strength is log-normalized so wider rivers have higher values.
//------------------------------------------------------------------------------
void UHydrologyGenerator::IdentifyRivers()
{
	const int32 TotalPixels = Resolution * Resolution;

	// Find the maximum accumulation across all pixels
	float MaxAccum = 0.0f;
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (FlowAccumulation[i] > MaxAccum)
		{
			MaxAccum = FlowAccumulation[i];
		}
	}

	if (MaxAccum <= 1.0f)
	{
		FMemory::Memzero(RiverMap.GetData(), TotalPixels * sizeof(float));
		UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator – No significant flow accumulation; no rivers"));
		return;
	}

	const float Threshold = Settings.RiverThreshold;
	const float LogMax = FMath::Loge(MaxAccum);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		const float FracAccum = FlowAccumulation[i] / MaxAccum;

		if (FracAccum > Threshold)
		{
			RiverMap[i] = FMath::Loge(FlowAccumulation[i]) / LogMax;
		}
		else
		{
			RiverMap[i] = 0.0f;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator – Rivers identified (threshold %.4f, maxAccum %.1f)"),
		Threshold, MaxAccum);
}

//------------------------------------------------------------------------------
// IdentifyLakes:
//   Local minima (FlowDirection == -1) with accumulation above a threshold
//   are lake seeds.  BFS expand them a few steps to give lakes area.
//------------------------------------------------------------------------------
void UHydrologyGenerator::IdentifyLakes(const TArray<float>& Elevation, const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;

	FMemory::Memzero(LakeMap.GetData(), TotalPixels * sizeof(uint8));

	// Determine max accumulation for threshold comparison
	float MaxAccum = 0.0f;
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (FlowAccumulation[i] > MaxAccum)
		{
			MaxAccum = FlowAccumulation[i];
		}
	}

	if (MaxAccum <= 1.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator – No significant accumulation; no lakes"));
		return;
	}

	const float LakeThresholdAccum = Settings.LakeThreshold * MaxAccum;
	constexpr int32 LakeExpandSteps = 3;

	// Seed: local minima on land with enough flow
	TQueue<int32> Queue;
	int32 LakeSeeds = 0;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] != 0 && FlowDirection[i] == -1 && FlowAccumulation[i] > LakeThresholdAccum)
		{
			LakeMap[i] = 1;
			Queue.Enqueue(i);
			LakeSeeds++;
		}
	}

	// BFS expand lake pixels a few steps
	int32 StepsRemaining = LakeExpandSteps;
	while (StepsRemaining > 0 && !Queue.IsEmpty())
	{
		// Process all pixels at the current BFS frontier
		int32 FrontierSize = 0;
		TArray<int32> CurrentFrontier;

		while (!Queue.IsEmpty())
		{
			int32 Idx;
			Queue.Dequeue(Idx);
			CurrentFrontier.Add(Idx);
		}

		for (int32 Idx : CurrentFrontier)
		{
			const int32 X = Idx % Resolution;
			const int32 Y = Idx / Resolution;

			for (int32 Dir = 0; Dir < 8; ++Dir)
			{
				const int32 NX = X + DX8[Dir];
				const int32 NY = Y + DY8[Dir];

				if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
				{
					continue;
				}

				const int32 NIdx = NY * Resolution + NX;

				if (LakeMap[NIdx] == 0 && LandMask[NIdx] != 0)
				{
					LakeMap[NIdx] = 1;
					Queue.Enqueue(NIdx);
				}
			}
		}

		StepsRemaining--;
	}

	UE_LOG(LogTemp, Log, TEXT("HydrologyGenerator – Lakes identified (%d seeds, expanded %d steps)"),
		LakeSeeds, LakeExpandSteps);
}
