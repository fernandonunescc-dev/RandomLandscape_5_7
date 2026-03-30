#include "BiomeAssignmentGenerator.h"

//------------------------------------------------------------------------------
// 4-connected neighbor offsets for BFS: E, N, W, S
//------------------------------------------------------------------------------
static constexpr int32 BDX4[] = {  1,  0, -1,  0 };
static constexpr int32 BDY4[] = {  0, -1,  0,  1 };

//------------------------------------------------------------------------------
// 8-connected neighbor offsets
//------------------------------------------------------------------------------
static constexpr int32 BDX8[] = {  1,  1,  0, -1, -1, -1,  0,  1 };
static constexpr int32 BDY8[] = {  0, -1, -1, -1,  0,  1,  1,  1 };

//------------------------------------------------------------------------------
// Initialize
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::Initialize(const FBiomeAssignmentSettings& InSettings, int32 TextureResolution)
{
	Settings = InSettings;
	Resolution = TextureResolution;

	const int32 TotalPixels = Resolution * Resolution;
	BiomeMap.SetNumZeroed(TotalPixels);
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
			const int32 NX = X + BDX8[Dir];
			const int32 NY = Y + BDY8[Dir];
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
	const TArray<uint8>& LakeMap, const TArray<FVector2D>& VolcanicCenters)
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

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator::Generate – starting (%d pixels, %d volcanic centers)"),
		TotalPixels, VolcanicCenters.Num());

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

	// Biome distribution counters
	TMap<EBiomeType, int32> BiomeCounts;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		EBiomeType Biome = EBiomeType::Land;

		// 1. Ocean
		if (LandMask[i] == 0)
		{
			Biome = EBiomeType::Ocean;
		}
		else
		{
			// Effective moisture: add bonus for proximity to water
			float EffectiveMoisture = Moisture[i];
			if (bApplyWaterProximity && WaterDistMap[i] < WaterProxRadius)
			{
				const float ProximityFactor = 1.0f - (WaterDistMap[i] / WaterProxRadius);
				EffectiveMoisture += Settings.WaterProximityMoistureBonus * ProximityFactor;
				EffectiveMoisture = FMath::Min(EffectiveMoisture, 1.0f);
			}

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
		BiomeCounts.FindOrAdd(Biome)++;
	}

	// --- Biome blending weights ---
	ComputeBiomeBlendWeights();

	// Log biome distribution
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
