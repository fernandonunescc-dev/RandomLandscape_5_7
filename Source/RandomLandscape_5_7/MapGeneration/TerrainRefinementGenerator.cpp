// TerrainRefinementGenerator.cpp
// Stage 7: Terrain Refinement — biome-local detail noise on top of macro terrain.
// Adds biome-specific surface detail and applies final smoothing.

#include "TerrainRefinementGenerator.h"
#include "NoiseUtility.h"

//------------------------------------------------------------------------------
// Initialize: store settings, derive seed, record resolution
//------------------------------------------------------------------------------
void UTerrainRefinementGenerator::Initialize(const FTerrainRefinementSettings& InSettings,
	int32 GlobalSeed, int32 TextureResolution)
{
	Settings = InSettings;

	ActualSeed = (Settings.Seed != 0)
		? Settings.Seed
		: WorldNoise::DeriveSeed(GlobalSeed, 7);

	Resolution = TextureResolution;

	UE_LOG(LogTemp, Log,
		TEXT("TerrainRefinementGenerator initialized – Resolution: %d, Seed: %d, DetailScale: %.3f, SmoothingPasses: %d"),
		Resolution, ActualSeed, Settings.DetailScale, Settings.FinalSmoothingPasses);
}

//------------------------------------------------------------------------------
// Generate: add biome detail noise to eroded elevation, then smooth
//------------------------------------------------------------------------------
bool UTerrainRefinementGenerator::Generate(const TArray<float>& ErodedElevation,
	const TArray<int32>& BiomeMap, const TArray<uint8>& LandMask)
{
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("TerrainRefinementGenerator::Generate – Resolution not set. Call Initialize first."));
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;

	if (ErodedElevation.Num() != TotalPixels || BiomeMap.Num() != TotalPixels
		|| LandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error,
			TEXT("TerrainRefinementGenerator::Generate – Input array size mismatch (expected %d)"), TotalPixels);
		return false;
	}

	// Step 1: copy the eroded elevation as our starting point
	FinalElevation = ErodedElevation;

	UE_LOG(LogTemp, Log, TEXT("TerrainRefinementGenerator::Generate – starting (%d pixels)"), TotalPixels);

	// Step 2: add biome-specific detail noise to each land pixel
	//         Also blend in mountain ridged-detail for high-elevation pixels so
	//         peaks retain craggy micro-detail even though biome is climate-driven.
	const float InvRes = 1.0f / static_cast<float>(Resolution);
	const float MountainDetailBlendStart = 0.55f;   // start blending at 55% elevation
	const float MountainDetailBlendEnd   = 0.75f;   // full mountain detail at 75% elevation
	const float MountainDetailBlendRange = MountainDetailBlendEnd - MountainDetailBlendStart;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (LandMask[i] == 0)
		{
			continue;
		}

		const EBiomeType Biome = static_cast<EBiomeType>(BiomeMap[i]);
		const int32 X = i % Resolution;
		const int32 Y = i / Resolution;
		const float NormX = static_cast<float>(X) * InvRes;
		const float NormY = static_cast<float>(Y) * InvRes;

		float Detail = ComputeBiomeDetail(NormX, NormY, Biome);

		// Blend in mountain ridged-detail based on elevation so high-altitude
		// terrain gets craggy micro-detail regardless of its climate biome.
		if (Biome != EBiomeType::Mountain && ErodedElevation[i] > MountainDetailBlendStart)
		{
			const float MountainDetail = ComputeBiomeDetail(NormX, NormY, EBiomeType::Mountain);
			const float Blend = FMath::Clamp(
				(ErodedElevation[i] - MountainDetailBlendStart) / MountainDetailBlendRange,
				0.0f, 1.0f);
			Detail = FMath::Lerp(Detail, MountainDetail, Blend);
		}

		FinalElevation[i] = FMath::Clamp(FinalElevation[i] + Detail, 0.0f, 1.0f);
	}

	// Step 3: smooth final elevation on land
	ApplySmoothing(LandMask);

	UE_LOG(LogTemp, Log, TEXT("TerrainRefinementGenerator::Generate – complete"));
	return true;
}

