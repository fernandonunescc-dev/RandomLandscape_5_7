#include "TerrainValidator.h"
#include "BiomeTypes.h"
#include "NoiseUtility.h"

using WorldNoise::DX8;
using WorldNoise::DY8;

// ----------------------------------------------------------------
void UTerrainValidator::Initialize(int32 TextureResolution)
{
	Resolution = TextureResolution;
}

// ----------------------------------------------------------------
FTerrainValidationResult UTerrainValidator::Validate(
	const TArray<uint8>& LandMask,
	const TArray<float>& CombinedElevation,
	const TArray<float>& UpliftMap,
	const TArray<int32>& FlowDirection,
	const TArray<float>& RiverMap,
	const TArray<uint8>& LakeMap,
	const TArray<uint8>& WaterfallMap,
	const TArray<uint8>& CanyonMask,
	const TArray<float>& ErodedElevation,
	const TArray<float>& Temperature,
	const TArray<float>& Moisture,
	const TArray<int32>& BiomeMap,
	const TArray<float>& SlopeMap,
	const TArray<FVector2D>& VolcanicCenters)
{
	FTerrainValidationResult Result;

	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("TerrainValidator: Resolution not set. Call Initialize first."));
		return Result;
	}

	CheckUphillRivers(CombinedElevation, FlowDirection, RiverMap, LandMask, Result.Errors);
	CheckRidgeLakes(LakeMap, CombinedElevation, SlopeMap, VolcanicCenters, Result.Errors);
	CheckAbruptBiomeSeams(BiomeMap, Temperature, Moisture, LandMask, Result.Errors);
	CheckUnsupportedMountains(BiomeMap, UpliftMap, LandMask, Result.Errors);
	CheckCanyonsWithoutRivers(CanyonMask, RiverMap, Result.Errors);
	CheckWaterfallElevationDrop(WaterfallMap, ErodedElevation, FlowDirection, Result.Errors);
	CheckIsolatedWetBiomes(BiomeMap, Moisture, RiverMap, LakeMap, LandMask, Result.Errors);
	CheckVolcanoPlacement(VolcanicCenters, LandMask, Result.Errors);

	Result.TotalChecksRun = 8;

	for (const FTerrainValidationError& Err : Result.Errors)
	{
		switch (Err.Severity)
		{
		case EValidationSeverity::Error:   Result.ErrorCount++;   break;
		case EValidationSeverity::Warning: Result.WarningCount++; break;
		case EValidationSeverity::Info:    Result.InfoCount++;    break;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("TerrainValidator: %d checks run — %d errors, %d warnings, %d info"),
		Result.TotalChecksRun, Result.ErrorCount, Result.WarningCount, Result.InfoCount);

	return Result;
}

// ================================================================
// Check 1: Rivers flowing uphill
// ================================================================
void UTerrainValidator::CheckUphillRivers(
	const TArray<float>& Elevation, const TArray<int32>& FlowDirection,
	const TArray<float>& RiverMap, const TArray<uint8>& LandMask,
	TArray<FTerrainValidationError>& OutErrors)
{
	const int32 Total = Resolution * Resolution;
	if (Elevation.Num() != Total || FlowDirection.Num() != Total) return;

	int32 UphillCount = 0;
	int32 RiverPixels = 0;

	for (int32 i = 0; i < Total; ++i)
	{
		if (RiverMap[i] <= 0.0f || FlowDirection[i] < 0) continue;
		RiverPixels++;

		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		const int32 Dir = FlowDirection[i];
		const int32 NX = X + DX8[Dir];
		const int32 NY = Y + DY8[Dir];

		if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution) continue;

		const int32 NIdx = NY * Resolution + NX;
		if (Elevation[NIdx] > Elevation[i] + 0.001f)
		{
			UphillCount++;
		}
	}

	if (UphillCount > 0 && RiverPixels > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("UphillRiver");
		Err.AffectedPixelCount = UphillCount;
		Err.AffectedPercentage = static_cast<float>(UphillCount) / FMath::Max(RiverPixels, 1);

		Err.Severity = (Err.AffectedPercentage > 0.05f) ? EValidationSeverity::Error : EValidationSeverity::Warning;
		Err.Description = FString::Printf(
			TEXT("%d river pixels (%0.1f%%) flow toward a higher-elevation neighbor"),
			UphillCount, Err.AffectedPercentage * 100.0f);
		Err.SuggestedFix = TEXT("Reduce FlowJitter in HydrologySettings or increase thermal erosion to smooth terrain");
		Err.DebugMapToInspect = TEXT("Debug_Rivers, Debug_CombinedElevation");

		OutErrors.Add(MoveTemp(Err));
	}
}

