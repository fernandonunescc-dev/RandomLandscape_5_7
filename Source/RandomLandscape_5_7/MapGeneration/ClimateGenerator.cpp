#include "ClimateGenerator.h"
#include "NoiseUtility.h"

//------------------------------------------------------------------------------
// 4-connected neighbor offsets for BFS: E, N, W, S
//------------------------------------------------------------------------------
static constexpr int32 CDX4[] = {  1,  0, -1,  0 };
static constexpr int32 CDY4[] = {  0, -1,  0,  1 };

//------------------------------------------------------------------------------
// Initialize: store settings, derive seed, record resolution
//------------------------------------------------------------------------------
void UClimateGenerator::Initialize(const FClimateSettings& InSettings, int32 GlobalSeed, int32 TextureResolution)
{
	Settings = InSettings;

	ActualSeed = (Settings.Seed != 0)
		? Settings.Seed
		: WorldNoise::DeriveSeed(GlobalSeed, 5);

	Resolution = TextureResolution;

	UE_LOG(LogTemp, Log,
		TEXT("ClimateGenerator initialized – Resolution: %d, Seed: %d, EquatorPos: %.2f, LapseRate: %.2f"),
		Resolution, ActualSeed, Settings.EquatorPosition, Settings.ElevationLapseRate);
}

//------------------------------------------------------------------------------
// Generate: validate inputs, then run the three climate sub-passes
//------------------------------------------------------------------------------
bool UClimateGenerator::Generate(const TArray<float>& Elevation, const TArray<uint8>& LandMask,
	const TArray<float>& RiverMap, const TArray<uint8>& LakeMap)
{
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ClimateGenerator::Generate – Resolution not set. Call Initialize first."));
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;

	if (Elevation.Num() != TotalPixels || LandMask.Num() != TotalPixels
		|| RiverMap.Num() != TotalPixels || LakeMap.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error,
			TEXT("ClimateGenerator::Generate – Input array size mismatch (expected %d)"), TotalPixels);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ClimateGenerator::Generate – starting (%d pixels)"), TotalPixels);

	ComputeTemperature(Elevation, LandMask);
	ComputeMoisture(Elevation, LandMask, RiverMap, LakeMap);
	ComputePrecipitation();

	UE_LOG(LogTemp, Log, TEXT("ClimateGenerator::Generate – complete"));
	return true;
}

//------------------------------------------------------------------------------
// ComputeTemperature:
//   Latitude-based gradient from equator to poles, reduced by an elevation
//   lapse rate and perturbed by fractal noise.
//------------------------------------------------------------------------------
void UClimateGenerator::ComputeTemperature(const TArray<float>& Elevation,
	const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	Temperature.SetNumUninitialized(TotalPixels);

	const float InvRes = 1.0f / static_cast<float>(Resolution);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		const float NormX = static_cast<float>(X) * InvRes;
		const float NormY = static_cast<float>(Y) * InvRes;

		// Latitude factor: distance from equator, normalized to [0,1]
		const float LatitudeFactor = FMath::Abs(NormY - Settings.EquatorPosition) * 2.0f;
		float Temp = FMath::Lerp(Settings.EquatorialTemperature, Settings.PolarTemperature,
			FMath::Clamp(LatitudeFactor, 0.0f, 1.0f));

		// Elevation lapse
		Temp -= Elevation[i] * Settings.ElevationLapseRate;

		// Climate noise
		Temp += WorldNoise::FBM(
			NormX * Settings.ClimateNoiseFrequency,
			NormY * Settings.ClimateNoiseFrequency,
			3, 0.5f, ActualSeed) * Settings.ClimateNoiseAmplitude;

		Temperature[i] = FMath::Clamp(Temp, 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("ClimateGenerator – Temperature map computed"));
}