//------------------------------------------------------------------------------
// ComputeBiomeDetail:
//   Switch on biome type and return a noise value scaled by DetailScale.
//   Each biome uses a decorrelated seed (biome type * 137 + ActualSeed).
//------------------------------------------------------------------------------
float UTerrainRefinementGenerator::ComputeBiomeDetail(float NormX, float NormY, EBiomeType Biome) const
{
	const int32 BiomeSeed = static_cast<int32>(Biome) * 137 + ActualSeed;
	const float BaseFreq = Settings.DetailFrequency;
	const int32 BaseOct = Settings.DetailOctaves;
	const float DS = Settings.DetailScale;
	const int32 BiomeIdx = static_cast<int32>(Biome);

	// Check if per-biome profiles are configured
	const bool bHasProfile = (BiomeIdx >= 0 && BiomeIdx < Settings.BiomeProfiles.Num());

	if (bHasProfile)
	{
		// --- Data-driven per-biome profile ---
		const FPerBiomeTerrainProfile& P = Settings.BiomeProfiles[BiomeIdx];
		const float Freq = BaseFreq * P.FrequencyMultiplier;
		const int32 Oct = BaseOct + P.ExtraOctaves;
		const float Scale = DS * P.AmplitudeMultiplier;
		const float SX = NormX * Freq;
		const float SY = NormY * Freq;

		if (P.bUseRidgedNoise)
		{
			return WorldNoise::RidgedFBM(SX, SY, Oct, 0.5f, P.RidgedSharpness, BiomeSeed) * Scale;
		}
		else
		{
			return WorldNoise::FBM(SX, SY, Oct, 0.5f, BiomeSeed) * Scale;
		}
	}

	// --- Fallback: original hard-coded per-biome noise ---
	const float Freq = BaseFreq;
	const int32 Oct = BaseOct;
	const float SX = NormX * Freq;
	const float SY = NormY * Freq;

	switch (Biome)
	{
	case EBiomeType::Ocean:
		return 0.0f;

	case EBiomeType::Land:
	{
		// Very gentle rolling hills
		const float N = WorldNoise::FBM(SX, SY, Oct, 0.5f, BiomeSeed);
		return N * 0.3f * DS;
	}

	case EBiomeType::Forest:
	{
		// Moderate undulation with an extra octave for uneven ground
		const float N = WorldNoise::FBM(SX, SY, Oct + 1, 0.5f, BiomeSeed);
		return N * 0.5f * DS;
	}

	case EBiomeType::Desert:
	{
		// Dune pattern: sin wave modulated by noise for irregularity
		const float NoiseOffset = WorldNoise::Noise2D(SX * 0.5f, SY * 0.5f, BiomeSeed);
		const float Dune = FMath::Sin(NormX * Settings.DuneFrequency + NoiseOffset);
		return Dune * Settings.DuneHeight * DS;
	}

	case EBiomeType::Snow:
	{
		// Moderate FBM similar to forest
		const float N = WorldNoise::FBM(SX, SY, Oct, 0.5f, BiomeSeed);
		return N * 0.5f * DS;
	}

	case EBiomeType::Ice:
	{
		// Very subtle flat noise
		const float N = WorldNoise::Noise2D(SX, SY, BiomeSeed);
		return N * 0.1f * DS;
	}

	case EBiomeType::Mountain:
	{
		// Ridged FBM for sharper peaks
		const float N = WorldNoise::RidgedFBM(SX, SY, Oct, 0.5f, 2.0f, BiomeSeed);
		return N * Settings.MountainDetailScale;
	}

	case EBiomeType::Volcanic:
	{
		// Gentle noise — volcano shape already comes from uplift
		const float N = WorldNoise::FBM(SX, SY, Oct, 0.5f, BiomeSeed);
		return N * 0.3f * DS;
	}

	default:
		return 0.0f;
	}
}

//------------------------------------------------------------------------------
// ApplySmoothing:
//   3×3 box blur for FinalSmoothingPasses iterations on land pixels only.
//   Uses a double-buffered swap to avoid order-dependent artifacts.
//   Ocean pixels stay at 0.
//------------------------------------------------------------------------------
void UTerrainRefinementGenerator::ApplySmoothing(const TArray<uint8>& LandMask)
{
	const int32 Passes = Settings.FinalSmoothingPasses;
	if (Passes <= 0)
	{
		return;
	}

	const int32 TotalPixels = Resolution * Resolution;

	TArray<float> TempBuffer;
	TempBuffer.SetNumZeroed(TotalPixels);

	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		for (int32 Y = 0; Y < Resolution; ++Y)
		{
			for (int32 X = 0; X < Resolution; ++X)
			{
				const int32 Idx = Y * Resolution + X;

				// Ocean pixels stay at 0
				if (LandMask[Idx] == 0)
				{
					TempBuffer[Idx] = 0.0f;
					continue;
				}

				float Sum = 0.0f;
				float Count = 0.0f;

				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 NX = X + DX;
						const int32 NY = Y + DY;
						if (NX >= 0 && NX < Resolution && NY >= 0 && NY < Resolution)
						{
							Sum += FinalElevation[NY * Resolution + NX];
							Count += 1.0f;
						}
					}
				}

				TempBuffer[Idx] = Sum / Count;
			}
		}

		// Swap buffers for next pass
		Swap(FinalElevation, TempBuffer);
	}

	UE_LOG(LogTemp, Log,
		TEXT("TerrainRefinementGenerator – Smoothing complete (%d passes)"), Passes);
}