// ================================================================
// Check 2: Lakes on ridges (high elevation + high slope, not in crater)
// ================================================================
void UTerrainValidator::CheckRidgeLakes(
	const TArray<uint8>& LakeMap, const TArray<float>& Elevation,
	const TArray<float>& SlopeMap, const TArray<FVector2D>& VolcanicCenters,
	TArray<FTerrainValidationError>& OutErrors)
{
	const int32 Total = Resolution * Resolution;
	if (LakeMap.Num() != Total || Elevation.Num() != Total) return;

	const float HighElevThreshold = 0.6f;
	const float HighSlopeThreshold = 0.15f;
	const float CraterProximity = 0.1f; // normalized distance

	int32 RidgeLakeCount = 0;
	int32 TotalLakePixels = 0;

	for (int32 i = 0; i < Total; ++i)
	{
		if (LakeMap[i] == 0) continue;
		TotalLakePixels++;

		if (Elevation[i] < HighElevThreshold) continue;

		// Check if slope data is available
		float Slope = 0.0f;
		if (SlopeMap.Num() == Total) Slope = SlopeMap[i];

		if (Slope < HighSlopeThreshold) continue;

		// Exempt pixels near volcanic craters
		const float NormX = static_cast<float>(i % Resolution) / FMath::Max(Resolution - 1, 1);
		const float NormY = static_cast<float>(i / Resolution) / FMath::Max(Resolution - 1, 1);
		bool bNearCrater = false;
		for (const FVector2D& VC : VolcanicCenters)
		{
			if (FVector2D::Distance(FVector2D(NormX, NormY), VC) < CraterProximity)
			{
				bNearCrater = true;
				break;
			}
		}
		if (bNearCrater) continue;

		RidgeLakeCount++;
	}

	if (RidgeLakeCount > 0 && TotalLakePixels > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("RidgeLake");
		Err.AffectedPixelCount = RidgeLakeCount;
		Err.AffectedPercentage = static_cast<float>(RidgeLakeCount) / FMath::Max(TotalLakePixels, 1);
		Err.Severity = (RidgeLakeCount > 50) ? EValidationSeverity::Error : EValidationSeverity::Warning;
		Err.Description = FString::Printf(
			TEXT("%d lake pixels sit on high-elevation ridges (elev>%.1f, slope>%.2f) without crater exemption"),
			RidgeLakeCount, HighElevThreshold, HighSlopeThreshold);
		Err.SuggestedFix = TEXT("Increase LakeThreshold in HydrologySettings to reduce spurious lake formation");
		Err.DebugMapToInspect = TEXT("Debug_Lakes, Debug_CombinedElevation, Debug_SlopeMap");
		OutErrors.Add(MoveTemp(Err));
	}
}

