#include "BiomeAssignmentGenerator.h"
#include "NoiseUtility.h"

//------------------------------------------------------------------------------
// 4-connected neighbor offsets for BFS: E, N, W, S
//------------------------------------------------------------------------------
static constexpr int32 BDX4[] = {  1,  0, -1,  0 };
static constexpr int32 BDY4[] = {  0, -1,  0,  1 };

//------------------------------------------------------------------------------
// 8-connected neighbor offsets: imported from shared header
//------------------------------------------------------------------------------
using WorldNoise::DX8;
using WorldNoise::DY8;

//------------------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::Initialize(const FBiomeAssignmentSettings& InSettings, int32 TextureResolution)
{
	Settings = InSettings;
	Resolution = TextureResolution;

	const int32 TotalPixels = Resolution * Resolution;
	BiomeMap.SetNumZeroed(TotalPixels);
	TerrainArchetypeMap.SetNumZeroed(TotalPixels);
	SurfaceOverlayMap.SetNumZeroed(TotalPixels);
	GeneratedFeatureMap.SetNumZeroed(TotalPixels);
	SlopeMap.SetNumZeroed(TotalPixels);
	WaterDistMap.SetNumZeroed(TotalPixels);
	BiomeBlendWeights.SetNumZeroed(TotalPixels * NumBiomeTypes);

	// Ensure material rules array has 8 entries (one per biome type)
	if (Settings.BiomeMaterialRules.Num() < NumBiomeTypes)
	{
		Settings.BiomeMaterialRules.SetNum(NumBiomeTypes);
	}

	UE_LOG(LogTemp, Log,
		TEXT("BiomeAssignmentGenerator initialized – Resolution: %d, MountainThreshold: %.2f, VolcanicRadius: %d px, BlendRadius: %d px"),
		Resolution, Settings.MountainElevationThreshold, Settings.VolcanicRadiusPixels, Settings.BiomeBlendRadius);
}

//------------------------------------------------------------------------------
// ComputeSlopeMap: 8-connected maximum absolute elevation difference
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::ComputeSlopeMap(const TArray<float>& Elevation)
{
	const int32 TotalPixels = Resolution * Resolution;
	SlopeMap.SetNumZeroed(TotalPixels);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		float MaxDiff = 0.0f;

		for (int32 Dir = 0; Dir < 8; ++Dir)
		{
			const int32 NX = X + DX8[Dir];
			const int32 NY = Y + DY8[Dir];
			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution) continue;
			const float Diff = FMath::Abs(Elevation[i] - Elevation[NY * Resolution + NX]);
			if (Diff > MaxDiff) MaxDiff = Diff;
		}

		SlopeMap[i] = FMath::Clamp(MaxDiff, 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator – Slope map computed"));
}

//------------------------------------------------------------------------------
// ComputeWaterDistMap: BFS distance from river/lake pixels
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::ComputeWaterDistMap(const TArray<float>& RiverMap, const TArray<uint8>& LakeMap)
{
	const int32 TotalPixels = Resolution * Resolution;
	const float Unvisited = static_cast<float>(Resolution * 2);
	WaterDistMap.SetNumUninitialized(TotalPixels);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		WaterDistMap[i] = Unvisited;
	}

	TQueue<int32> BFSQueue;

	// Seed with water pixels
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (RiverMap[i] > 0.0f || LakeMap[i] == 1)
		{
			WaterDistMap[i] = 0.0f;
			BFSQueue.Enqueue(i);
		}
	}

	while (!BFSQueue.IsEmpty())
	{
		int32 Cur;
		BFSQueue.Dequeue(Cur);

		const int32 CX = Cur % Resolution;
		const int32 CY = Cur / Resolution;
		const float CurDist = WaterDistMap[Cur];

		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const int32 NX = CX + BDX4[Dir];
			const int32 NY = CY + BDY4[Dir];
			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution) continue;

			const int32 NIdx = NY * Resolution + NX;
			const float NewDist = CurDist + 1.0f;
			if (NewDist < WaterDistMap[NIdx])
			{
				WaterDistMap[NIdx] = NewDist;
				BFSQueue.Enqueue(NIdx);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator – Water distance map computed"));
}