//------------------------------------------------------------------------------
// ComputeMoisture:
//   1. BFS from ocean pixels to compute distance-from-coast
//   2. Exponential decay inland
//   3. Rain shadow sweep in prevailing wind direction
//   4. River/lake moisture boost
//------------------------------------------------------------------------------
void UClimateGenerator::ComputeMoisture(const TArray<float>& Elevation,
	const TArray<uint8>& LandMask, const TArray<float>& RiverMap, const TArray<uint8>& LakeMap)
{
	const int32 TotalPixels = Resolution * Resolution;
	Moisture.SetNumUninitialized(TotalPixels);

	// ---- Step 1: BFS distance from coast ----------------------------------
	TArray<float> CoastDist;
	CoastDist.SetNumUninitialized(TotalPixels);
	const float Unvisited = static_cast<float>(Resolution * 2);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		CoastDist[i] = Unvisited;
	}

	TQueue<int32> BFSQueue;

	// Seed BFS with ocean pixels adjacent to land
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] != 0)
		{
			continue;
		}

		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;

		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const int32 NX = X + CDX4[Dir];
			const int32 NY = Y + CDY4[Dir];

			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
			{
				continue;
			}

			const int32 NIdx = NY * Resolution + NX;
			if (LandMask[NIdx] != 0 && CoastDist[NIdx] > 0.5f)
			{
				CoastDist[NIdx] = 1.0f;
				BFSQueue.Enqueue(NIdx);
			}
		}
	}

	while (!BFSQueue.IsEmpty())
	{
		int32 Cur;
		BFSQueue.Dequeue(Cur);

		const int32 CX = Cur % Resolution;
		const int32 CY = Cur / Resolution;
		const float CurDist = CoastDist[Cur];

		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const int32 NX = CX + CDX4[Dir];
			const int32 NY = CY + CDY4[Dir];

			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
			{
				continue;
			}

			const int32 NIdx = NY * Resolution + NX;
			const float NewDist = CurDist + 1.0f;

			if (LandMask[NIdx] != 0 && NewDist < CoastDist[NIdx])
			{
				CoastDist[NIdx] = NewDist;
				BFSQueue.Enqueue(NIdx);
			}
		}
	}

	// ---- Step 2: Base moisture from coastal proximity ----------------------
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			// Ocean pixels get full moisture
			Moisture[i] = 1.0f;
		}
		else
		{
			Moisture[i] = FMath::Exp(-CoastDist[i] * Settings.MoistureDecayRate);
		}
	}

	// ---- Step 3: Rain shadow sweep ----------------------------------------
	// Determine sweep direction from WindDirection (0=East→West sweep)
	const float WindAngle = Settings.WindDirection * 2.0f * PI;
	const float WindCosine = FMath::Cos(WindAngle);
	const float WindSine = FMath::Sin(WindAngle);

	// Determine primary sweep axis: sweep along the dominant wind component
	const bool SweepHorizontal = FMath::Abs(WindCosine) >= FMath::Abs(WindSine);

	if (SweepHorizontal)
	{
		// Wind blows mostly along X. Sweep each row.
		const bool LeftToRight = (WindCosine > 0.0f);

		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			float PrevElev = 0.0f;
			const int32 Start = LeftToRight ? 0 : (Resolution - 1);
			const int32 End   = LeftToRight ? Resolution : -1;
			const int32 Step  = LeftToRight ? 1 : -1;

			for (int32 X = Start; X != End; X += Step)
			{
				const int32 Idx = Y * Resolution + X;
				if (LandMask[Idx] == 0)
				{
					PrevElev = 0.0f;
					continue;
				}

				const float CurElev = Elevation[Idx];
				const float Slope = CurElev - PrevElev;

				if (Slope > 0.0f)
				{
					// Rising terrain blocks moisture
					Moisture[Idx] -= Slope * Settings.RainShadowStrength;
				}

				PrevElev = CurElev;
			}
		}
	}
	else
	{
		// Wind blows mostly along Y. Sweep each column.
		const bool TopToBottom = (WindSine > 0.0f);

		for (int32 X = 0; X < Resolution; ++X)
		{
			float PrevElev = 0.0f;
			const int32 Start = TopToBottom ? 0 : (Resolution - 1);
			const int32 End   = TopToBottom ? Resolution : -1;
			const int32 Step  = TopToBottom ? 1 : -1;

			for (int32 Y = Start; Y != End; Y += Step)
			{
				const int32 Idx = Y * Resolution + X;
				if (LandMask[Idx] == 0)
				{
					PrevElev = 0.0f;
					continue;
				}

				const float CurElev = Elevation[Idx];
				const float Slope = CurElev - PrevElev;

				if (Slope > 0.0f)
				{
					Moisture[Idx] -= Slope * Settings.RainShadowStrength;
				}

				PrevElev = CurElev;
			}
		}
	}

	// ---- Step 4: Water body boost (rivers & lakes) ------------------------
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (RiverMap[i] > 0.0f)
		{
			Moisture[i] += Settings.WaterMoistureBoost;
		}
		if (LakeMap[i] == 1)
		{
			Moisture[i] += Settings.WaterMoistureBoost;
		}

		Moisture[i] = FMath::Clamp(Moisture[i], 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("ClimateGenerator – Moisture map computed (DecayRate: %.4f, RainShadow: %.2f)"),
		Settings.MoistureDecayRate, Settings.RainShadowStrength);
}

//------------------------------------------------------------------------------
// ComputePrecipitation:
//   Warmer + wetter = more rain. Cold + dry = least precipitation.
//------------------------------------------------------------------------------
void UClimateGenerator::ComputePrecipitation()
{
	const int32 TotalPixels = Resolution * Resolution;
	Precipitation.SetNumUninitialized(TotalPixels);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		const float Precip = Moisture[i] * (0.5f + 0.5f * Temperature[i]);
		Precipitation[i] = FMath::Clamp(Precip, 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("ClimateGenerator – Precipitation map computed"));
}