// ================================================================
// Check 3: Abrupt biome seams
// ================================================================
void UTerrainValidator::CheckAbruptBiomeSeams(
	const TArray<int32>& BiomeMap, const TArray<float>& Temperature,
	const TArray<float>& Moisture, const TArray<uint8>& LandMask,
	TArray<FTerrainValidationError>& OutErrors)
{
	const int32 Total = Resolution * Resolution;
	if (BiomeMap.Num() != Total || Temperature.Num() != Total || Moisture.Num() != Total) return;

	const float AbruptTempDelta = 0.35f;
	const float AbruptMoistureDelta = 0.4f;

	int32 AbruptCount = 0;
	int32 BoundaryPixels = 0;

	for (int32 i = 0; i < Total; ++i)
	{
		if (LandMask[i] == 0) continue;

		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;

		// Check right and down neighbors only (avoid double counting)
		for (int32 d = 0; d < 2; ++d)
		{
			const int32 NX = X + (d == 0 ? 1 : 0);
			const int32 NY = Y + (d == 1 ? 1 : 0);

			if (NX >= Resolution || NY >= Resolution) continue;

			const int32 NIdx = NY * Resolution + NX;
			if (LandMask[NIdx] == 0) continue;
			if (BiomeMap[i] == BiomeMap[NIdx]) continue;

			BoundaryPixels++;

			const float TempDelta = FMath::Abs(Temperature[i] - Temperature[NIdx]);
			const float MoistDelta = FMath::Abs(Moisture[i] - Moisture[NIdx]);

			if (TempDelta > AbruptTempDelta || MoistDelta > AbruptMoistureDelta)
			{
				AbruptCount++;
			}
		}
	}

	if (AbruptCount > 0 && BoundaryPixels > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("AbruptBiomeSeam");
		Err.AffectedPixelCount = AbruptCount;
		Err.AffectedPercentage = static_cast<float>(AbruptCount) / FMath::Max(BoundaryPixels, 1);
		Err.Severity = (Err.AffectedPercentage > 0.1f) ? EValidationSeverity::Warning : EValidationSeverity::Info;
		Err.Description = FString::Printf(
			TEXT("%d biome boundary pixels (%.1f%%) have abrupt climate jumps (temp>%.2f or moisture>%.2f)"),
			AbruptCount, Err.AffectedPercentage * 100.0f, AbruptTempDelta, AbruptMoistureDelta);
		Err.SuggestedFix = TEXT("Increase BiomeBlendRadius or smooth climate noise (ClimateNoiseAmplitude)");
		Err.DebugMapToInspect = TEXT("Debug_BiomeMap, Debug_Temperature, Debug_Moisture");
		OutErrors.Add(MoveTemp(Err));
	}
}

// ================================================================
// Check 4: Mountains without geological control
// ================================================================
void UTerrainValidator::CheckUnsupportedMountains(
	const TArray<int32>& BiomeMap, const TArray<float>& UpliftMap,
	const TArray<uint8>& LandMask,
	TArray<FTerrainValidationError>& OutErrors)
{
	const int32 Total = Resolution * Resolution;
	if (BiomeMap.Num() != Total || UpliftMap.Num() != Total) return;

	const int32 MountainBiome = static_cast<int32>(EBiomeType::Mountain);
	const float LowUpliftThreshold = 0.15f;

	int32 UnsupportedCount = 0;
	int32 MountainPixels = 0;

	for (int32 i = 0; i < Total; ++i)
	{
		if (LandMask[i] == 0) continue;
		if (BiomeMap[i] != MountainBiome) continue;
		MountainPixels++;

		if (UpliftMap[i] < LowUpliftThreshold)
		{
			UnsupportedCount++;
		}
	}

	if (UnsupportedCount > 0 && MountainPixels > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("UnsupportedMountain");
		Err.AffectedPixelCount = UnsupportedCount;
		Err.AffectedPercentage = static_cast<float>(UnsupportedCount) / FMath::Max(MountainPixels, 1);
		Err.Severity = (Err.AffectedPercentage > 0.2f) ? EValidationSeverity::Error : EValidationSeverity::Warning;
		Err.Description = FString::Printf(
			TEXT("%d Mountain-biome pixels (%.1f%%) have low uplift (<%.2f) — no geological support"),
			UnsupportedCount, Err.AffectedPercentage * 100.0f, LowUpliftThreshold);
		Err.SuggestedFix = TEXT("Raise MountainElevationThreshold in BiomeAssignmentSettings or increase MountainRidgeAmplitude in UpliftSettings");
		Err.DebugMapToInspect = TEXT("Debug_BiomeMap, Debug_UpliftMap");
		OutErrors.Add(MoveTemp(Err));
	}
}

