// BiomeMapGenerator.cpp
// Deterministic biome layout generator - single blob per biome, exact target matching
// BiomeSeed controls biome placement independently from landmass seed
// Version: 01.27.2026.23.51

#include "BiomeMapGenerator.h"
#include <queue>

// ------------------------------------------------------------
// Hash helper for deterministic noise and derived seeds
// ------------------------------------------------------------
static FORCEINLINE uint32 Hash32(uint32 x)
{
	x ^= x >> 16;
	x *= 0x7FEB352Du;
	x ^= x >> 15;
	x *= 0x846CA68Bu;
	x ^= x >> 16;
	return x;
}

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
UBiomeMapGenerator::UBiomeMapGenerator()
{
	Settings.ConnectorBiomeType = EBiomeType::Forest;
}

void UBiomeMapGenerator::Initialize(const FBiomeLayoutSettings& InSettings)
{
	Settings = InSettings;
	// Initialize RandomStream with BiomeSeed (NOT landmass seed)
	RandomStream.Initialize(Settings.BiomeSeed);
}

// ------------------------------------------------------------
// Main entry point
// ------------------------------------------------------------
bool UBiomeMapGenerator::Generate(const TArray<uint8>& LandMask)
{
	const int32 Res = Settings.TextureResolution;
	const int32 Expected = Res * Res;

	if (LandMask.Num() != Expected)
	{
		UE_LOG(LogTemp, Error, TEXT("BiomeMapGenerator::Generate - LandMask size mismatch: expected %d (%dx%d), got %d"),
			Expected, Res, Res, LandMask.Num());
		return false;
	}

	BiomeMap.SetNumUninitialized(Expected);
	
	// Pre-allocate component ID buffer to avoid reallocation per biome
	CompIdPerPixelBuffer.SetNumUninitialized(Expected);
	
	BuildLandIndexList(LandMask);
	FillConnector();

	if (LandIndices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BiomeMapGenerator::Generate - No land pixels."));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("BiomeMapGenerator::Generate - BiomeSeed=%d Res=%d LandPixels=%d Layers=%d"),
		Settings.BiomeSeed, Res, LandIndices.Num(), Settings.Layers.Num());

	CarveBiomesSequentially(LandMask);

	UE_LOG(LogTemp, Log, TEXT("BiomeMapGenerator::Generate - Done."));

	return true;
}

// ------------------------------------------------------------
// Build list of land pixel indices
// ------------------------------------------------------------
void UBiomeMapGenerator::BuildLandIndexList(const TArray<uint8>& LandMask)
{
	LandIndices.Reset();
	LandIndices.Reserve(LandMask.Num() / 2);

	for (int32 i = 0; i < LandMask.Num(); ++i)
	{
		if (LandMask[i] != 0)
		{
			LandIndices.Add(i);
		}
	}
}

// ------------------------------------------------------------
// Fill entire map with connector biome
// ------------------------------------------------------------
void UBiomeMapGenerator::FillConnector()
{
	const int32 ConnectorId = static_cast<int32>(Settings.ConnectorBiomeType);
	for (int32 i = 0; i < BiomeMap.Num(); ++i)
	{
		BiomeMap[i] = ConnectorId;
	}
}

