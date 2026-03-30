#include "UpliftGenerator.h"
#include "NoiseUtility.h"

//------------------------------------------------------------------------------
// Initialize: store settings, derive seed, record resolution
//------------------------------------------------------------------------------
void UUpliftGenerator::Initialize(const FUpliftSettings& InSettings, int32 GlobalSeed, int32 TextureResolution)
{
	Settings = InSettings;

	ActualSeed = (Settings.Seed != 0)
		? Settings.Seed
		: WorldNoise::DeriveSeed(GlobalSeed, 2);

	Resolution = TextureResolution;

	UE_LOG(LogTemp, Log,
		TEXT("UpliftGenerator initialized – Resolution: %d, Seed: %d, CoastGrad: %d, MtnFreq: %.2f"),
		Resolution, ActualSeed, Settings.CoastlineGradientWidth, Settings.MountainRidgeFrequency);
}

//------------------------------------------------------------------------------
// Generate: run the full uplift pipeline
//------------------------------------------------------------------------------
bool UUpliftGenerator::Generate(const TArray<uint8>& LandMask)
{
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("UpliftGenerator::Generate – Resolution not set. Call Initialize first."));
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;

	BaseElevation.SetNum(TotalPixels);
	UpliftMap.SetNum(TotalPixels);
	CombinedElevation.SetNum(TotalPixels);
	PlateauMap.SetNum(TotalPixels);
	VolcanicCenters.Empty();

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator::Generate – starting (%d pixels)"), TotalPixels);

	GenerateBaseElevation(LandMask);
	GenerateUpliftMap(LandMask);
	GenerateHills(LandMask);
	GenerateVolcanicHotspots(LandMask);
	GeneratePlateaus(LandMask);
	CombineElevation();

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator::Generate – complete"));
	return true;
}

//------------------------------------------------------------------------------
// ComputeCoastlineDistance: BFS from ocean pixels outward
//   Ocean pixels → distance 0, land pixels → shortest pixel distance to ocean.
//------------------------------------------------------------------------------
void UUpliftGenerator::ComputeCoastlineDistance(const TArray<uint8>& LandMask, TArray<float>& OutDistances)
{
	const int32 TotalPixels = Resolution * Resolution;
	OutDistances.SetNum(TotalPixels);

	constexpr float LargeValue = 1e9f;

	TQueue<int32> Queue;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			OutDistances[i] = 0.0f;
			Queue.Enqueue(i);
		}
		else
		{
			OutDistances[i] = LargeValue;
		}
	}

	// 4-connected BFS
	while (!Queue.IsEmpty())
	{
		int32 Idx;
		Queue.Dequeue(Idx);

		const int32 X = Idx % Resolution;
		const int32 Y = Idx / Resolution;
		const float NextDist = OutDistances[Idx] + 1.0f;

		const int32 DX[] = { -1, 1,  0, 0 };
		const int32 DY[] = {  0, 0, -1, 1 };

		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const int32 NX = X + DX[Dir];
			const int32 NY = Y + DY[Dir];

			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
			{
				continue;
			}

			const int32 NIdx = NY * Resolution + NX;

			if (NextDist < OutDistances[NIdx])
			{
				OutDistances[NIdx] = NextDist;
				Queue.Enqueue(NIdx);
			}
		}
	}
}

