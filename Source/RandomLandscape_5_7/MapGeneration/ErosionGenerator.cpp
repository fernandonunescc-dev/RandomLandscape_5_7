#include "ErosionGenerator.h"
#include "NoiseUtility.h"

//------------------------------------------------------------------------------
// 8-connected neighbor offsets: E, NE, N, NW, W, SW, S, SE
//------------------------------------------------------------------------------
static constexpr int32 DX8[] = {  1,  1,  0, -1, -1, -1,  0,  1 };
static constexpr int32 DY8[] = {  0, -1, -1, -1,  0,  1,  1,  1 };

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
		TEXT("ErosionGenerator initialized – Resolution: %d, Seed: %d, IncisionStrength: %.2f, ThermalPasses: %d"),
		Resolution, ActualSeed, Settings.RiverIncisionStrength, Settings.ThermalErosionPasses);
}

//------------------------------------------------------------------------------
// Generate: copy elevation, then run incision and thermal erosion
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

	UE_LOG(LogTemp, Log, TEXT("ErosionGenerator::Generate – starting (%d pixels)"), TotalPixels);

	ApplyRiverIncision(RiverMap, UpliftMap, LandMask);
	ApplyThermalErosion(LandMask);

	UE_LOG(LogTemp, Log, TEXT("ErosionGenerator::Generate – complete"));
	return true;
}

//------------------------------------------------------------------------------
// ApplyRiverIncision:
//   For each land pixel where the river map is positive, lower the elevation
//   proportionally.  High-uplift regions receive a deeper cut so that rivers
//   carve pronounced canyons through mountain terrain.
//------------------------------------------------------------------------------
void UErosionGenerator::ApplyRiverIncision(const TArray<float>& RiverMap,
	const TArray<float>& UpliftMap, const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
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

		const float OldElev = ErodedElevation[i];
		ErodedElevation[i] = FMath::Max(0.0f, OldElev - Incision);

		TotalRemoved += static_cast<double>(OldElev - ErodedElevation[i]);
		AffectedPixels++;
	}

	UE_LOG(LogTemp, Log,
		TEXT("ErosionGenerator – River incision: %d pixels carved, total elevation removed: %.4f"),
		AffectedPixels, TotalRemoved);
}

//------------------------------------------------------------------------------
// ApplyThermalErosion:
//   Iterative smoothing of steep slopes.  For each pass, every land pixel
//   checks its 8 neighbors.  When the height difference exceeds the threshold,
//   material is transferred from the higher cell to the lower one.
//   A temporary buffer prevents read/write conflicts within a single pass.
//------------------------------------------------------------------------------
void UErosionGenerator::ApplyThermalErosion(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const int32 Passes = Settings.ThermalErosionPasses;
	const float Threshold = Settings.ThermalErosionThreshold;
	const float Rate = Settings.ThermalErosionRate;

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
					const float Transfer = Rate * (Diff - Threshold) * 0.5f;
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
		TEXT("ErosionGenerator – Thermal erosion complete (%d passes, threshold: %.4f, rate: %.2f)"),
		Passes, Threshold, Rate);
}
