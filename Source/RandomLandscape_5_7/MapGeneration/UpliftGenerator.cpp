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
	CliffFactor.SetNum(TotalPixels);
	PlateauMap.SetNum(TotalPixels);
	CoastlineDistance.Empty();
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
//   4. MinCliffCoverage guarantee: bias noise so at least N% of coast is cliffs
//------------------------------------------------------------------------------
void UUpliftGenerator::GenerateBaseElevation(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;

	// Compute and store coastline distance (reused by mountain/hill stages)
	ComputeCoastlineDistance(LandMask, CoastlineDistance);

	const float GradWidth = FMath::Max(static_cast<float>(Settings.CoastlineGradientWidth), 1.0f);
	const float Freq      = Settings.BaseNoiseFrequency;
	const int32 Octaves   = Settings.BaseNoiseOctaves;
	const float Persist    = Settings.BaseNoisePersistence;
	const int32 Seed       = ActualSeed;
	const float InvRes     = 1.0f / FMath::Max(Resolution - 1, 1);

	// Coastal variation: use low-frequency noise to vary gradient width per pixel.
	// At high variation, some coastlines become very steep cliffs (2-3 pixel gradient)
	// while others remain gentle beaches (full configured gradient width).
	const float CoastalVar     = FMath::Clamp(Settings.CoastalVariation, 0.0f, 1.0f);
	const float CoastalVarFreq = Settings.CoastalVariationFrequency;
	// Decorrelate coastal variation noise from other sub-stage seeds
	const int32 CoastalVarSeed = WorldNoise::DeriveSeed(ActualSeed, 5);

	// The narrowest gradient (cliff): elevation jumps from 0 to full within 2 pixels
	constexpr float CliffGradWidth = 2.0f;

	// Cliff floor: in cliff zones, ALL land pixels start at this minimum elevation.
	// This creates an actual vertical cliff face (ocean=0, first land pixel=high).
	const float CliffFloor = FMath::Clamp(Settings.CliffFloorHeight, 0.0f, 1.0f);

	// Minimum and maximum cliff coverage guarantees
	const float MinCliffCoverage = FMath::Clamp(Settings.MinCliffCoverage, 0.0f, 1.0f);
	const float MaxCliffCoverage = FMath::Clamp(Settings.MaxCliffCoverage, 0.0f, 1.0f);

	// --- Pass 1: collect coastal variation noise for coastal pixels ---
	// We sample VarNoise for all coastal land pixels (CoastlineDistance <= 1.5)
	// to determine a bias that guarantees at least MinCliffCoverage of the
	// coastline becomes strong cliffs, and at most MaxCliffCoverage (so every
	// landmass retains accessible beaches).
	// BeachFactor = VarNoise * 0.5 + 0.5 + Bias.  Lower BeachFactor → more cliff.
	// We want the fraction of coastal pixels with BeachFactor < CliffBeachThreshold (0.35)
	// to be >= MinCliffCoverage and <= MaxCliffCoverage.

	float NoiseBias = 0.0f;

	if (CoastalVar > 0.0f && (MinCliffCoverage > 0.0f || MaxCliffCoverage < 1.0f))
	{
		TArray<float> CoastalNoiseValues;
		CoastalNoiseValues.Reserve(Resolution * 4); // rough estimate for coastal perimeter

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			// Coastal pixels: land pixels immediately adjacent to ocean
			if (LandMask[i] != 0 && CoastlineDistance[i] <= 1.5f)
			{
				const float NormX = static_cast<float>(i % Resolution) * InvRes;
				const float NormY = static_cast<float>(i / Resolution) * InvRes;
				const float VarNoise = WorldNoise::FBM(NormX * CoastalVarFreq, NormY * CoastalVarFreq, 2, 0.5f, CoastalVarSeed);
				CoastalNoiseValues.Add(VarNoise);
			}
		}

		if (CoastalNoiseValues.Num() > 0)
		{
			// Sort ascending: lower noise → lower BeachFactor → more cliff-like
			CoastalNoiseValues.Sort();

			// The BeachFactor threshold that separates cliff (CliffFactor > 0)
			// from beach (CliffFactor = 0).  Must match BeachCutoff in Pass 2.
			constexpr float CliffBeachThreshold = 0.35f;

			// --- Minimum cliff bias (shift toward more cliffs) ---
			float MinBias = 0.0f;
			if (MinCliffCoverage > 0.0f)
			{
				const int32 MinTargetIdx = FMath::Clamp(
					FMath::FloorToInt32(MinCliffCoverage * CoastalNoiseValues.Num()),
					0, CoastalNoiseValues.Num() - 1);
				// BeachFactor = Noise*0.5 + 0.5 + Bias = CliffBeachThreshold
				const float NoiseAtMin = CoastalNoiseValues[MinTargetIdx];
				MinBias = CliffBeachThreshold - NoiseAtMin * 0.5f - 0.5f;
				// Only apply negative bias (shift toward more cliffs)
				MinBias = FMath::Min(MinBias, 0.0f);
			}

			// --- Maximum cliff bias (shift toward more beaches) ---
			float MaxBias = 0.0f;
			if (MaxCliffCoverage < 1.0f)
			{
				const int32 MaxTargetIdx = FMath::Clamp(
					FMath::FloorToInt32(MaxCliffCoverage * CoastalNoiseValues.Num()),
					0, CoastalNoiseValues.Num() - 1);
				// We want at most MaxCliffCoverage fraction below threshold
				// So pixel at MaxTargetIdx should have BeachFactor = CliffBeachThreshold
				const float NoiseAtMax = CoastalNoiseValues[MaxTargetIdx];
				MaxBias = CliffBeachThreshold - NoiseAtMax * 0.5f - 0.5f;
				// Only apply positive bias (shift toward more beaches)
				MaxBias = FMath::Max(MaxBias, 0.0f);
			}

			// If both constraints apply, MaxBias (positive) wins over MinBias (negative)
			// because we must always guarantee at least some beach.
			// When only one constraint is active, the other is 0 and doesn't interfere.
			NoiseBias = (MaxBias > 0.0f) ? MaxBias : MinBias;
		}
	}

	// --- Pass 2: compute BaseElevation and CliffFactor with bias ---
	// BeachFactor threshold: above this → CliffFactor = 0 (true sea-level beach).
	// Must match CliffBeachThreshold used in Pass 1 bias computation.
	constexpr float BeachCutoff = 0.35f;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			BaseElevation[i] = 0.0f;
			CliffFactor[i] = 0.0f;
			continue;
		}

		const float NormX = static_cast<float>(i % Resolution) * InvRes;
		const float NormY = static_cast<float>(i / Resolution) * InvRes;

		// Per-pixel gradient width: noise determines cliff vs beach zones
		float LocalGradWidth = GradWidth;
		float LocalCliffFactor = 0.0f;
		if (CoastalVar > 0.0f)
		{
			// Sample low-frequency noise for this position, returns ~[-1, 1]
			const float VarNoise = WorldNoise::FBM(NormX * CoastalVarFreq, NormY * CoastalVarFreq, 2, 0.5f, CoastalVarSeed);
			// Map noise [-1,1] → beach factor [0,1]: 0 = cliff zone, 1 = beach zone
			// NoiseBias shifts the distribution to guarantee Min/MaxCliffCoverage
			const float BeachFactor = FMath::Clamp(VarNoise * 0.5f + 0.5f + NoiseBias, 0.0f, 1.0f);
			// The narrowest possible width at this CoastalVariation level:
			// CoastalVar=0 → MinWidth = GradWidth (no variation at all)
			// CoastalVar=1 → MinWidth = CliffGradWidth (full cliff)
			const float MinWidth = FMath::Lerp(GradWidth, CliffGradWidth, CoastalVar);
			// Lerp between cliff (MinWidth) and beach (GradWidth)
			LocalGradWidth = FMath::Lerp(MinWidth, GradWidth, BeachFactor);
			// Sharp cliff/beach cutoff: once BeachFactor exceeds BeachCutoff,
			// CliffFactor goes to exactly 0 → no cliff floor, no mountain/hill
			// coast bypass.  This ensures beaches are truly at sea level.
			// Below BeachCutoff, CliffFactor ramps linearly to 1.
			LocalCliffFactor = FMath::Clamp(1.0f - BeachFactor / BeachCutoff, 0.0f, 1.0f);
		}
		CliffFactor[i] = LocalCliffFactor;

		// Coastal transition: smoothstep for natural falloff instead of linear
		const float T = FMath::Clamp(CoastlineDistance[i] / LocalGradWidth, 0.0f, 1.0f);
		const float CoastFade = T * T * (3.0f - 2.0f * T);

		// In cliff zones, enforce a minimum elevation for ANY land pixel.
		// This creates the dramatic step: ocean=0, first land pixel=high.
		const float EffectiveCoastFade = FMath::Max(CoastFade, LocalCliffFactor * CliffFloor);

		// Modulate with FBM noise
		const float Noise = WorldNoise::FBM(NormX * Freq, NormY * Freq, Octaves, Persist, Seed);

		// Remap FBM [-1,1] → [0,1], then clamp to [0.2,1.0] so coasts never
		// collapse to zero (0.2 floor) while inland areas get full modulation.
		const float NoiseMod = FMath::Clamp(0.5f + 0.5f * Noise, 0.2f, 1.0f);

		BaseElevation[i] = EffectiveCoastFade * NoiseMod;
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – BaseElevation generated (gradient width %d, coastal variation %.2f, cliff bias %.3f)"),
		Settings.CoastlineGradientWidth, Settings.CoastalVariation, NoiseBias);
}