//------------------------------------------------------------------------------
// GenerateBaseElevation:
//   1. Coastline distance → gradient clamped to [0,1]
//      Gradient width varies spatially via low-frequency noise so different
//      sides of the island can have steep cliffs or gentle beaches.
//   2. Modulate with FBM noise to break up uniformity
//   3. Ocean pixels = 0
//------------------------------------------------------------------------------
void UUpliftGenerator::GenerateBaseElevation(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;

	TArray<float> CoastDist;
	ComputeCoastlineDistance(LandMask, CoastDist);

	const float GradWidth = FMath::Max(static_cast<float>(Settings.CoastlineGradientWidth), 1.0f);
	const float Freq      = Settings.BaseNoiseFrequency;
	const int32 Octaves   = Settings.BaseNoiseOctaves;
	const float Persist    = Settings.BaseNoisePersistence;
	const int32 Seed       = ActualSeed;
	const float InvRes     = 1.0f / FMath::Max(Resolution - 1, 1);

	// Coastal variation: use low-frequency noise to vary gradient width per pixel
	const float CoastalVar     = FMath::Clamp(Settings.CoastalVariation, 0.0f, 1.0f);
	const float CoastalVarFreq = Settings.CoastalVariationFrequency;
	// Offset seed to decorrelate coastal variation noise from base elevation noise
	const int32 CoastalVarSeed = ActualSeed + 500;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			BaseElevation[i] = 0.0f;
			continue;
		}

		const float NormX = static_cast<float>(i % Resolution) * InvRes;
		const float NormY = static_cast<float>(i / Resolution) * InvRes;

		// Per-pixel gradient width: modulate by low-frequency noise
		float LocalGradWidth = GradWidth;
		if (CoastalVar > 0.0f)
		{
			// Sample low-frequency noise for this position, returns ~[-1, 1]
			const float VarNoise = WorldNoise::FBM(NormX * CoastalVarFreq, NormY * CoastalVarFreq, 2, 0.5f, CoastalVarSeed);
			// Map noise to a multiplier: centre on 1.0, spread by CoastalVar
			// At CoastalVar=1: multiplier ranges [0.25, 1.75] → steep cliffs to gentle beaches
			const float Multiplier = 1.0f + VarNoise * CoastalVar * 0.75f;
			LocalGradWidth = FMath::Max(GradWidth * Multiplier, 1.0f);
		}

		// Coastline distance gradient
		float Elev = FMath::Clamp(CoastDist[i] / LocalGradWidth, 0.0f, 1.0f);

		// Modulate with FBM noise
		const float Noise = WorldNoise::FBM(NormX * Freq, NormY * Freq, Octaves, Persist, Seed);

		// Remap FBM [-1,1] → [0,1], then clamp to [0.2,1.0] so coasts never
		// collapse to zero (0.2 floor) while inland areas get full modulation.
		const float NoiseMod = FMath::Clamp(0.5f + 0.5f * Noise, 0.2f, 1.0f);

		BaseElevation[i] = Elev * NoiseMod;
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – BaseElevation generated (gradient width %d, coastal variation %.2f)"),
		Settings.CoastlineGradientWidth, Settings.CoastalVariation);
}

//------------------------------------------------------------------------------
// GenerateUpliftMap:
//   Ridged FBM for mountain ranges, multiplied by BaseElevation so ridges
//   fade toward the coast. Ocean = 0.
//------------------------------------------------------------------------------
void UUpliftGenerator::GenerateUpliftMap(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const float Freq       = Settings.MountainRidgeFrequency;
	const float Amplitude  = Settings.MountainRidgeAmplitude;
	const float Sharpness  = Settings.MountainSharpness;
	const int32 Octaves    = Settings.MountainOctaves;
	// Offset seed by a fixed amount to decorrelate mountain noise from base elevation noise
	const int32 Seed       = ActualSeed + 100;
	const float InvRes     = 1.0f / FMath::Max(Resolution - 1, 1);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			UpliftMap[i] = 0.0f;
			continue;
		}

		const float NormX = static_cast<float>(i % Resolution) * InvRes;
		const float NormY = static_cast<float>(i / Resolution) * InvRes;

		float Ridge = WorldNoise::RidgedFBM(NormX * Freq, NormY * Freq, Octaves, 0.5f, Sharpness, Seed);
		Ridge *= Amplitude;

		// Fade mountains near coastline by multiplying with base elevation
		UpliftMap[i] = Ridge * BaseElevation[i];
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – UpliftMap generated (freq %.2f, amp %.2f)"),
		Freq, Amplitude);
}