// ------------------------------------------------------------
// Compute connected components of remaining connector land (BFS)
// Uses bUseEightWayAdjacency setting to match biome growth adjacency
// Also computes random representative via reservoir sampling during BFS
// ------------------------------------------------------------
void UBiomeMapGenerator::ComputeConnectorComponents(
	const TArray<uint8>& LandMask,
	int32 ConnectorId,
	TArray<int32>& OutCompIdPerPixel,
	TArray<FConnectorComponent>& OutComponents) const
{
	const int32 Res = Settings.TextureResolution;
	const int32 Total = Res * Res;

	// Reset component ID buffer (reusing pre-allocated buffer)
	OutCompIdPerPixel.SetNumUninitialized(Total);
	for (int32 i = 0; i < Total; ++i)
	{
		OutCompIdPerPixel[i] = -1;
	}
	OutComponents.Reset();

	TArray<int32> Queue;
	Queue.Reserve(1024);

	int32 NextCompId = 0;

	for (int32 StartIdx = 0; StartIdx < Total; ++StartIdx)
	{
		// Skip if not connector land or already visited
		if (LandMask[StartIdx] == 0) continue;
		if (BiomeMap[StartIdx] != ConnectorId) continue;
		if (OutCompIdPerPixel[StartIdx] >= 0) continue;

		// BFS from this pixel
		FConnectorComponent Comp;
		Comp.ComponentId = NextCompId;
		Comp.Size = 0;
		Comp.RepresentativeIndexMin = StartIdx;
		Comp.RepresentativeIndexRandom = StartIdx; // First pixel is initial random rep

		Queue.Reset();
		Queue.Add(StartIdx);
		OutCompIdPerPixel[StartIdx] = NextCompId;

		int32 Head = 0;
		while (Head < Queue.Num())
		{
			const int32 Cur = Queue[Head++];
			Comp.Size++;

			// Track minimum index for deterministic tiebreak
			if (Cur < Comp.RepresentativeIndexMin)
			{
				Comp.RepresentativeIndexMin = Cur;
			}

			// Reservoir sampling: with probability 1/Size, replace random rep
			// This gives uniform random selection over all pixels in component
			if (RandomStream.RandRange(1, Comp.Size) == 1)
			{
				Comp.RepresentativeIndexRandom = Cur;
			}

			int32 X, Y;
			XY(Cur, Res, X, Y);

			// Neighbor helper - adds valid connector land neighbors to queue
			auto TryNeighbor = [&](int32 nx, int32 ny)
			{
				if ((uint32)nx >= (uint32)Res || (uint32)ny >= (uint32)Res) return;
				const int32 nIdx = Idx(nx, ny, Res);
				if (LandMask[nIdx] == 0) return;
				if (BiomeMap[nIdx] != ConnectorId) return;
				if (OutCompIdPerPixel[nIdx] >= 0) return;

				OutCompIdPerPixel[nIdx] = NextCompId;
				Queue.Add(nIdx);
			};

			// 4-way neighbors in fixed order (deterministic traversal): left, right, up, down
			TryNeighbor(X - 1, Y);
			TryNeighbor(X + 1, Y);
			TryNeighbor(X, Y - 1);
			TryNeighbor(X, Y + 1);

			// 8-way: add diagonals in fixed order if enabled
			if (Settings.bUseEightWayAdjacency)
			{
				TryNeighbor(X - 1, Y - 1); // up-left
				TryNeighbor(X + 1, Y - 1); // up-right
				TryNeighbor(X - 1, Y + 1); // down-left
				TryNeighbor(X + 1, Y + 1); // down-right
			}
		}

		OutComponents.Add(Comp);
		NextCompId++;
	}
}

// ------------------------------------------------------------
// Choose component for a biome with given TargetCount
// If multiple components fit, can choose randomly (deterministic from BiomeSeed)
// ------------------------------------------------------------
int32 UBiomeMapGenerator::ChooseComponentForBiome(
	const TArray<FConnectorComponent>& Components,
	int32 TargetCount,
	int32 BiomeId,
	int32 CarveOrder) const
{
	if (Components.Num() == 0)
	{
		return -1;
	}

	// Build list of candidate components that can fit TargetCount
	TArray<int32> CandidateIndices;
	CandidateIndices.Reserve(Components.Num());

	// Also track largest overall (for fallback if none fit)
	int32 LargestIdx = -1;
	int32 LargestSize = 0;
	int32 LargestRep = INT32_MAX;

	for (int32 i = 0; i < Components.Num(); ++i)
	{
		const FConnectorComponent& C = Components[i];

		// Track largest overall (tiebreak by min representative index)
		if (C.Size > LargestSize || (C.Size == LargestSize && C.RepresentativeIndexMin < LargestRep))
		{
			LargestIdx = i;
			LargestSize = C.Size;
			LargestRep = C.RepresentativeIndexMin;
		}

		// Add to candidates if it can fit TargetCount
		if (C.Size >= TargetCount)
		{
			CandidateIndices.Add(i);
		}
	}

	// If no candidates fit, return largest (will cause underflow)
	if (CandidateIndices.Num() == 0)
	{
		return LargestIdx;
	}

	// If only one candidate, return it
	if (CandidateIndices.Num() == 1)
	{
		return CandidateIndices[0];
	}

	// Multiple candidates - choose based on settings
	if (!Settings.bRandomizeComponentChoice)
	{
		// Deterministic: pick largest fitting component (old behavior)
		int32 BestFitIdx = -1;
		int32 BestFitSize = 0;
		int32 BestFitRep = INT32_MAX;

		for (int32 CandIdx : CandidateIndices)
		{
			const FConnectorComponent& C = Components[CandIdx];
			if (C.Size > BestFitSize || (C.Size == BestFitSize && C.RepresentativeIndexMin < BestFitRep))
			{
				BestFitIdx = CandIdx;
				BestFitSize = C.Size;
				BestFitRep = C.RepresentativeIndexMin;
			}
		}
		return BestFitIdx;
	}

	// Random choice among candidates (deterministic from BiomeSeed + BiomeId + CarveOrder)
	FRandomStream ChoiceRng;
	ChoiceRng.Initialize(Hash32((uint32)Settings.BiomeSeed ^ ((uint32)BiomeId * 2654435761u) ^ ((uint32)CarveOrder * 1013904223u)));

	if (Settings.bWeightComponentChoiceBySize)
	{
		// Weighted random by size (roulette wheel selection)
		int64 TotalSize = 0;
		for (int32 CandIdx : CandidateIndices)
		{
			TotalSize += Components[CandIdx].Size;
		}

		if (TotalSize > 0)
		{
			int64 Pick = ChoiceRng.RandRange(0, (int32)(TotalSize - 1));
			int64 Cumulative = 0;
			for (int32 CandIdx : CandidateIndices)
			{
				Cumulative += Components[CandIdx].Size;
				if (Pick < Cumulative)
				{
					return CandIdx;
				}
			}
		}

		// Fallback (shouldn't reach here)
		return CandidateIndices[0];
	}
	else
	{
		// Uniform random among candidates
		int32 Pick = ChoiceRng.RandRange(0, CandidateIndices.Num() - 1);
		return CandidateIndices[Pick];
	}
}


