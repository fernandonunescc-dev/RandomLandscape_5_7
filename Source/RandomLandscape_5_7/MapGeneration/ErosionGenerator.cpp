#include "ErosionGenerator.h"
#include "NoiseUtility.h"

//------------------------------------------------------------------------------
// 8-connected neighbor offsets: imported from WorldNoise namespace
//------------------------------------------------------------------------------
using WorldNoise::DX8;
using WorldNoise::DY8;

//------------------------------------------------------------------------------
// Initialize: store settings, derive seed, record resolution
//------------------------------------------------------------------------------
void UErosionGenerator::Initialize(const FErosionSettings& InSettings, int32 GlobalSeed, int32 TextureResolution)
{
	Settings = InSettings;

	ActualSeed = (Settings.Seed != 0)
		? Settings.Seed
		: WorldNoise::DeriveSeed(GlobalSeed, 4);

	Resolution = TextureResolution;

	UE_LOG(LogTemp, Log,
		TEXT("ErosionGenerator initialized – Resolution: %d, Seed: %d, IncisionStrength: %.2f, CanyonWidth: %d, ThermalPasses: %d"),
		Resolution, ActualSeed, Settings.RiverIncisionStrength, Settings.CanyonWidth, Settings.ThermalErosionPasses);
}

//------------------------------------------------------------------------------
// Generate: copy elevation, then run incision and thermal erosion, compute delta
//------------------------------------------------------------------------------
bool UErosionGenerator::Generate(const TArray<float>& InputElevation, const TArray<float>& RiverMap,
	const TArray<float>& UpliftMap, const TArray<uint8>& LandMask)
{
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ErosionGenerator::Generate – Resolution not set. Call Initialize first."));
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;

	if (InputElevation.Num() != TotalPixels || RiverMap.Num() != TotalPixels
		|| UpliftMap.Num() != TotalPixels || LandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ErosionGenerator::Generate – Input array size mismatch (expected %d)"), TotalPixels);
		return false;
	}

	// Start from the input elevation
	ErodedElevation = InputElevation;

	// Allocate canyon mask and erosion delta
	CanyonMask.SetNumZeroed(TotalPixels);
	ErosionDeltaMap.SetNumZeroed(TotalPixels);

	UE_LOG(LogTemp, Log, TEXT("ErosionGenerator::Generate – starting (%d pixels)"), TotalPixels);

	ApplyRiverIncision(RiverMap, UpliftMap, LandMask);
	ApplyThermalErosion(LandMask);

	// Compute erosion delta: original minus final (positive = removed)
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		ErosionDeltaMap[i] = InputElevation[i] - ErodedElevation[i];
	}

	UE_LOG(LogTemp, Log, TEXT("ErosionGenerator::Generate – complete"));
	return true;
}

//------------------------------------------------------------------------------
// ApplyRiverIncision:
//   For each land pixel where the river map is positive, lower the elevation
//   proportionally.  High-uplift regions receive a deeper cut so that rivers
//   carve pronounced canyons through mountain terrain.
//
//   Width expansion: After computing per-river-pixel incision depths, a BFS
//   expands the carving outward by CanyonWidth pixels.  The incision depth
//   tapers linearly from full depth at the river center to zero at the edge.
//   All carved pixels are marked in CanyonMask.
//------------------------------------------------------------------------------
void UErosionGenerator::ApplyRiverIncision(const TArray<float>& RiverMap,
	const TArray<float>& UpliftMap, const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const int32 CanyonW = FMath::Max(0, Settings.CanyonWidth);

	// Per-pixel incision depth (center-line only for now)
	TArray<float> IncisionDepth;
	IncisionDepth.SetNumZeroed(TotalPixels);

	double TotalRemoved = 0.0;
	int32 AffectedPixels = 0;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0 || RiverMap[i] <= 0.0f)
		{
			continue;
		}

		float Incision = RiverMap[i] * Settings.RiverIncisionStrength;

		// Deepen canyons in high-uplift areas
		Incision *= (1.0f + UpliftMap[i] * Settings.CanyonDepthMultiplier);

		IncisionDepth[i] = Incision;
	}

	// Width expansion: BFS from river pixels outward up to CanyonWidth steps.
	// At each step, taper the incision linearly.
	if (CanyonW > 0)
	{
		// BFS distance from nearest river pixel (INT_MAX = unvisited)
		TArray<int32> Dist;
		Dist.SetNumUninitialized(TotalPixels);
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			Dist[i] = (IncisionDepth[i] > 0.0f) ? 0 : TotalPixels; // river=0, else large
		}

		// Seed queue with all river pixels
		TQueue<int32> Queue;
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (Dist[i] == 0)
			{
				Queue.Enqueue(i);
			}
		}

		// BFS level-by-level up to CanyonW
		while (!Queue.IsEmpty())
		{
			int32 Idx;
			Queue.Dequeue(Idx);

			const int32 CurDist = Dist[Idx];
			if (CurDist >= CanyonW)
			{
				continue;
			}

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
				const int32 NewDist = CurDist + 1;

				if (NewDist < Dist[NIdx] && LandMask[NIdx] != 0)
				{
					Dist[NIdx] = NewDist;

					// Taper incision linearly: full at dist 0, zero at dist CanyonW
					const float Taper = 1.0f - (static_cast<float>(NewDist) / static_cast<float>(CanyonW));

					// Find the incision from the river pixel that seeded this BFS direction.
					// Use the center pixel's incision as the source depth.
					const float SourceDepth = IncisionDepth[Idx];
					const float TaperedDepth = SourceDepth * Taper;

					// Keep the maximum incision if this pixel is reached from multiple rivers
					if (TaperedDepth > IncisionDepth[NIdx])
					{
						IncisionDepth[NIdx] = TaperedDepth;
					}

					Queue.Enqueue(NIdx);
				}
			}
		}
	}

	// Apply incision to elevation and mark canyon mask
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (IncisionDepth[i] > 0.0f)
		{
			const float OldElev = ErodedElevation[i];
			ErodedElevation[i] = FMath::Max(0.0f, OldElev - IncisionDepth[i]);

			const float ActualRemoved = OldElev - ErodedElevation[i];
			if (ActualRemoved > 0.0f)
			{
				CanyonMask[i] = 1;
				TotalRemoved += static_cast<double>(ActualRemoved);
				AffectedPixels++;
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("ErosionGenerator – River incision: %d pixels carved (width: %d), total elevation removed: %.4f"),
		AffectedPixels, CanyonW, TotalRemoved);
}