//------------------------------------------------------------------------------
// GenerateVolcanicHotspots:
//   Random cone-shaped volcanoes with craters, added to UpliftMap.
//------------------------------------------------------------------------------
void UUpliftGenerator::GenerateVolcanicHotspots(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const int32 HotspotCount = Settings.VolcanicHotspotCount;

	if (HotspotCount <= 0)
	{
		return;
	}

	// Offset seed to decorrelate hotspot placement from elevation/mountain noise
	FRandomStream RandStream(ActualSeed + 200);

	const float Radius         = Settings.VolcanicRadius;
	const float PeakHeight     = Settings.VolcanicPeakHeight;
	const float CraterDepth    = Settings.CraterDepth;
	const float CraterFraction = Settings.CraterRadiusFraction;

	// Gather land pixel indices for candidate placement
	TArray<int32> LandIndices;
	LandIndices.Reserve(TotalPixels / 2);
	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] != 0)
		{
			LandIndices.Add(i);
		}
	}

	if (LandIndices.Num() == 0)
	{
		return;
	}

	const float InvRes = 1.0f / FMath::Max(Resolution - 1, 1);

	for (int32 H = 0; H < HotspotCount; ++H)
	{
		// Pick a random land pixel as the hotspot center
		const int32 CenterIdx = LandIndices[RandStream.RandHelper(LandIndices.Num())];
		const float CenterX = static_cast<float>(CenterIdx % Resolution) * InvRes;
		const float CenterY = static_cast<float>(CenterIdx / Resolution) * InvRes;

		VolcanicCenters.Add(FVector2D(CenterX, CenterY));

		// Rasterize the cone within a bounding box around the hotspot
		const int32 PixelRadius = FMath::CeilToInt(Radius * Resolution);
		const int32 MinX = FMath::Max(0, (CenterIdx % Resolution) - PixelRadius);
		const int32 MaxX = FMath::Min(Resolution - 1, (CenterIdx % Resolution) + PixelRadius);
		const int32 MinY = FMath::Max(0, (CenterIdx / Resolution) - PixelRadius);
		const int32 MaxY = FMath::Min(Resolution - 1, (CenterIdx / Resolution) + PixelRadius);

		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const int32 Idx = Y * Resolution + X;

				if (LandMask[Idx] == 0)
				{
					continue;
				}

				const float PX = static_cast<float>(X) * InvRes;
				const float PY = static_cast<float>(Y) * InvRes;
				const float Dist = FMath::Sqrt(FMath::Square(PX - CenterX) + FMath::Square(PY - CenterY));

				if (Dist > Radius)
				{
					continue;
				}

				const float NormDist = Dist / Radius; // 0 at center, 1 at edge

				// Cone shape: linearly decreasing from peak at center to 0 at edge
				float Height = PeakHeight * (1.0f - NormDist);

				// Crater: subtract a bowl inside the crater radius fraction
				if (NormDist < CraterFraction)
				{
					const float CraterNorm = NormDist / CraterFraction; // 0 at center, 1 at rim
					Height -= CraterDepth * PeakHeight * (1.0f - CraterNorm);
				}

				Height = FMath::Max(Height, 0.0f);

				// Additive contribution to the uplift map
				UpliftMap[Idx] = FMath::Clamp(UpliftMap[Idx] + Height, 0.0f, 1.0f);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – %d volcanic hotspots placed"), VolcanicCenters.Num());
}