// ------------------------------------------------------------
// Noise helpers
// ------------------------------------------------------------
float UBiomeMapGenerator::ValueNoise2D(float X, float Y, uint32 Seed)
{
	const int32 Xi = FMath::FloorToInt(X);
	const int32 Yi = FMath::FloorToInt(Y);

	uint32 h = static_cast<uint32>(Xi) * 374761393u + static_cast<uint32>(Yi) * 668265263u;
	h ^= Seed * 1442695040888963407ull;
	h = Hash32(h);

	const float v01 = (h & 0xFFFFFFu) / 16777215.0f;
	return v01 * 2.0f - 1.0f;
}

float UBiomeMapGenerator::SmoothNoise2D(float X, float Y, float Frequency, uint32 Seed)
{
	const float fx = X * Frequency;
	const float fy = Y * Frequency;

	const int32 x0 = FMath::FloorToInt(fx);
	const int32 y0 = FMath::FloorToInt(fy);
	const int32 x1 = x0 + 1;
	const int32 y1 = y0 + 1;

	const float tx = fx - x0;
	const float ty = fy - y0;

	const float sx = tx * tx * (3.0f - 2.0f * tx);
	const float sy = ty * ty * (3.0f - 2.0f * ty);

	const float v00 = ValueNoise2D((float)x0, (float)y0, Seed);
	const float v10 = ValueNoise2D((float)x1, (float)y0, Seed);
	const float v01 = ValueNoise2D((float)x0, (float)y1, Seed);
	const float v11 = ValueNoise2D((float)x1, (float)y1, Seed);

	const float ix0 = FMath::Lerp(v00, v10, sx);
	const float ix1 = FMath::Lerp(v01, v11, sx);
	return FMath::Lerp(ix0, ix1, sy);
}