//------------------------------------------------------------------------------
// ComputeBiomeBlendWeights: Gaussian-distance blend from neighboring biomes
//   For each pixel, scan the BiomeBlendRadius neighborhood. For every distinct
//   biome found, accumulate weight = exp(-dist^2 / (2*sigma^2)).  Normalize so
//   all weights sum to 1.
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::ComputeBiomeBlendWeights()
{
	const int32 TotalPixels = Resolution * Resolution;
	const int32 R = Settings.BiomeBlendRadius;

	if (R <= 0)
	{
		// No blending: each pixel gets weight 1.0 for its own biome
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const int32 Base = i * NumBiomeTypes;
			for (int32 b = 0; b < NumBiomeTypes; ++b)
			{
				BiomeBlendWeights[Base + b] = 0.0f;
			}
			BiomeBlendWeights[Base + BiomeMap[i]] = 1.0f;
		}
		return;
	}

	const float Sigma = static_cast<float>(R) * 0.5f;
	const float InvTwoSigmaSq = 1.0f / (2.0f * Sigma * Sigma);

	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 Idx = Y * Resolution + X;
			const int32 Base = Idx * NumBiomeTypes;

			// Zero weights
			for (int32 b = 0; b < NumBiomeTypes; ++b)
			{
				BiomeBlendWeights[Base + b] = 0.0f;
			}

			// Scan neighborhood
			for (int32 DY = -R; DY <= R; ++DY)
			{
				const int32 NY = Y + DY;
				if (NY < 0 || NY >= Resolution) continue;

				for (int32 DX = -R; DX <= R; ++DX)
				{
					const int32 NX = X + DX;
					if (NX < 0 || NX >= Resolution) continue;

					const float DistSq = static_cast<float>(DX * DX + DY * DY);
					if (DistSq > static_cast<float>(R * R)) continue;

					const float Weight = FMath::Exp(-DistSq * InvTwoSigmaSq);
					const int32 NBiome = BiomeMap[NY * Resolution + NX];
					BiomeBlendWeights[Base + NBiome] += Weight;
				}
			}

			// Normalize
			float Sum = 0.0f;
			for (int32 b = 0; b < NumBiomeTypes; ++b)
			{
				Sum += BiomeBlendWeights[Base + b];
			}
			if (Sum > 0.0f)
			{
				const float InvSum = 1.0f / Sum;
				for (int32 b = 0; b < NumBiomeTypes; ++b)
				{
					BiomeBlendWeights[Base + b] *= InvSum;
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator – Biome blend weights computed (radius: %d)"), R);
}

//------------------------------------------------------------------------------
// Generate
//------------------------------------------------------------------------------
bool UBiomeAssignmentGenerator::Generate(const TArray<float>& Elevation, const TArray<float>& Temperature,
	const TArray<float>& Moisture, const TArray<float>& Precipitation,
	const TArray<uint8>& LandMask, const TArray<float>& RiverMap,
	const TArray<uint8>& LakeMap, const TArray<FVector2D>& VolcanicCenters,
	const TArray<float>& PlateauMap, const TArray<uint8>& CanyonMask,
	const TArray<uint8>& WaterfallMap, float SeaLevel)
{
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("BiomeAssignmentGenerator::Generate – Resolution not set. Call Initialize first."));
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;

	if (Elevation.Num() != TotalPixels || Temperature.Num() != TotalPixels
		|| Moisture.Num() != TotalPixels || Precipitation.Num() != TotalPixels
		|| LandMask.Num() != TotalPixels || RiverMap.Num() != TotalPixels
		|| LakeMap.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error,
			TEXT("BiomeAssignmentGenerator::Generate – Input array size mismatch (expected %d)"), TotalPixels);
		return false;
	}

	// PlateauMap, CanyonMask, WaterfallMap may be empty if earlier stages were skipped
	const bool bHasPlateauMap = (PlateauMap.Num() == TotalPixels);
	const bool bHasCanyonMask = (CanyonMask.Num() == TotalPixels);
	const bool bHasWaterfallMap = (WaterfallMap.Num() == TotalPixels);

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator::Generate – starting (%d pixels, %d volcanic centers, SeaLevel=%.3f)"),
		TotalPixels, VolcanicCenters.Num(), SeaLevel);

	// --- Pre-compute derived inputs ---
	ComputeSlopeMap(Elevation);
	ComputeWaterDistMap(RiverMap, LakeMap);

	// Pre-compute volcanic center pixel coordinates
	TArray<FVector2D> VolcanicPixelPositions;
	VolcanicPixelPositions.Reserve(VolcanicCenters.Num());
	const float Res = static_cast<float>(Resolution);
	for (const FVector2D& Center : VolcanicCenters)
	{
		VolcanicPixelPositions.Add(FVector2D(Center.X * Res, Center.Y * Res));
	}

	const float VolcanicRadiusSq = static_cast<float>(Settings.VolcanicRadiusPixels)
		* static_cast<float>(Settings.VolcanicRadiusPixels);

	const float WaterProxRadius = static_cast<float>(Settings.WaterProximityRadiusPixels);
	const bool bApplyWaterProximity = (WaterProxRadius > 0.0f);

	const float WetlandsMaxDist = static_cast<float>(Settings.WetlandsMaxWaterDistance);

	// Pre-compute effective moisture (with water proximity bonus) for reuse
	TArray<float> EffMoisture;
	EffMoisture.SetNum(TotalPixels);
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		EffMoisture[i] = Moisture[i];
		if (LandMask[i] != 0 && bApplyWaterProximity && WaterDistMap[i] < WaterProxRadius)
		{
			const float ProximityFactor = 1.0f - (WaterDistMap[i] / WaterProxRadius);
			EffMoisture[i] += Settings.WaterProximityMoistureBonus * ProximityFactor;
			EffMoisture[i] = FMath::Min(EffMoisture[i], 1.0f);
		}
	}

	// Biome distribution counters
	TMap<EBiomeType, int32> BiomeCounts;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		EBiomeType Biome = EBiomeType::Land;
		ETerrainArchetype Archetype = ETerrainArchetype::Plains;
		ESurfaceOverlay Overlay = ESurfaceOverlay::Grassland;
		EGeneratedFeature Feature = EGeneratedFeature::None;

		// --- Generated feature layer ---
		if (bHasWaterfallMap && WaterfallMap[i] != 0)
		{
			Feature = EGeneratedFeature::Waterfall;
		}
		else if (RiverMap[i] > 0.0f)
		{
			Feature = EGeneratedFeature::River;
		}
		else if (LakeMap[i] != 0)
		{
			Feature = EGeneratedFeature::Lake;
		}

		// 1. Ocean
		if (LandMask[i] == 0)
		{
			Biome = EBiomeType::Ocean;
			Archetype = ETerrainArchetype::Ocean;
			Overlay = ESurfaceOverlay::None;
		}
		else
		{
			const float EffectiveMoisture = EffMoisture[i];

			// --- Terrain archetype classification (shape-based) ---
			// Priority: Canyons > Plateaus > Mountains > Desert > Hills > Plains
			if (bHasCanyonMask && CanyonMask[i] != 0)
			{
				Archetype = ETerrainArchetype::Canyons;
			}
			else if (bHasPlateauMap && PlateauMap[i] >= Settings.PlateauArchetypeThreshold)
			{
				Archetype = ETerrainArchetype::Plateaus;
			}
			else if (Elevation[i] > Settings.MountainElevationThreshold
				|| SlopeMap[i] > Settings.SteepSlopeThreshold)
			{
				Archetype = ETerrainArchetype::Mountains;
			}
			else if (Temperature[i] > Settings.DesertTemperatureThreshold
				&& EffectiveMoisture < Settings.DesertMoistureThreshold)
			{
				Archetype = ETerrainArchetype::Desert;
			}
			else if (SlopeMap[i] > Settings.HillSlopeThreshold)
			{
				Archetype = ETerrainArchetype::Hills;
			}
			else
			{
				Archetype = ETerrainArchetype::Plains;
			}

			// --- Surface overlay classification (climate-based) ---
			// Priority: Snow > Wetlands > Forest > DesertScrub > Grassland (default)
			if (Temperature[i] < Settings.SnowTemperatureThreshold)
			{
				Overlay = ESurfaceOverlay::Snow;
			}
			else if (EffectiveMoisture >= Settings.WetlandsMoistureThreshold
				&& WaterDistMap[i] < WetlandsMaxDist)
			{
				Overlay = ESurfaceOverlay::Wetlands;
			}
			else if (EffectiveMoisture > Settings.ForestMoistureThreshold
				&& Temperature[i] > Settings.ForestTemperatureThreshold
				&& Precipitation[i] > Settings.ForestPrecipitationThreshold)
			{
				Overlay = ESurfaceOverlay::Forest;
			}
			else if (Temperature[i] > Settings.DesertScrubTemperatureThreshold
				&& EffectiveMoisture < Settings.DesertScrubMoistureMax
				&& EffectiveMoisture >= Settings.DesertMoistureThreshold)
			{
				Overlay = ESurfaceOverlay::DesertScrub;
			}
			else
			{
				Overlay = ESurfaceOverlay::Grassland;
			}

			// --- Legacy biome classification (unchanged for backward compat) ---
			// 2. Volcanic — near a volcanic center AND elevation > 0.3
			bool bIsVolcanic = false;
			if (VolcanicPixelPositions.Num() > 0 && Elevation[i] > 0.3f)
			{
				const float PixelX = static_cast<float>(i % Resolution);
				const float PixelY = static_cast<float>(i / Resolution);

				for (const FVector2D& VPos : VolcanicPixelPositions)
				{
					const float DX = PixelX - VPos.X;
					const float DY = PixelY - VPos.Y;
					if (DX * DX + DY * DY < VolcanicRadiusSq)
					{
						bIsVolcanic = true;
						break;
					}
				}
			}

			if (bIsVolcanic)
			{
				Biome = EBiomeType::Volcanic;
			}
			// 3. Mountain — high elevation OR very steep slope
			else if (Elevation[i] > Settings.MountainElevationThreshold
				|| SlopeMap[i] > Settings.SteepSlopeThreshold)
			{
				Biome = EBiomeType::Mountain;
			}
			// 4. Ice
			else if (Temperature[i] < Settings.IceTemperatureThreshold && EffectiveMoisture > Settings.IceMoistureThreshold)
			{
				Biome = EBiomeType::Ice;
			}
			// 5. Snow
			else if (Temperature[i] < Settings.SnowTemperatureThreshold)
			{
				Biome = EBiomeType::Snow;
			}
			// 6. Desert
			else if (Temperature[i] > Settings.DesertTemperatureThreshold && EffectiveMoisture < Settings.DesertMoistureThreshold)
			{
				Biome = EBiomeType::Desert;
			}
			// 7. Forest — uses precipitation in addition to moisture/temperature
			else if (EffectiveMoisture > Settings.ForestMoistureThreshold
				&& Temperature[i] > Settings.ForestTemperatureThreshold
				&& Precipitation[i] > Settings.ForestPrecipitationThreshold)
			{
				Biome = EBiomeType::Forest;
			}
			// 8. Default — Land (grassland) already set
		}

		BiomeMap[i] = static_cast<int32>(Biome);
		TerrainArchetypeMap[i] = static_cast<int32>(Archetype);
		SurfaceOverlayMap[i] = static_cast<int32>(Overlay);
		GeneratedFeatureMap[i] = static_cast<int32>(Feature);
	}

	// --- Optional: target percentage biome redistribution ---
	if (Settings.bEnableBiomeTargets)
	{
		ApplyBiomeTargets(Elevation, Temperature, EffMoisture, Precipitation, LandMask);
	}

	// --- Optional: remove small biome clusters ---
	RemoveSmallClusters();

	// --- Majority-vote spatial smoothing ---
	SmoothBiomeMap();

	// --- Biome blending weights ---
	ComputeBiomeBlendWeights();

	// Log biome distribution (after all adjustments)
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		BiomeCounts.FindOrAdd(static_cast<EBiomeType>(BiomeMap[i]))++;
	}
	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator::Generate – Biome distribution:"));
	UE_LOG(LogTemp, Log, TEXT("  Ocean:    %d"), BiomeCounts.FindRef(EBiomeType::Ocean));
	UE_LOG(LogTemp, Log, TEXT("  Volcanic: %d"), BiomeCounts.FindRef(EBiomeType::Volcanic));
	UE_LOG(LogTemp, Log, TEXT("  Mountain: %d"), BiomeCounts.FindRef(EBiomeType::Mountain));
	UE_LOG(LogTemp, Log, TEXT("  Ice:      %d"), BiomeCounts.FindRef(EBiomeType::Ice));
	UE_LOG(LogTemp, Log, TEXT("  Snow:     %d"), BiomeCounts.FindRef(EBiomeType::Snow));
	UE_LOG(LogTemp, Log, TEXT("  Desert:   %d"), BiomeCounts.FindRef(EBiomeType::Desert));
	UE_LOG(LogTemp, Log, TEXT("  Forest:   %d"), BiomeCounts.FindRef(EBiomeType::Forest));
	UE_LOG(LogTemp, Log, TEXT("  Land:     %d"), BiomeCounts.FindRef(EBiomeType::Land));

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator::Generate – complete"));
	return true;
}