//------------------------------------------------------------------------------
// GenerateUpliftMap:
//   Ridged FBM for mountain ranges.  Mountains are placed by noise alone —
//   NOT confined to the island center.  Only a thin coastal margin prevents
//   ridges from literally touching the ocean.  MountainCoverage controls the
//   width of that margin: high values → mountains right at the coast (cliffs);
//   low values → mountains confined farther inland.
//------------------------------------------------------------------------------
void UUpliftGenerator::GenerateUpliftMap(const TArray<uint8>& LandMask)
{
	const int32 TotalPixels = Resolution * Resolution;
	const float Freq       = Settings.MountainRidgeFrequency;
	const float Amplitude  = Settings.MountainRidgeAmplitude;
	const float Sharpness  = Settings.MountainSharpness;
	const int32 Octaves    = Settings.MountainOctaves;

	// MountainCoverage → coastal margin in pixels.
	// Higher coverage → thinner margin → mountains extend closer to coast.
	const float CoverageT = FMath::Clamp(
		(Settings.MountainCoverage - 0.1f) / 1.9f, 0.0f, 1.0f);
	const float MountainCoastMargin = FMath::Lerp(40.0f, 1.0f, CoverageT);

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

		// Thin coastal margin: smoothstep fade within MountainCoastMargin pixels
		// of the shore.  Beyond the margin, mountains are at full strength.
		// In cliff zones, bypass the margin so mountains contribute at the cliff edge.
		const float D = FMath::Clamp(CoastlineDistance[i] / MountainCoastMargin, 0.0f, 1.0f);
		const float MarginFade = D * D * (3.0f - 2.0f * D);
		const float CoastFade = FMath::Max(MarginFade, CliffFactor[i]);
		UpliftMap[i] = Ridge * CoastFade;
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – UpliftMap generated (freq %.2f, amp %.2f, coverage %.2f, margin %.0f px)"),
		Freq, Amplitude, Settings.MountainCoverage, MountainCoastMargin);
}