// ------------------------------------------------------------
// Sequential carve: process biomes one at a time
// All randomness derives from BiomeSeed
// ------------------------------------------------------------
void UBiomeMapGenerator::CarveBiomesSequentially(const TArray<uint8>& LandMask)
{
	const int32 ConnectorId = static_cast<int32>(Settings.ConnectorBiomeType);
	const int32 LandCount = LandIndices.Num();

	if (Settings.Layers.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BiomeMapGenerator - No layers to carve; all land stays connector."));
		return;
	}

	// ---------- Largest-remainder allocation ----------
	struct FLayerAlloc
	{
		int32 LayerIndex;
		int32 BiomeId;
		float Percentage;
		int32 TargetCount;
		float Remainder;
		float NoiseFreq;
		float NoiseAmp;
		uint32 NoiseSeed; // derived from BiomeSeed
	};

	TArray<FLayerAlloc> Allocs;
	Allocs.Reserve(Settings.Layers.Num());

	float TotalPercent = 0.0f;
	for (int32 i = 0; i < Settings.Layers.Num(); ++i)
	{
		const FBiomeLayerSettings& L = Settings.Layers[i];
		TotalPercent += FMath::Clamp(L.TargetPercentOfLand, 0.0f, 100.0f);
	}

	// Normalize if > 100%
	const float NormScale = (TotalPercent > 100.0f) ? (100.0f / TotalPercent) : 1.0f;

	int32 SumFloor = 0;
	for (int32 i = 0; i < Settings.Layers.Num(); ++i)
	{
		const FBiomeLayerSettings& L = Settings.Layers[i];
		FLayerAlloc A;
		A.LayerIndex = i;
		A.BiomeId = static_cast<int32>(L.BiomeType);
		A.Percentage = FMath::Clamp(L.TargetPercentOfLand, 0.0f, 100.0f) * NormScale;
		
		const float Exact = (A.Percentage / 100.0f) * (float)LandCount;
		A.TargetCount = FMath::FloorToInt(Exact);
		A.Remainder = Exact - (float)A.TargetCount;
		A.NoiseFreq = L.SpreadNoiseFrequency;
		A.NoiseAmp = L.SpreadNoiseAmplitude;
		
		// Derive noise seed from BiomeSeed (NOT landmass seed)
		A.NoiseSeed = Hash32((uint32)Settings.BiomeSeed ^ (uint32)A.BiomeId * 2654435761u ^ (uint32)i * 1013904223u);

		SumFloor += A.TargetCount;
		Allocs.Add(A);
	}

	// Distribute remainder pixels to layers with largest fractional part
	int32 Remainder = LandCount - SumFloor;
	if (Remainder < 0) Remainder = 0; // safety

	// Sort by remainder descending (deterministic tiebreak by BiomeId)
	Allocs.Sort([](const FLayerAlloc& A, const FLayerAlloc& B)
	{
		if (!FMath::IsNearlyEqual(A.Remainder, B.Remainder, 0.0001f))
			return A.Remainder > B.Remainder;
		return A.BiomeId < B.BiomeId;
	});

	for (int32 i = 0; i < Remainder && i < Allocs.Num(); ++i)
	{
		Allocs[i].TargetCount += 1;
	}

	// ---------- Sort by TargetCount descending for carving order ----------
	Allocs.Sort([](const FLayerAlloc& A, const FLayerAlloc& B)
	{
		if (A.TargetCount != B.TargetCount)
			return A.TargetCount > B.TargetCount;
		return A.BiomeId < B.BiomeId; // deterministic tiebreak
	});

	// ---------- Carve each biome sequentially ----------
	int32 CarveOrder = 0;
	for (const FLayerAlloc& A : Allocs)
	{
		if (A.TargetCount <= 0)
		{
			UE_LOG(LogTemp, Log, TEXT("  Biome %d: TargetCount=0, skipped."), A.BiomeId);
			CarveOrder++;
			continue;
		}

		// Compute connected components of remaining connector land
		TArray<int32> CompIdPerPixel;
		TArray<FConnectorComponent> Components;
		ComputeConnectorComponents(LandMask, ConnectorId, CompIdPerPixel, Components);

		if (Components.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Biome %d: No connector land left. TargetCount=%d, Achieved=0"),
				A.BiomeId, A.TargetCount);
			CarveOrder++;
			continue;
		}

		// Choose best component for this biome (may randomize among fitting components)
		const int32 ChosenCompIdx = ChooseComponentForBiome(Components, A.TargetCount, A.BiomeId, CarveOrder);
		if (ChosenCompIdx < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Biome %d: Failed to choose component. TargetCount=%d, Achieved=0"),
				A.BiomeId, A.TargetCount);
			CarveOrder++;
			continue;
		}

		const FConnectorComponent& ChosenComp = Components[ChosenCompIdx];

		// Warn if component is too small
		if (ChosenComp.Size < A.TargetCount)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Biome %d: Component size %d < TargetCount %d (underflow expected)"),
				A.BiomeId, ChosenComp.Size, A.TargetCount);
		}

		// Use pre-computed random seed index from reservoir sampling (O(1) lookup)
		const int32 SeedIdx = ChosenComp.RepresentativeIndexRandom;
		if (SeedIdx < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Biome %d: Invalid random seed in component. TargetCount=%d, Achieved=0"),
				A.BiomeId, A.TargetCount);
			CarveOrder++;
			continue;
		}

		const int32 Painted = GrowSingleBiome(
			LandMask,
			SeedIdx,
			A.BiomeId,
			A.TargetCount,
			ConnectorId,
			A.NoiseFreq,
			A.NoiseAmp,
			A.NoiseSeed);

		const float AchievedPct = (LandCount > 0) ? (100.0f * (float)Painted / (float)LandCount) : 0.0f;

		if (Painted < A.TargetCount)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Biome %d: TargetCount=%d, Achieved=%d (%.2f%%) - UNDERFLOW"),
				A.BiomeId, A.TargetCount, Painted, AchievedPct);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("  Biome %d: TargetCount=%d, Achieved=%d (%.2f%%)"),
				A.BiomeId, A.TargetCount, Painted, AchievedPct);
		}

		CarveOrder++;
	}
}