//------------------------------------------------------------------------------
// ApplyBiomeTargets:
//   Score-ranked biome assignment to achieve target percentage coverage.
//   For each biome (in priority order), compute an affinity score per pixel,
//   then select the top-N land pixels to match the configured target.
//   Ocean and Volcanic assignments are preserved; remaining land → Grassland.
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::ApplyBiomeTargets(
	const TArray<float>& Elevation,
	const TArray<float>& Temperature,
	const TArray<float>& EffectiveMoisture,
	const TArray<float>& Precipitation,
	const TArray<uint8>& LandMask)
{
	const int32 Total = Resolution * Resolution;

	// Count land pixels and mark fixed biomes (Ocean, Volcanic)
	int32 LandCount = 0;
	TArray<bool> Fixed;
	Fixed.SetNumZeroed(Total);

	for (int32 i = 0; i < Total; ++i)
	{
		if (LandMask[i] == 0 || BiomeMap[i] == static_cast<int32>(EBiomeType::Volcanic))
		{
			Fixed[i] = true;
		}
		else
		{
			LandCount++;
		}
	}
	if (LandCount == 0) return;

	// Reset all non-fixed to Land (Grassland) — the default remainder
	const int32 LandVal = static_cast<int32>(EBiomeType::Land);
	for (int32 i = 0; i < Total; ++i)
	{
		if (!Fixed[i]) BiomeMap[i] = LandVal;
	}

	// Scored candidate for sorting
	struct FScored
	{
		int32 Idx;
		float Score;
	};

	// --- Mountain pass: highest elevation + steepest slope ---
	{
		int32 Target = FMath::RoundToInt(LandCount * Settings.TargetMountainPercent / 100.0f);
		if (Target > 0)
		{
			TArray<FScored> Cands;
			Cands.Reserve(LandCount);
			for (int32 i = 0; i < Total; ++i)
			{
				if (Fixed[i] || BiomeMap[i] != LandVal) continue;
				float S = FMath::Max(0.0f, Elevation[i] - 0.3f) * 2.0f
				        + FMath::Max(0.0f, SlopeMap[i] - 0.03f);
				if (S > 0.0f) Cands.Add({i, S});
			}
			Cands.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });
			int32 Count = FMath::Min(Target, Cands.Num());
			for (int32 j = 0; j < Count; ++j)
				BiomeMap[Cands[j].Idx] = static_cast<int32>(EBiomeType::Mountain);
		}
	}

	// --- Snow/Ice pass: coldest pixels ---
	{
		int32 Target = FMath::RoundToInt(LandCount * Settings.TargetSnowPercent / 100.0f);
		if (Target > 0)
		{
			TArray<FScored> Cands;
			Cands.Reserve(LandCount);
			for (int32 i = 0; i < Total; ++i)
			{
				if (Fixed[i] || BiomeMap[i] != LandVal) continue;
				float S = FMath::Max(0.0f, 0.5f - Temperature[i]);
				if (S > 0.0f) Cands.Add({i, S});
			}
			Cands.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });
			int32 Count = FMath::Min(Target, Cands.Num());
			for (int32 j = 0; j < Count; ++j)
			{
				const int32 Idx = Cands[j].Idx;
				if (Temperature[Idx] < Settings.IceTemperatureThreshold
					&& EffectiveMoisture[Idx] > Settings.IceMoistureThreshold)
				{
					BiomeMap[Idx] = static_cast<int32>(EBiomeType::Ice);
				}
				else
				{
					BiomeMap[Idx] = static_cast<int32>(EBiomeType::Snow);
				}
			}
		}
	}

	// --- Desert pass: hottest + driest ---
	{
		int32 Target = FMath::RoundToInt(LandCount * Settings.TargetDesertPercent / 100.0f);
		if (Target > 0)
		{
			TArray<FScored> Cands;
			Cands.Reserve(LandCount);
			for (int32 i = 0; i < Total; ++i)
			{
				if (Fixed[i] || BiomeMap[i] != LandVal) continue;
				float S = FMath::Max(0.0f, Temperature[i] - 0.3f)
				        * FMath::Max(0.0f, 0.6f - EffectiveMoisture[i]);
				if (S > 0.0f) Cands.Add({i, S});
			}
			Cands.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });
			int32 Count = FMath::Min(Target, Cands.Num());
			for (int32 j = 0; j < Count; ++j)
				BiomeMap[Cands[j].Idx] = static_cast<int32>(EBiomeType::Desert);
		}
	}

	// --- Forest pass: wettest + warmest + highest precipitation ---
	{
		int32 Target = FMath::RoundToInt(LandCount * Settings.TargetForestPercent / 100.0f);
		if (Target > 0)
		{
			TArray<FScored> Cands;
			Cands.Reserve(LandCount);
			for (int32 i = 0; i < Total; ++i)
			{
				if (Fixed[i] || BiomeMap[i] != LandVal) continue;
				float S = EffectiveMoisture[i]
				        * FMath::Max(0.0f, Temperature[i] - 0.1f)
				        * Precipitation[i];
				if (S > 0.0f) Cands.Add({i, S});
			}
			Cands.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });
			int32 Count = FMath::Min(Target, Cands.Num());
			for (int32 j = 0; j < Count; ++j)
				BiomeMap[Cands[j].Idx] = static_cast<int32>(EBiomeType::Forest);
		}
	}

	// Remaining land pixels stay as Land (Grassland)

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator – Applied target percentages (Mtn:%.0f%%, Snow:%.0f%%, Desert:%.0f%%, Forest:%.0f%% of %d land)"),
		Settings.TargetMountainPercent, Settings.TargetSnowPercent,
		Settings.TargetDesertPercent, Settings.TargetForestPercent, LandCount);
}