//------------------------------------------------------------------------------
// GenerateVolcanicHotspots:
//   Smooth shield-shaped volcanoes with rounded craters, blended into
//   UpliftMap.  Uses cosine falloff for the cone and a parabolic bowl
//   for the crater so the result looks like a real volcanic landform.
//   Underlying terrain noise is suppressed inside the volcano footprint
//   via a replace-blend so mountain ridges don't poke through the cone.
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

				// Smooth shield-volcano profile: cosine falloff gives a
				// rounded dome instead of a sharp linear cone.
				float Height = PeakHeight * 0.5f * (1.0f + FMath::Cos(PI * NormDist));

				// Rounded crater bowl: parabolic subtraction inside the
				// crater radius produces a smooth U-shaped depression.
				if (NormDist < CraterFraction && CraterFraction > 0.0f)
				{
					const float CraterNorm = NormDist / CraterFraction; // 0 at center, 1 at rim
					Height -= CraterDepth * PeakHeight * (1.0f - CraterNorm * CraterNorm);
				}

				Height = FMath::Max(Height, 0.0f);

				// Replace-blend: suppress underlying terrain noise inside
				// the volcano so mountain ridges don't poke through.
				// Near the centre the volcano fully replaces the base;
				// at the outer edge it blends additively with existing terrain.
				const float BlendAlpha = NormDist * NormDist; // 0 at centre, 1 at edge
				const float Existing = UpliftMap[Idx];
				UpliftMap[Idx] = FMath::Clamp(
					FMath::Lerp(Height, Existing + Height, BlendAlpha),
					0.0f, 1.0f);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UpliftGenerator – %d volcanic hotspots placed"), VolcanicCenters.Num());
}

//------------------------------------------------------------------------------
// GenerateHills:
//   FBM-based rolling hills, added to UpliftMap.
//   Hills are smaller-scale undulations distinct from mountain ridges.
//   They fade within a thin coastal margin (using coastline distance) so
//   they can appear anywhere on land, not only in the high-elevation centre.
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

	// Hills use a slightly wider coast margin than mountains (gentler fade).
	// Derived from MountainCoverage: higher coverage → hills also extend closer.
	const float CoverageT = FMath::Clamp(
		(Settings.MountainCoverage - 0.1f) / 1.9f, 0.0f, 1.0f);
	const float HillCoastMargin = FMath::Lerp(30.0f, 3.0f, CoverageT);

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

		// Thin coastal fade so hills can appear anywhere except right at shore.
		// In cliff zones, bypass the margin so hills contribute at the cliff edge.
		const float D = FMath::Clamp(CoastlineDistance[i] / HillCoastMargin, 0.0f, 1.0f);
		const float MarginFade = D * D * (3.0f - 2.0f * D);
		const float CoastFade = FMath::Max(MarginFade, CliffFactor[i]);

		UpliftMap[i] += Hill * CoastFade;
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