// ================================================================
// Check 5: Canyons without feeder rivers
// ================================================================
void UTerrainValidator::CheckCanyonsWithoutRivers(
	const TArray<uint8>& CanyonMask, const TArray<float>& RiverMap,
	TArray<FTerrainValidationError>& OutErrors)
{
	const int32 Total = Resolution * Resolution;
	if (CanyonMask.Num() != Total || RiverMap.Num() != Total) return;

	// For each canyon pixel, check if any pixel within a 3-pixel radius is a river
	const int32 SearchRadius = 3;
	int32 OrphanCount = 0;
	int32 CanyonPixels = 0;

	for (int32 i = 0; i < Total; ++i)
	{
		if (CanyonMask[i] == 0) continue;
		CanyonPixels++;

		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		bool bFoundRiver = false;

		for (int32 dy = -SearchRadius; dy <= SearchRadius && !bFoundRiver; ++dy)
		{
			for (int32 dx = -SearchRadius; dx <= SearchRadius && !bFoundRiver; ++dx)
			{
				const int32 NX = X + dx;
				const int32 NY = Y + dy;
				if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution) continue;
				if (RiverMap[NY * Resolution + NX] > 0.0f)
				{
					bFoundRiver = true;
				}
			}
		}

		if (!bFoundRiver)
		{
			OrphanCount++;
		}
	}

	if (OrphanCount > 0 && CanyonPixels > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("OrphanCanyon");
		Err.AffectedPixelCount = OrphanCount;
		Err.AffectedPercentage = static_cast<float>(OrphanCount) / FMath::Max(CanyonPixels, 1);
		Err.Severity = (Err.AffectedPercentage > 0.1f) ? EValidationSeverity::Error : EValidationSeverity::Warning;
		Err.Description = FString::Printf(
			TEXT("%d canyon pixels (%.1f%%) have no river within %d pixels"),
			OrphanCount, Err.AffectedPercentage * 100.0f, SearchRadius);
		Err.SuggestedFix = TEXT("Reduce CanyonWidth in ErosionSettings so canyons stay closer to rivers");
		Err.DebugMapToInspect = TEXT("Debug_CanyonMask, Debug_Rivers");
		OutErrors.Add(MoveTemp(Err));
	}
}

// ================================================================
// Check 6: Waterfalls without sufficient elevation drop
// ================================================================
void UTerrainValidator::CheckWaterfallElevationDrop(
	const TArray<uint8>& WaterfallMap, const TArray<float>& Elevation,
	const TArray<int32>& FlowDirection,
	TArray<FTerrainValidationError>& OutErrors)
{
	const int32 Total = Resolution * Resolution;
	if (WaterfallMap.Num() != Total || Elevation.Num() != Total || FlowDirection.Num() != Total) return;

	const float MinExpectedDrop = 0.02f;
	int32 InsufficientDropCount = 0;
	int32 WaterfallPixels = 0;

	for (int32 i = 0; i < Total; ++i)
	{
		if (WaterfallMap[i] == 0) continue;
		WaterfallPixels++;

		if (FlowDirection[i] < 0) { InsufficientDropCount++; continue; }

		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		const int32 Dir = FlowDirection[i];
		const int32 NX = X + DX8[Dir];
		const int32 NY = Y + DY8[Dir];

		if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
		{
			InsufficientDropCount++;
			continue;
		}

		const float Drop = Elevation[i] - Elevation[NY * Resolution + NX];
		if (Drop < MinExpectedDrop)
		{
			InsufficientDropCount++;
		}
	}

	if (InsufficientDropCount > 0 && WaterfallPixels > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("WeakWaterfall");
		Err.AffectedPixelCount = InsufficientDropCount;
		Err.AffectedPercentage = static_cast<float>(InsufficientDropCount) / FMath::Max(WaterfallPixels, 1);
		Err.Severity = (Err.AffectedPercentage > 0.3f) ? EValidationSeverity::Warning : EValidationSeverity::Info;
		Err.Description = FString::Printf(
			TEXT("%d waterfall sites (%.1f%%) have downstream elevation drop < %.3f"),
			InsufficientDropCount, Err.AffectedPercentage * 100.0f, MinExpectedDrop);
		Err.SuggestedFix = TEXT("Increase WaterfallMinElevationDrop in HydrologySettings");
		Err.DebugMapToInspect = TEXT("Debug_Waterfalls, Debug_ErodedElevation");
		OutErrors.Add(MoveTemp(Err));
	}
}