//------------------------------------------------------------------------------
// ApplyThermalErosion:
//   Iterative smoothing of steep slopes.  For each pass, every land pixel
//   checks its 8 neighbors.  When the height difference exceeds the threshold,
//   material is transferred from the higher cell to the lower one.
//   A temporary buffer prevents read/write conflicts within a single pass.
//
//   Canyon wall protection: when CanyonWallSteepness > 0, pixels in the
//   CanyonMask have their effective erosion rate reduced, preserving steep
//   canyon walls from being smoothed away.
//------------------------------------------------------------------------------
void UErosionGenerator::ApplyThermalErosion(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const int32 Passes = Settings.ThermalErosionPasses;
	const float Threshold = Settings.ThermalErosionThreshold;
	const float Rate = Settings.ThermalErosionRate;
	const float WallProtection = FMath::Clamp(Settings.CanyonWallSteepness, 0.0f, 1.0f);

	if (Passes <= 0)
	{
		return;
	}

	TArray<float> TempBuffer;
	TempBuffer.SetNum(TotalPixels);

	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		FMemory::Memcpy(TempBuffer.GetData(), ErodedElevation.GetData(), TotalPixels * sizeof(float));

		double TotalMoved = 0.0;

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (LandMask[i] == 0)
			{
				continue;
			}

			const int32 X = i % Resolution;
			const int32 Y = i / Resolution;
			const float CenterH = ErodedElevation[i];

			// Reduce erosion rate for canyon pixels to preserve steep walls
			const float EffectiveRate = (CanyonMask[i] != 0)
				? Rate * (1.0f - WallProtection)
				: Rate;

			for (int32 Dir = 0; Dir < 8; ++Dir)
			{
				const int32 NX = X + DX8[Dir];
				const int32 NY = Y + DY8[Dir];

				if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
				{
					continue;
				}

				const int32 NIdx = NY * Resolution + NX;
				const float NeighH = ErodedElevation[NIdx];
				const float Diff = CenterH - NeighH;

				if (Diff > Threshold)
				{
					const float Transfer = FMath::Min(EffectiveRate * (Diff - Threshold) * 0.5f, TempBuffer[i] * 0.25f);
					TempBuffer[i] -= Transfer;
					TempBuffer[NIdx] += Transfer;
					TotalMoved += static_cast<double>(Transfer);
				}
			}
		}

		// Clamp and swap
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			ErodedElevation[i] = FMath::Max(0.0f, TempBuffer[i]);
		}

		UE_LOG(LogTemp, Verbose,
			TEXT("ErosionGenerator – Thermal pass %d/%d: total material moved %.6f"),
			Pass + 1, Passes, TotalMoved);
	}

	UE_LOG(LogTemp, Log,
		TEXT("ErosionGenerator – Thermal erosion complete (%d passes, threshold: %.4f, rate: %.2f, wall protection: %.2f)"),
		Passes, Threshold, Rate, WallProtection);
}
