// BiomeMapGenerator.cpp
// Deterministic biome layout generator - single blob per biome, exact target matching
// BiomeSeed controls biome placement independently from landmass seed
// Land is the connector biome (gets remainder after carving)

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
	// LayoutSettings has default constructor that sets up default biomes
}

void UBiomeMapGenerator::Initialize()
{
	// Use the LayoutSettings from the UPROPERTY
	Settings = LayoutSettings;
	RandomStream.Initialize(Settings.BiomeSeed);
}

void UBiomeMapGenerator::Initialize(const FBiomeLayoutSettings& InSettings)
{
	Settings = InSettings;
	// Also update LayoutSettings so they stay in sync
	LayoutSettings = InSettings;
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
	
	BuildLandIndexList(LandMask);
	FillConnector();
	NormalizeLayerPercentages();

	if (LandIndices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BiomeMapGenerator::Generate - No land pixels."));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("BiomeMapGenerator::Generate - BiomeSeed=%d Res=%d LandPixels=%d Layers=%d"),
		Settings.BiomeSeed, Res, LandIndices.Num(), Settings.Layers.Num());

	CarveBiomesSequentially(LandMask);

	// Log final counts by scanning BiomeMap (no post-processing)
	LogFinalBiomeCounts(LandMask);

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
// Fill entire map with connector biome (Land) on land, UnassignedId elsewhere
// ------------------------------------------------------------
void UBiomeMapGenerator::FillConnector()
{
	// Ocean pixels stay as UnassignedId (will be colored OceanColor)
	// Land pixels filled with Land (connector)
	for (int32 i = 0; i < BiomeMap.Num(); ++i)
	{
		BiomeMap[i] = UnassignedId;
	}
	for (int32 idx : LandIndices)
	{
		BiomeMap[idx] = ConnectorBiomeId;
	}
}

// ------------------------------------------------------------
// Normalize layer percentages to sum to 100
// If empty, add default Forest 100%
// ------------------------------------------------------------
void UBiomeMapGenerator::NormalizeLayerPercentages()
{
	if (Settings.Layers.Num() == 0)
	{
		// Default: single Land covering all land
		Settings.Layers.Add(FBiomeLayerSettings(EBiomeType::Land, TEXT("Land"), 100.0f, FLinearColor(0.6f, 0.8f, 0.3f, 1.0f)));
		return;
	}

	float TotalPercent = 0.0f;
	for (const FBiomeLayerSettings& L : Settings.Layers)
	{
		TotalPercent += FMath::Max(0.0f, L.TargetPercentOfLand);
	}

	if (TotalPercent <= 0.0f)
	{
		// All zeros - distribute evenly
		const float EvenShare = 100.0f / (float)Settings.Layers.Num();
		for (FBiomeLayerSettings& L : Settings.Layers)
		{
			L.TargetPercentOfLand = EvenShare;
		}
	}
	else if (!FMath::IsNearlyEqual(TotalPercent, 100.0f, 0.01f))
	{
		// Normalize to 100%
		const float Scale = 100.0f / TotalPercent;
		for (FBiomeLayerSettings& L : Settings.Layers)
		{
			L.TargetPercentOfLand = FMath::Max(0.0f, L.TargetPercentOfLand) * Scale;
		}
	}
}

// ------------------------------------------------------------
// Pick a deterministic random seed index from connector (Land) land
// Uses BiomeSeed + BiomeId for deterministic selection
// ------------------------------------------------------------
int32 UBiomeMapGenerator::PickRandomConnectorIndex(const TArray<uint8>& LandMask, int32 BiomeId) const
{
	// Build list of connector (Land) land pixels
	TArray<int32> ConnectorIndices;
	ConnectorIndices.Reserve(LandIndices.Num());

	for (int32 idx : LandIndices)
	{
		if (BiomeMap[idx] == ConnectorBiomeId)
		{
			ConnectorIndices.Add(idx);
		}
	}

	if (ConnectorIndices.Num() == 0)
	{
		return -1;
	}

	// Deterministic random selection based on BiomeSeed + BiomeId
	FRandomStream SeedRng;
	SeedRng.Initialize(Hash32((uint32)Settings.BiomeSeed ^ ((uint32)BiomeId * 2654435761u)));
	const int32 Pick = SeedRng.RandRange(0, ConnectorIndices.Num() - 1);
	return ConnectorIndices[Pick];
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
// Sequential carve: process non-Land biomes one at a time
// Land is the connector biome and gets remainder after carving
// All randomness derives from BiomeSeed
// ------------------------------------------------------------
void UBiomeMapGenerator::CarveBiomesSequentially(const TArray<uint8>& LandMask)
{
	const int32 LandCount = LandIndices.Num();

	if (Settings.Layers.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BiomeMapGenerator - No layers to carve; all land stays Land."));
		return;
	}

	// ---------- Largest-remainder allocation for non-Land biomes ----------
	struct FLayerAlloc
	{
		int32 LayerIndex;
		int32 BiomeId;
		float Percentage;
		int32 TargetCount;
		float Remainder;
		uint32 NoiseSeed; // derived from BiomeSeed
	};

	TArray<FLayerAlloc> Allocs;
	Allocs.Reserve(Settings.Layers.Num());

	// Percentages are already normalized by NormalizeLayerPercentages()
	int32 SumFloor = 0;
	for (int32 i = 0; i < Settings.Layers.Num(); ++i)
	{
		const FBiomeLayerSettings& L = Settings.Layers[i];
		
		// Skip Land - it's the connector and gets remainder
		if (L.BiomeType == EBiomeType::Land)
		{
			continue;
		}

		FLayerAlloc A;
		A.LayerIndex = i;
		A.BiomeId = static_cast<int32>(L.BiomeType);
		A.Percentage = FMath::Clamp(L.TargetPercentOfLand, 0.0f, 100.0f);
		
		const float Exact = (A.Percentage / 100.0f) * (float)LandCount;
		A.TargetCount = FMath::FloorToInt(Exact);
		A.Remainder = Exact - (float)A.TargetCount;
		
		// Derive noise seed from BiomeSeed (NOT landmass seed)
		A.NoiseSeed = Hash32((uint32)Settings.BiomeSeed ^ (uint32)A.BiomeId * 2654435761u ^ (uint32)i * 1013904223u);


		SumFloor += A.TargetCount;
		Allocs.Add(A);
	}

	// Distribute remainder pixels to non-Land biomes with largest fractional part
	// Remainder = floor(total exact) - sum of floors
	float TotalExact = 0.0f;
	for (const FLayerAlloc& A : Allocs)
	{
		TotalExact += (A.Percentage / 100.0f) * (float)LandCount;
	}
	int32 Remainder = FMath::FloorToInt(TotalExact) - SumFloor;
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

	// ---------- Carve each non-Land biome sequentially from connector (Land) ----------
	int32 CarveOrder = 0;
	for (const FLayerAlloc& A : Allocs)
	{
		if (A.TargetCount <= 0)
		{
			UE_LOG(LogTemp, Log, TEXT("  Biome %d: TargetCount=0, skipped."), A.BiomeId);
			CarveOrder++;
			continue;
		}

		// Pick a random seed from connector (Land) land
		const int32 SeedIdx = PickRandomConnectorIndex(LandMask, A.BiomeId);

		if (SeedIdx < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("  Biome %d: No connector land left. TargetCount=%d, Achieved=0"),
				A.BiomeId, A.TargetCount);
			CarveOrder++;
			continue;
		}

		const int32 Painted = GrowSingleBiome(
			LandMask,
			SeedIdx,
			A.BiomeId,
			A.TargetCount,
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
// Only paints on connector (Land) cells
// Uses domain warp + two signed noise layers for organic, non-circular borders
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
	uint32 NoiseSeed)
{
	// Hardcoded noise constants for organic, non-circular borders
	constexpr float LargeFreq = 2.0f;
	constexpr float LargeAmp  = 0.9f;   // signed
	constexpr float SmallFreq = 12.0f;
	constexpr float SmallAmp  = 0.6f;   // signed
	constexpr float WarpFreq  = 4.0f;
	constexpr float WarpAmp   = 0.03f;

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

		if (bUseEightWayAdjacency)
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

		// Must be land and still connector (Forest)
		if (LandMask[idx] == 0) continue;
		if (BiomeMap[idx] != ConnectorBiomeId) continue;

		// Paint it
		BiomeMap[idx] = BiomeId;
		Painted++;

		if (Painted >= TargetCount)
			break;

		// Expand frontier
		GetNeighbors(idx, Neigh);

		for (int32 nIdx : Neigh)
		{
			if (LandMask[nIdx] == 0) continue;
			if (BiomeMap[nIdx] != ConnectorBiomeId) continue;

			int32 nX, nY;
			XY(nIdx, Res, nX, nY);

			const float nnx = (Res > 1) ? (float)nX / (float)(Res - 1) : 0.0f;
			const float nny = (Res > 1) ? (float)nY / (float)(Res - 1) : 0.0f;

			// Domain warp for more irregular shapes
			const float wx = SmoothNoise2D(nnx, nny, WarpFreq, NoiseSeed ^ 0xA341316Cu) * WarpAmp;
			const float wy = SmoothNoise2D(nnx, nny, WarpFreq, NoiseSeed ^ 0xC8013EA4u) * WarpAmp;
			const float u = nnx + wx;
			const float v = nny + wy;

			// Two signed noise layers: large for bulges, small for detail
			const float large = SmoothNoise2D(u, v, LargeFreq, NoiseSeed);
			const float small = SmoothNoise2D(u, v, SmallFreq, NoiseSeed ^ 0x9E3779B9u);

			const float perturb = (large * LargeAmp) + (small * SmallAmp);
			const float mult = FMath::Clamp(1.0f + perturb, 0.25f, 2.5f);

			// Diagonal step cost
			int32 curX, curY;
			XY(idx, Res, curX, curY);
			const bool bDiagonal = (FMath::Abs(nX - curX) == 1 && FMath::Abs(nY - curY) == 1);
			const float step = bDiagonal ? 1.41421356f : 1.0f;

			const float cost = ((float)Cur.CostQ / 1000.0f) + step * mult;

			FGrowNode Next;
			Next.Index = nIdx;
			Next.CostQ = FMath::Clamp((int32)FMath::RoundToInt(cost * 1000.0f), 0, INT32_MAX);

			PQ.push(Next);
		}
	}

	return Painted;
}

// ------------------------------------------------------------
// Log final biome counts by scanning BiomeMap
// ------------------------------------------------------------
void UBiomeMapGenerator::LogFinalBiomeCounts(const TArray<uint8>& LandMask) const
{
	const int32 LandCount = LandIndices.Num();
	
	// Count pixels per biome type
	TMap<int32, int32> BiomeCounts;
	for (int32 idx : LandIndices)
	{
		const int32 BiomeId = BiomeMap[idx];
		BiomeCounts.FindOrAdd(BiomeId)++;
	}

	UE_LOG(LogTemp, Log, TEXT("BiomeMapGenerator - Final biome counts (scanned from BiomeMap):"));
	
	for (const FBiomeLayerSettings& Layer : Settings.Layers)
	{
		const int32 BiomeId = static_cast<int32>(Layer.BiomeType);
		const int32 Count = BiomeCounts.FindRef(BiomeId);
		const float Pct = (LandCount > 0) ? (100.0f * (float)Count / (float)LandCount) : 0.0f;
		
		UE_LOG(LogTemp, Log, TEXT("  %s (id=%d): %d pixels (%.2f%%), target was %.1f%%"),
			*Layer.DisplayName, BiomeId, Count, Pct, Layer.TargetPercentOfLand);
	}
}