//------------------------------------------------------------------------------
// RemoveSmallClusters:
//   Flood-fill connected components (4-connected) in the BiomeMap.
//   Any component with fewer pixels than MinBiomeClusterSize is absorbed into
//   the most common neighbouring biome, producing cleaner biome boundaries.
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::RemoveSmallClusters()
{
	const int32 Total = Resolution * Resolution;
	const int32 MinSize = Settings.MinBiomeClusterSize;
	if (MinSize <= 1) return;

	// --- Pass 1: label connected components ---
	TArray<int32> Label;
	Label.SetNumUninitialized(Total);
	for (int32 i = 0; i < Total; ++i) Label[i] = -1;

	struct FComp { int32 Size; int32 Biome; };
	TArray<FComp> Comps;

	TArray<int32> FloodStack;
	FloodStack.Reserve(512);

	for (int32 Seed = 0; Seed < Total; ++Seed)
	{
		if (Label[Seed] >= 0) continue;

		const int32 CId = Comps.Num();
		const int32 SeedBiome = BiomeMap[Seed];
		Comps.Add({0, SeedBiome});

		FloodStack.Reset();
		FloodStack.Add(Seed);
		Label[Seed] = CId;

		while (FloodStack.Num() > 0)
		{
			const int32 Cur = FloodStack.Pop(false);
			Comps[CId].Size++;

			const int32 X = Cur % Resolution;
			const int32 Y = Cur / Resolution;
			for (int32 D = 0; D < 4; ++D)
			{
				const int32 NX = X + BDX4[D];
				const int32 NY = Y + BDY4[D];
				if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution) continue;
				const int32 NI = NY * Resolution + NX;
				if (Label[NI] >= 0 || BiomeMap[NI] != SeedBiome) continue;
				Label[NI] = CId;
				FloodStack.Add(NI);
			}
		}
	}

	// --- Pass 2: for each small component, tally neighbour biome votes ---
	TArray<TMap<int32, int32>> Votes;
	Votes.SetNum(Comps.Num());

	for (int32 i = 0; i < Total; ++i)
	{
		const int32 CId = Label[i];
		if (Comps[CId].Size >= MinSize) continue;

		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		for (int32 D = 0; D < 4; ++D)
		{
			const int32 NX = X + BDX4[D];
			const int32 NY = Y + BDY4[D];
			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution) continue;
			const int32 NI = NY * Resolution + NX;
			if (Label[NI] == CId) continue;
			Votes[CId].FindOrAdd(BiomeMap[NI])++;
		}
	}

	// Determine replacement biome for each small component
	TArray<int32> Replacement;
	Replacement.SetNumUninitialized(Comps.Num());
	for (int32 c = 0; c < Comps.Num(); ++c) Replacement[c] = -1;

	for (int32 c = 0; c < Comps.Num(); ++c)
	{
		if (Comps[c].Size >= MinSize) continue;
		int32 Best = Comps[c].Biome;
		int32 BestN = 0;
		for (const auto& V : Votes[c])
		{
			if (V.Value > BestN) { BestN = V.Value; Best = V.Key; }
		}
		if (Best != Comps[c].Biome)
		{
			Replacement[c] = Best;
		}
	}

	// --- Pass 3: apply replacements ---
	int32 Absorbed = 0;
	for (int32 i = 0; i < Total; ++i)
	{
		const int32 CId = Label[i];
		if (Replacement[CId] >= 0)
		{
			BiomeMap[i] = Replacement[CId];
			Absorbed++;
		}
	}

	if (Absorbed > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("BiomeAssignmentGenerator – removed small clusters: %d pixels absorbed (min size %d)"),
			Absorbed, MinSize);
	}
}