// ------------------------------------------------------------
// Grow a single biome from one seed using Dijkstra with noise cost
// ------------------------------------------------------------
struct FGrowNode
{
	int32 CostQ = 0;
	int32 Index = 0;

	bool operator<(const FGrowNode& Other) const
	{
		if (CostQ != Other.CostQ) return CostQ > Other.CostQ; // min-heap via reverse
		return Index > Other.Index; // deterministic tiebreak
	}
};

int32 UBiomeMapGenerator::GrowSingleBiome(
	const TArray<uint8>& LandMask,
	int32 SeedIndex,
	int32 BiomeId,
	int32 TargetCount,
	int32 ConnectorId,
	float NoiseFreq,
	float NoiseAmp,
	uint32 NoiseSeed)
{
	const int32 Res = Settings.TextureResolution;
	int32 Painted = 0;

	std::priority_queue<FGrowNode> PQ;

	// Seed the growth
	{
		FGrowNode N;
		N.CostQ = 0;
		N.Index = SeedIndex;
		PQ.push(N);
	}

	auto GetNeighbors = [&](int32 Index, TArray<int32>& Out)
	{
		Out.Reset();
		int32 X, Y;
		XY(Index, Res, X, Y);

		auto PushIfValid = [&](int32 nx, int32 ny)
		{
			if ((uint32)nx < (uint32)Res && (uint32)ny < (uint32)Res)
			{
				Out.Add(Idx(nx, ny, Res));
			}
		};

		PushIfValid(X - 1, Y);
		PushIfValid(X + 1, Y);
		PushIfValid(X, Y - 1);
		PushIfValid(X, Y + 1);

		if (Settings.bUseEightWayAdjacency)
		{
			PushIfValid(X - 1, Y - 1);
			PushIfValid(X + 1, Y - 1);
			PushIfValid(X - 1, Y + 1);
			PushIfValid(X + 1, Y + 1);
		}
	};

	TArray<int32> Neigh;
	Neigh.Reserve(8);

	while (!PQ.empty() && Painted < TargetCount)
	{
		const FGrowNode Cur = PQ.top();
		PQ.pop();

		const int32 idx = Cur.Index;

		// Must be land and still connector
		if (LandMask[idx] == 0) continue;
		if (BiomeMap[idx] != ConnectorId) continue;

		// Paint it
		BiomeMap[idx] = BiomeId;
		Painted++;

		if (Painted >= TargetCount)
			break;

		// Expand frontier
		GetNeighbors(idx, Neigh);

		int32 X, Y;
		XY(idx, Res, X, Y);

		for (int32 nIdx : Neigh)
		{
			if (LandMask[nIdx] == 0) continue;
			if (BiomeMap[nIdx] != ConnectorId) continue;

			int32 nX, nY;
			XY(nIdx, Res, nX, nY);

			const float nnx = (Res > 1) ? (float)nX / (float)(Res - 1) : 0.0f;
			const float nny = (Res > 1) ? (float)nY / (float)(Res - 1) : 0.0f;

			const float noise = SmoothNoise2D(nnx, nny, NoiseFreq, NoiseSeed);
			const float noise01 = (noise * 0.5f) + 0.5f;

			const bool bDiagonal = (FMath::Abs(nX - X) == 1 && FMath::Abs(nY - Y) == 1);
			const float step = bDiagonal ? 1.41421356f : 1.0f;

			const float cost = ((float)Cur.CostQ / 1000.0f) + step + (noise01 * NoiseAmp);

			FGrowNode Next;
			Next.Index = nIdx;
			Next.CostQ = FMath::Clamp((int32)FMath::RoundToInt(cost * 1000.0f), 0, INT32_MAX);

			PQ.push(Next);
		}
	}

	return Painted;
}