// ================================================================
// Check 7: Isolated wet biomes (Forest/Ice far from moisture source)
// ================================================================
void UTerrainValidator::CheckIsolatedWetBiomes(
	const TArray<int32>& BiomeMap, const TArray<float>& Moisture,
	const TArray<float>& RiverMap, const TArray<uint8>& LakeMap,
	const TArray<uint8>& LandMask,
	TArray<FTerrainValidationError>& OutErrors)
{
	const int32 Total = Resolution * Resolution;
	if (BiomeMap.Num() != Total || Moisture.Num() != Total) return;

	const int32 IceBiome = static_cast<int32>(EBiomeType::Ice);
	const float LowMoistureThreshold = 0.2f;
	const int32 WaterSearchRadius = 16;

	int32 IsolatedCount = 0;
	int32 WetBiomePixels = 0;

	for (int32 i = 0; i < Total; ++i)
	{
		if (LandMask[i] == 0) continue;
		const int32 Biome = BiomeMap[i];
		if (Biome != IceBiome) continue;
		WetBiomePixels++;

		if (Moisture[i] >= LowMoistureThreshold) continue;

		// Check if any water source (river, lake, ocean) is within search radius
		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		bool bFoundSource = false;

		for (int32 dy = -WaterSearchRadius; dy <= WaterSearchRadius && !bFoundSource; dy += 2)
		{
			for (int32 dx = -WaterSearchRadius; dx <= WaterSearchRadius && !bFoundSource; dx += 2)
			{
				const int32 NX = X + dx;
				const int32 NY = Y + dy;
				if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution) continue;
				const int32 NIdx = NY * Resolution + NX;

				if (LandMask[NIdx] == 0 // ocean
					|| (RiverMap.Num() == Total && RiverMap[NIdx] > 0.0f)
					|| (LakeMap.Num() == Total && LakeMap[NIdx] == 1))
				{
					bFoundSource = true;
				}
			}
		}

		if (!bFoundSource)
		{
			IsolatedCount++;
		}
	}

	if (IsolatedCount > 0 && WetBiomePixels > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("IsolatedWetBiome");
		Err.AffectedPixelCount = IsolatedCount;
		Err.AffectedPercentage = static_cast<float>(IsolatedCount) / FMath::Max(WetBiomePixels, 1);
		Err.Severity = (Err.AffectedPercentage > 0.15f) ? EValidationSeverity::Warning : EValidationSeverity::Info;
		Err.Description = FString::Printf(
			TEXT("%d wet-biome pixels (%.1f%%) have low moisture (<%.2f) and no water source within %d pixels"),
			IsolatedCount, Err.AffectedPercentage * 100.0f, LowMoistureThreshold, WaterSearchRadius);
		Err.SuggestedFix = TEXT("Increase WaterMoistureBoost in ClimateSettings or decrease MoistureDecayRate");
		Err.DebugMapToInspect = TEXT("Debug_BiomeMap, Debug_Moisture, Debug_Rivers");
		OutErrors.Add(MoveTemp(Err));
	}
}