//------------------------------------------------------------------------------
// GenerateHills:
//   FBM-based rolling hills, added to UpliftMap.
//   Hills are smaller-scale undulations distinct from mountain ridges.
//   They are modulated by BaseElevation so they fade near coastlines.
//------------------------------------------------------------------------------
void UUpliftGenerator::GenerateHills(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const float Freq = Settings.HillFrequency;
	const float Amplitude = Settings.HillAmplitude;
	const int32 Octaves = Settings.HillOctaves;
	const int32 Seed = ActualSeed + 300; // Decorrelate from mountain noise
	const float InvRes = 1.0f / FMath::Max(Resolution - 1, 1);

	if (Amplitude <= 0.0f)
	{
		return;
	}

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			continue;
		}

		const float NormX = static_cast<float>(i % Resolution) * InvRes;
		const float NormY = static_cast<float>(i / Resolution) * InvRes;

		float Hill = WorldNoise::FBM(NormX * Freq, NormY * Freq, Octaves, 0.5f, Seed);
		// Remap [-1,1] → [0,1] then scale by amplitude
		Hill = (Hill * 0.5f + 0.5f) * Amplitude;

		// Fade near coastline by multiplying with base elevation
		UpliftMap[i] += Hill * BaseElevation[i];
		UpliftMap[i] = FMath::Clamp(UpliftMap[i], 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – Hills generated (freq %.2f, amp %.2f)"),
		Freq, Amplitude);
}

//------------------------------------------------------------------------------
// GeneratePlateaus:
//   Identify plateau regions using thresholded FBM noise and flatten terrain
//   in those regions to a target elevation, producing flat-topped elevated areas.
//   PlateauMap stores the plateau mask [0,1] per pixel.
//------------------------------------------------------------------------------
void UUpliftGenerator::GeneratePlateaus(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const float Freq = Settings.PlateauNoiseFrequency;
	const float Threshold = Settings.PlateauThreshold;
	const float Flatness = Settings.PlateauFlatness;
	const float TargetElev = Settings.PlateauElevation;
	const int32 Seed = ActualSeed + 400; // Decorrelate from other noise
	const float InvRes = 1.0f / FMath::Max(Resolution - 1, 1);

	FMemory::Memzero(PlateauMap.GetData(), TotalPixels * sizeof(float));

	if (Flatness <= 0.0f)
	{
		return;
	}

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			continue;
		}

		const float NormX = static_cast<float>(i % Resolution) * InvRes;
		const float NormY = static_cast<float>(i / Resolution) * InvRes;

		// Plateau noise field — remap [-1,1] to [0,1]
		float PlateauNoise = WorldNoise::FBM(NormX * Freq, NormY * Freq, 3, 0.5f, Seed);
		PlateauNoise = PlateauNoise * 0.5f + 0.5f;

		if (PlateauNoise < Threshold)
		{
			continue;
		}

		// Only apply plateaus to areas with moderate elevation (not ocean-edge or mountain-peak)
		const float CurrentElev = BaseElevation[i] + UpliftMap[i];
		if (CurrentElev < 0.15f || CurrentElev > 0.8f)
		{
			continue;
		}

		// Smooth transition into plateau zone
		const float PlateauStrength = FMath::Clamp((PlateauNoise - Threshold) / (1.0f - Threshold), 0.0f, 1.0f);
		PlateauMap[i] = PlateauStrength;

		// Flatten toward target elevation: lerp current uplift toward TargetElev
		const float DesiredUplift = FMath::Max(TargetElev - BaseElevation[i], 0.0f);
		UpliftMap[i] = FMath::Lerp(UpliftMap[i], DesiredUplift, PlateauStrength * Flatness);
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – Plateaus generated (freq %.2f, threshold %.2f, flatness %.2f)"),
		Freq, Threshold, Flatness);
}

//------------------------------------------------------------------------------
// CombineElevation: BaseElevation + UpliftMap → CombinedElevation, clamped [0,1]
//------------------------------------------------------------------------------
void UUpliftGenerator::CombineElevation()
{
	const int32 TotalPixels = Resolution * Resolution;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		CombinedElevation[i] = FMath::Clamp(BaseElevation[i] + UpliftMap[i], 0.0f, 1.0f);
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – CombinedElevation computed"));
}