//------------------------------------------------------------------------------
// SmoothBiomeMap:
//   Majority-vote filter in a 5×5 neighbourhood window.
//   For each pixel, count how often each biome type appears in the window;
//   replace the pixel with the most common biome.  This eliminates thin
//   stripe artefacts that form along elevation/temperature contour lines.
//   Repeated for BiomeSmoothingPasses iterations.
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::SmoothBiomeMap()
{
	const int32 Passes = Settings.BiomeSmoothingPasses;
	if (Passes <= 0) return;

	const int32 Total = Resolution * Resolution;
	const int32 R = 2;  // half-window radius → 5×5

	TArray<int32> Temp;
	Temp.SetNumUninitialized(Total);

	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		int32 Changed = 0;

		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				const int32 Idx = Y * Resolution + X;

				// Tally votes from NxN neighbourhood
				int32 Votes[NumBiomeTypes] = {};

				const int32 YMin = FMath::Max(0, Y - R);
				const int32 YMax = FMath::Min(Resolution - 1, Y + R);
				const int32 XMin = FMath::Max(0, X - R);
				const int32 XMax = FMath::Min(Resolution - 1, X + R);

				for (int32 NY = YMin; NY <= YMax; ++NY)
				{
					for (int32 NX = XMin; NX <= XMax; ++NX)
					{
						const int32 B = BiomeMap[NY * Resolution + NX];
						if (B >= 0 && B < NumBiomeTypes)
						{
							Votes[B]++;
						}
					}
				}

				// Find the biome with the most votes
				int32 BestBiome = BiomeMap[Idx];
				int32 BestCount = 0;
				for (int32 B = 0; B < NumBiomeTypes; ++B)
				{
					if (Votes[B] > BestCount)
					{
						BestCount = Votes[B];
						BestBiome = B;
					}
				}

				Temp[Idx] = BestBiome;
				if (BestBiome != BiomeMap[Idx]) Changed++;
			}
		}

		// Copy smoothed result back
		FMemory::Memcpy(BiomeMap.GetData(), Temp.GetData(), Total * sizeof(int32));

		UE_LOG(LogTemp, Log,
			TEXT("BiomeAssignmentGenerator – smoothing pass %d/%d: %d pixels changed"),
			Pass + 1, Passes, Changed);

		if (Changed == 0) break;  // converged
	}
}