// ================================================================
// Check 8: Implausible volcano placement
// ================================================================
void UTerrainValidator::CheckVolcanoPlacement(
	const TArray<FVector2D>& VolcanicCenters, const TArray<uint8>& LandMask,
	TArray<FTerrainValidationError>& OutErrors)
{
	if (VolcanicCenters.Num() == 0) return;

	const int32 Total = Resolution * Resolution;
	if (LandMask.Num() != Total) return;

	const float MinEdgeDistance = 0.05f; // normalized
	const float MinVolcanoSpacing = 0.12f;

	int32 OceanVolcanoes = 0;
	int32 EdgeVolcanoes = 0;
	int32 ClusteredPairs = 0;

	for (int32 Idx = 0; Idx < VolcanicCenters.Num(); ++Idx)
	{
		const FVector2D& VC = VolcanicCenters[Idx];

		// Check if the volcano center pixel is on land
		const int32 PX = FMath::Clamp(FMath::RoundToInt(VC.X * (Resolution - 1)), 0, Resolution - 1);
		const int32 PY = FMath::Clamp(FMath::RoundToInt(VC.Y * (Resolution - 1)), 0, Resolution - 1);
		if (LandMask[PY * Resolution + PX] == 0)
		{
			OceanVolcanoes++;
		}

		// Check edge proximity
		if (VC.X < MinEdgeDistance || VC.X > (1.0f - MinEdgeDistance) ||
			VC.Y < MinEdgeDistance || VC.Y > (1.0f - MinEdgeDistance))
		{
			EdgeVolcanoes++;
		}

		// Check clustering with other volcanoes
		for (int32 Other = Idx + 1; Other < VolcanicCenters.Num(); ++Other)
		{
			if (FVector2D::Distance(VC, VolcanicCenters[Other]) < MinVolcanoSpacing)
			{
				ClusteredPairs++;
			}
		}
	}

	if (OceanVolcanoes > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("OceanVolcano");
		Err.AffectedPixelCount = OceanVolcanoes;
		Err.AffectedPercentage = static_cast<float>(OceanVolcanoes) / VolcanicCenters.Num();
		Err.Severity = EValidationSeverity::Warning;
		Err.Description = FString::Printf(
			TEXT("%d volcano center(s) placed in ocean pixels"), OceanVolcanoes);
		Err.SuggestedFix = TEXT("Adjust VolcanicHotspotCount or increase TargetLandAreaSqKm");
		Err.DebugMapToInspect = TEXT("Debug_Landmass, Debug_UpliftMap");
		OutErrors.Add(MoveTemp(Err));
	}

	if (EdgeVolcanoes > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("EdgeVolcano");
		Err.AffectedPixelCount = EdgeVolcanoes;
		Err.AffectedPercentage = static_cast<float>(EdgeVolcanoes) / VolcanicCenters.Num();
		Err.Severity = EValidationSeverity::Info;
		Err.Description = FString::Printf(
			TEXT("%d volcano(es) placed very near the map edge (within %.0f%% of border)"),
			EdgeVolcanoes, MinEdgeDistance * 100.0f);
		Err.SuggestedFix = TEXT("Consider constraining volcanic placement in UpliftGenerator to avoid edges");
		Err.DebugMapToInspect = TEXT("Debug_UpliftMap");
		OutErrors.Add(MoveTemp(Err));
	}

	if (ClusteredPairs > 0)
	{
		FTerrainValidationError Err;
		Err.CheckName = TEXT("ClusteredVolcanoes");
		Err.AffectedPixelCount = ClusteredPairs;
		Err.AffectedPercentage = static_cast<float>(ClusteredPairs) / FMath::Max(VolcanicCenters.Num(), 1);
		Err.Severity = EValidationSeverity::Info;
		Err.Description = FString::Printf(
			TEXT("%d volcano pair(s) closer than %.0f%% of map width"),
			ClusteredPairs, MinVolcanoSpacing * 100.0f);
		Err.SuggestedFix = TEXT("Reduce VolcanicHotspotCount or increase VolcanicRadius spacing");
		Err.DebugMapToInspect = TEXT("Debug_UpliftMap");
		OutErrors.Add(MoveTemp(Err));
	}
}
