// WorldGenerationActor.cpp
// Geology-driven world generation pipeline orchestrator

#include "WorldGenerationActor.h"
#include "UpliftGenerator.h"
#include "HydrologyGenerator.h"
#include "ErosionGenerator.h"
#include "ClimateGenerator.h"
#include "BiomeAssignmentGenerator.h"
#include "TerrainRefinementGenerator.h"
#include "TerrainValidator.h"
#include "Engine/Texture2D.h"

AWorldGenerationActor::AWorldGenerationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	RootComponent = TerrainMesh;
	TerrainMesh->bUseComplexAsSimpleCollision = true;
}

// ------------------------------------------------------------
// Sync global settings into landmass settings
// ------------------------------------------------------------
void AWorldGenerationActor::SyncLandmassSettings()
{
	LandmassSettings.Seed = GlobalSeed;
	LandmassSettings.TextureResolution = TextureResolution;
	LandmassSettings.LandmassType = LandmassType;
	LandmassSettings.TargetLandAreaSqKm = TargetLandAreaSqKm;
	LandmassSettings.OceanPaddingMeters = OceanPaddingMeters;
}

// ------------------------------------------------------------
// Stage 1: Landmass
// ------------------------------------------------------------
void AWorldGenerationActor::Step1_GenerateLandmass()
{
	SyncLandmassSettings();

	ULandmassGenerator* Generator = NewObject<ULandmassGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create ULandmassGenerator"));
		return;
	}

	Generator->Initialize(LandmassSettings);

	if (!Generator->Generate())
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Landmass generation failed"));
		return;
	}

	CachedLandMask = Generator->GetLandMask();
	ActualSeedUsed = Generator->GetSeed();
	ResolutionUsed = TextureResolution;
	CachedWorldSizeCm = Generator->GetComputedWorldSizeCm();
	FinalMapSizeMeters = CachedWorldSizeCm / 100.0f;
	Debug_Landmass = Generator->GetPreviewTexture();

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Stage 1 Landmass complete - Seed: %d, Resolution: %d, LandPixels: %d"),
		ActualSeedUsed, ResolutionUsed, CachedLandMask.Num());
}

// ------------------------------------------------------------
// Stage 2: Uplift / Geology
// ------------------------------------------------------------
void AWorldGenerationActor::Step2_GenerateUplift()
{
	if (CachedLandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run Step1_GenerateLandmass first"));
		return;
	}

	UUpliftGenerator* Generator = NewObject<UUpliftGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create UUpliftGenerator"));
		return;
	}

	Generator->Initialize(UpliftSettings, GlobalSeed, TextureResolution);

	if (!Generator->Generate(CachedLandMask))
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Uplift generation failed"));
		return;
	}

	CachedBaseElevation = Generator->GetBaseElevation();
	CachedUpliftMap = Generator->GetUpliftMap();
	CachedCombinedElevation = Generator->GetCombinedElevation();
	CachedVolcanicCenters = Generator->GetVolcanicCenters();
	CachedPlateauMap = Generator->GetPlateauMap();

	Debug_BaseElevation = CreateGrayscaleDebugTexture(TextureResolution, CachedBaseElevation);
	Debug_UpliftMap = CreateGrayscaleDebugTexture(TextureResolution, CachedUpliftMap);
	Debug_CombinedElevation = CreateGrayscaleDebugTexture(TextureResolution, CachedCombinedElevation);
	Debug_PlateauMap = CreateGrayscaleDebugTexture(TextureResolution, CachedPlateauMap);

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Stage 2 Uplift complete - VolcanicCenters: %d"),
		CachedVolcanicCenters.Num());
}

// ------------------------------------------------------------
// Stage 3: Hydrology
// ------------------------------------------------------------
void AWorldGenerationActor::Step3_GenerateHydrology()
{
	if (CachedCombinedElevation.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run Step2_GenerateUplift first"));
		return;
	}

	UHydrologyGenerator* Generator = NewObject<UHydrologyGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create UHydrologyGenerator"));
		return;
	}

	Generator->Initialize(HydrologySettings, GlobalSeed, TextureResolution);

	if (!Generator->Generate(CachedCombinedElevation, CachedLandMask))
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Hydrology generation failed"));
		return;
	}

	CachedRiverMap = Generator->GetRiverMap();
	CachedLakeMap = Generator->GetLakeMap();
	CachedWaterfallMap = Generator->GetWaterfallMap();
	CachedFlowAccumulation = Generator->GetFlowAccumulation();
	CachedFlowDirection = Generator->GetFlowDirection();

	// Debug_Rivers: blue where river, grayscale elevation otherwise
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> RiverPixels;
		RiverPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (CachedRiverMap[i] > 0.0f)
			{
				const uint8 Intensity = static_cast<uint8>(FMath::Clamp(CachedRiverMap[i] * 255.0f, 50.0f, 255.0f));
				RiverPixels[i] = FColor(0, 0, Intensity, 255);
			}
			else
			{
				const uint8 Gray = static_cast<uint8>(FMath::Clamp(CachedCombinedElevation[i] * 255.0f, 0.0f, 255.0f));
				RiverPixels[i] = FColor(Gray, Gray, Gray, 255);
			}
		}

		Debug_Rivers = CreateColorDebugTexture(TextureResolution, RiverPixels);
	}

	// Debug_Lakes: cyan where lake
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> LakePixels;
		LakePixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (CachedLakeMap[i] == 1)
			{
				LakePixels[i] = FColor(0, 200, 220, 255);
			}
			else
			{
				const uint8 Gray = static_cast<uint8>(FMath::Clamp(CachedCombinedElevation[i] * 255.0f, 0.0f, 255.0f));
				LakePixels[i] = FColor(Gray, Gray, Gray, 255);
			}
		}

		Debug_Lakes = CreateColorDebugTexture(TextureResolution, LakePixels);
	}

	// Debug_Waterfalls: magenta where waterfall, grayscale elevation otherwise
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> WaterfallPixels;
		WaterfallPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (CachedWaterfallMap[i] == 1)
			{
				WaterfallPixels[i] = FColor(230, 50, 230, 255);
			}
			else if (CachedRiverMap[i] > 0.0f)
			{
				const uint8 Intensity = static_cast<uint8>(FMath::Clamp(CachedRiverMap[i] * 255.0f, 50.0f, 255.0f));
				WaterfallPixels[i] = FColor(0, 0, Intensity, 255);
			}
			else
			{
				const uint8 Gray = static_cast<uint8>(FMath::Clamp(CachedCombinedElevation[i] * 255.0f, 0.0f, 255.0f));
				WaterfallPixels[i] = FColor(Gray, Gray, Gray, 255);
			}
		}

		Debug_Waterfalls = CreateColorDebugTexture(TextureResolution, WaterfallPixels);
	}

	// Debug_FlowAccumulation: log-normalized grayscale
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		float MaxAccum = 1.0f;
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (CachedFlowAccumulation[i] > MaxAccum)
			{
				MaxAccum = CachedFlowAccumulation[i];
			}
		}

		const float LogMax = FMath::Loge(MaxAccum);
		TArray<float> LogNormAccum;
		LogNormAccum.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (CachedFlowAccumulation[i] > 1.0f && LogMax > 0.0f)
			{
				LogNormAccum[i] = FMath::Clamp(FMath::Loge(CachedFlowAccumulation[i]) / LogMax, 0.0f, 1.0f);
			}
			else
			{
				LogNormAccum[i] = 0.0f;
			}
		}

		Debug_FlowAccumulation = CreateGrayscaleDebugTexture(TextureResolution, LogNormAccum);
	}

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Stage 3 Hydrology complete"));
}

// ------------------------------------------------------------
// Stage 4: Erosion
// ------------------------------------------------------------
void AWorldGenerationActor::Step4_GenerateErosion()
{
	if (CachedCombinedElevation.Num() == 0 || CachedRiverMap.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run Step3_GenerateHydrology first"));
		return;
	}

	UErosionGenerator* Generator = NewObject<UErosionGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create UErosionGenerator"));
		return;
	}

	Generator->Initialize(ErosionSettings, GlobalSeed, TextureResolution);

	if (!Generator->Generate(CachedCombinedElevation, CachedRiverMap, CachedUpliftMap, CachedLandMask))
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Erosion generation failed"));
		return;
	}

	CachedErodedElevation = Generator->GetErodedElevation();
	CachedCanyonMask = Generator->GetCanyonMask();             // Binary: 1 = carved by river incision
	CachedErosionDeltaMap = Generator->GetErosionDeltaMap();   // Signed: positive = material removed

	Debug_ErodedElevation = CreateGrayscaleDebugTexture(TextureResolution, CachedErodedElevation);

	// Debug_CanyonMask: red where canyon, grayscale elevation otherwise
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> CanyonPixels;
		CanyonPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			if (CachedCanyonMask[i] == 1)
			{
				CanyonPixels[i] = FColor(200, 80, 40, 255);
			}
			else
			{
				const uint8 Gray = static_cast<uint8>(FMath::Clamp(CachedErodedElevation[i] * 255.0f, 0.0f, 255.0f));
				CanyonPixels[i] = FColor(Gray, Gray, Gray, 255);
			}
		}

		Debug_CanyonMask = CreateColorDebugTexture(TextureResolution, CanyonPixels);
	}

	// Debug_ErosionDelta: green = removed, blue = deposited, black = no change
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;

		// Find max absolute delta for normalization
		float MaxAbsDelta = 0.0f;
		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const float AbsDelta = FMath::Abs(CachedErosionDeltaMap[i]);
			if (AbsDelta > MaxAbsDelta) MaxAbsDelta = AbsDelta;
		}

		TArray<FColor> DeltaPixels;
		DeltaPixels.SetNumUninitialized(TotalPixels);

		const float InvMax = (MaxAbsDelta > 0.0f) ? (1.0f / MaxAbsDelta) : 0.0f;

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const float NormDelta = CachedErosionDeltaMap[i] * InvMax;
			if (NormDelta > 0.01f)
			{
				// Material removed — green
				const uint8 G = static_cast<uint8>(FMath::Clamp(NormDelta * 255.0f, 0.0f, 255.0f));
				DeltaPixels[i] = FColor(0, G, 0, 255);
			}
			else if (NormDelta < -0.01f)
			{
				// Material deposited — blue
				const uint8 B = static_cast<uint8>(FMath::Clamp(-NormDelta * 255.0f, 0.0f, 255.0f));
				DeltaPixels[i] = FColor(0, 0, B, 255);
			}
			else
			{
				DeltaPixels[i] = FColor(0, 0, 0, 255);
			}
		}

		Debug_ErosionDelta = CreateColorDebugTexture(TextureResolution, DeltaPixels);
	}

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Stage 4 Erosion complete"));
}

// ------------------------------------------------------------
// Stage 5: Climate
// ------------------------------------------------------------
void AWorldGenerationActor::Step5_GenerateClimate()
{
	if (CachedErodedElevation.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run Step4_GenerateErosion first"));
		return;
	}

	UClimateGenerator* Generator = NewObject<UClimateGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create UClimateGenerator"));
		return;
	}

	Generator->Initialize(ClimateSettings, GlobalSeed, TextureResolution);

	if (!Generator->Generate(CachedErodedElevation, CachedLandMask, CachedRiverMap, CachedLakeMap))
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Climate generation failed"));
		return;
	}

	CachedTemperature = Generator->GetTemperature();
	CachedMoisture = Generator->GetMoisture();
	CachedPrecipitation = Generator->GetPrecipitation();

	const int32 TotalPixels = TextureResolution * TextureResolution;

	// Debug_Temperature: heat colormap (blue → cyan → yellow → orange → red)
	{
		TArray<FColor> TempPixels;
		TempPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const float T = FMath::Clamp(CachedTemperature[i], 0.0f, 1.0f);
			FLinearColor C;

			if (T < 0.25f)
			{
				const float A = T / 0.25f;
				C = FMath::Lerp(FLinearColor(0.0f, 0.0f, 1.0f), FLinearColor(0.0f, 1.0f, 1.0f), A);
			}
			else if (T < 0.5f)
			{
				const float A = (T - 0.25f) / 0.25f;
				C = FMath::Lerp(FLinearColor(0.0f, 1.0f, 1.0f), FLinearColor(1.0f, 1.0f, 0.0f), A);
			}
			else if (T < 0.75f)
			{
				const float A = (T - 0.5f) / 0.25f;
				C = FMath::Lerp(FLinearColor(1.0f, 1.0f, 0.0f), FLinearColor(1.0f, 0.5f, 0.0f), A);
			}
			else
			{
				const float A = (T - 0.75f) / 0.25f;
				C = FMath::Lerp(FLinearColor(1.0f, 0.5f, 0.0f), FLinearColor(1.0f, 0.0f, 0.0f), A);
			}

			TempPixels[i] = C.ToFColor(false);
		}

		Debug_Temperature = CreateColorDebugTexture(TextureResolution, TempPixels);
	}

	// Debug_Moisture: white (dry) → cyan → blue (wet)
	{
		TArray<FColor> MoistPixels;
		MoistPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const float M = FMath::Clamp(CachedMoisture[i], 0.0f, 1.0f);
			FLinearColor C;

			if (M < 0.5f)
			{
				const float A = M / 0.5f;
				C = FMath::Lerp(FLinearColor(1.0f, 1.0f, 1.0f), FLinearColor(0.0f, 1.0f, 1.0f), A);
			}
			else
			{
				const float A = (M - 0.5f) / 0.5f;
				C = FMath::Lerp(FLinearColor(0.0f, 1.0f, 1.0f), FLinearColor(0.0f, 0.0f, 1.0f), A);
			}

			MoistPixels[i] = C.ToFColor(false);
		}

		Debug_Moisture = CreateColorDebugTexture(TextureResolution, MoistPixels);
	}

	// Debug_Precipitation: white (none) → light green → dark green (heavy)
	{
		TArray<FColor> PrecipPixels;
		PrecipPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const float P = FMath::Clamp(CachedPrecipitation[i], 0.0f, 1.0f);
			FLinearColor C;

			if (P < 0.5f)
			{
				const float A = P / 0.5f;
				C = FMath::Lerp(FLinearColor(1.0f, 1.0f, 1.0f), FLinearColor(0.5f, 0.9f, 0.3f), A);
			}
			else
			{
				const float A = (P - 0.5f) / 0.5f;
				C = FMath::Lerp(FLinearColor(0.5f, 0.9f, 0.3f), FLinearColor(0.0f, 0.3f, 0.0f), A);
			}

			PrecipPixels[i] = C.ToFColor(false);
		}

		Debug_Precipitation = CreateColorDebugTexture(TextureResolution, PrecipPixels);
	}

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Stage 5 Climate complete"));
}

// ------------------------------------------------------------
// Stage 6: Biomes
// ------------------------------------------------------------
void AWorldGenerationActor::Step6_GenerateBiomes()
{
	if (CachedTemperature.Num() == 0 || CachedMoisture.Num() == 0 || CachedErodedElevation.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run Step5_GenerateClimate first"));
		return;
	}

	UBiomeAssignmentGenerator* Generator = NewObject<UBiomeAssignmentGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create UBiomeAssignmentGenerator"));
		return;
	}

	// Sync top-level Quick Presets → nested struct
	BiomeAssignmentSettings.TargetSnowPercent = TargetSnowPercent;
	BiomeAssignmentSettings.TargetDesertPercent = TargetDesertPercent;
	BiomeAssignmentSettings.TargetForestPercent = TargetForestPercent;

	Generator->Initialize(BiomeAssignmentSettings, GlobalSeed, TextureResolution);

	if (!Generator->Generate(CachedErodedElevation, CachedTemperature, CachedMoisture,
		CachedPrecipitation, CachedLandMask, CachedRiverMap, CachedLakeMap, CachedVolcanicCenters,
		CachedPlateauMap, CachedCanyonMask, CachedWaterfallMap, SeaLevel))
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Biome assignment failed"));
		return;
	}

	CachedBiomeMap = Generator->GetBiomeMap();
	CachedTerrainArchetypeMap = Generator->GetTerrainArchetypeMap();
	CachedSurfaceOverlayMap = Generator->GetSurfaceOverlayMap();
	CachedGeneratedFeatureMap = Generator->GetGeneratedFeatureMap();
	CachedSlopeMap = Generator->GetSlopeMap();
	CachedBiomeBlendWeights = Generator->GetBiomeBlendWeights();

	// Debug_BiomeMap: color each pixel by biome type
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> BiomePixels;
		BiomePixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const EBiomeType BiomeType = static_cast<EBiomeType>(CachedBiomeMap[i]);
			const FLinearColor BiomeColor = GetBiomeDebugColor(BiomeType);
			BiomePixels[i] = BiomeColor.ToFColor(false);
		}

		Debug_BiomeMap = CreateColorDebugTexture(TextureResolution, BiomePixels);
	}

	// Debug_SlopeMap: grayscale slope
	Debug_SlopeMap = CreateGrayscaleDebugTexture(TextureResolution, CachedSlopeMap);

	// Debug_BiomeBlendWeights: visualise dominant-biome purity (white=pure, gray=blended)
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> BlendPixels;
		BlendPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			// Find max weight for this pixel (blend purity)
			float MaxW = 0.0f;
			int32 DomBiome = 0;
			const int32 Base = i * 8;
			for (int32 b = 0; b < 8; ++b)
			{
				if (CachedBiomeBlendWeights[Base + b] > MaxW)
				{
					MaxW = CachedBiomeBlendWeights[Base + b];
					DomBiome = b;
				}
			}

			// Tint the dominant biome color by purity
			const FLinearColor BiomeColor = GetBiomeDebugColor(static_cast<EBiomeType>(DomBiome));
			const FLinearColor Blended = FMath::Lerp(FLinearColor(0.5f, 0.5f, 0.5f), BiomeColor, MaxW);
			BlendPixels[i] = Blended.ToFColor(false);
		}

		Debug_BiomeBlendWeights = CreateColorDebugTexture(TextureResolution, BlendPixels);
	}

	// Debug_TerrainArchetypeMap: color each pixel by terrain archetype
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> ArchetypePixels;
		ArchetypePixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const ETerrainArchetype Archetype = static_cast<ETerrainArchetype>(CachedTerrainArchetypeMap[i]);
			const FLinearColor ArchetypeColor = GetArchetypeDebugColor(Archetype);
			ArchetypePixels[i] = ArchetypeColor.ToFColor(false);
		}

		Debug_TerrainArchetypeMap = CreateColorDebugTexture(TextureResolution, ArchetypePixels);
	}

	// Debug_SurfaceOverlayMap: color each pixel by surface overlay
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> OverlayPixels;
		OverlayPixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const ESurfaceOverlay Overlay = static_cast<ESurfaceOverlay>(CachedSurfaceOverlayMap[i]);
			const FLinearColor OverlayColor = GetSurfaceOverlayDebugColor(Overlay);
			OverlayPixels[i] = OverlayColor.ToFColor(false);
		}

		Debug_SurfaceOverlayMap = CreateColorDebugTexture(TextureResolution, OverlayPixels);
	}

	// Debug_GeneratedFeatureMap: highlight water features
	{
		const int32 TotalPixels = TextureResolution * TextureResolution;
		TArray<FColor> FeaturePixels;
		FeaturePixels.SetNumUninitialized(TotalPixels);

		for (int32 i = 0; i < TotalPixels; ++i)
		{
			const EGeneratedFeature Feature = static_cast<EGeneratedFeature>(CachedGeneratedFeatureMap[i]);
			switch (Feature)
			{
			case EGeneratedFeature::River:     FeaturePixels[i] = FColor(30, 80, 200, 255); break;
			case EGeneratedFeature::Lake:      FeaturePixels[i] = FColor(20, 60, 160, 255); break;
			case EGeneratedFeature::Waterfall: FeaturePixels[i] = FColor(100, 180, 255, 255); break;
			default:                           FeaturePixels[i] = FColor(0, 0, 0, 255); break;
			}
		}

		Debug_GeneratedFeatureMap = CreateColorDebugTexture(TextureResolution, FeaturePixels);
	}

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Stage 6 Biomes complete"));
}

// ------------------------------------------------------------
// Stage 7: Terrain Refinement
// ------------------------------------------------------------
void AWorldGenerationActor::Step7_GenerateRefinement()
{
	if (CachedErodedElevation.Num() == 0 || CachedBiomeMap.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run Step6_GenerateBiomes first"));
		return;
	}

	UTerrainRefinementGenerator* Generator = NewObject<UTerrainRefinementGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create UTerrainRefinementGenerator"));
		return;
	}

	Generator->Initialize(RefinementSettings, GlobalSeed, TextureResolution);

	if (!Generator->Generate(CachedErodedElevation, CachedBiomeMap, CachedLandMask))
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Terrain refinement failed"));
		return;
	}

	CachedFinalElevation = Generator->GetFinalElevation();
	Debug_FinalElevation = CreateGrayscaleDebugTexture(TextureResolution, CachedFinalElevation);

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Stage 7 Refinement complete"));
}

// ------------------------------------------------------------
// Generate All Stages
// ------------------------------------------------------------
void AWorldGenerationActor::GenerateAll()
{
	Step1_GenerateLandmass();
	if (CachedLandMask.Num() == 0) return;

	Step2_GenerateUplift();
	if (CachedCombinedElevation.Num() == 0) return;

	Step3_GenerateHydrology();
	if (CachedRiverMap.Num() == 0) return;

	Step4_GenerateErosion();
	if (CachedErodedElevation.Num() == 0) return;

	Step5_GenerateClimate();
	if (CachedTemperature.Num() == 0) return;

	Step6_GenerateBiomes();
	if (CachedBiomeMap.Num() == 0) return;

	Step7_GenerateRefinement();
}

// ------------------------------------------------------------
// Randomize: new random seed → full pipeline → mesh
// ------------------------------------------------------------
void AWorldGenerationActor::Randomize()
{
	GlobalSeed = FMath::RandRange(1, 0x7FFFFFFF);

	// Randomise target land area for varied island sizes
	TargetLandAreaSqKm = FMath::FRandRange(0.1f, 1.0f);

	// Randomise biome target percentages
	TargetSnowPercent = FMath::FRandRange(2.0f, 20.0f);
	TargetDesertPercent = FMath::FRandRange(5.0f, 30.0f);
	TargetForestPercent = FMath::FRandRange(10.0f, 45.0f);

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor::Randomize - New seed: %d, TargetLandArea: %.3f sq km, Snow: %.1f%%, Desert: %.1f%%, Forest: %.1f%%"),
		GlobalSeed, TargetLandAreaSqKm, TargetSnowPercent, TargetDesertPercent, TargetForestPercent);

	GenerateAll();
	GenerateMesh();
}

// ------------------------------------------------------------
// RandomizeSeed: new random seed only → full pipeline → mesh
// ------------------------------------------------------------
void AWorldGenerationActor::RandomizeSeed()
{
	GlobalSeed = FMath::RandRange(1, 0x7FFFFFFF);

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor::RandomizeSeed - New seed: %d"), GlobalSeed);

	GenerateAll();
	GenerateMesh();
}

// ------------------------------------------------------------
// Generate Mesh
// ------------------------------------------------------------
void AWorldGenerationActor::GenerateMesh()
{
	// Determine the best available elevation data (walk pipeline backwards)
	const TArray<float>* ElevationSource = nullptr;

	if (CachedFinalElevation.Num() > 0)
	{
		ElevationSource = &CachedFinalElevation;
	}
	else if (CachedErodedElevation.Num() > 0)
	{
		ElevationSource = &CachedErodedElevation;
	}
	else if (CachedCombinedElevation.Num() > 0)
	{
		ElevationSource = &CachedCombinedElevation;
	}
	else if (CachedLandMask.Num() > 0)
	{
		// Derive a flat elevation from the land mask (land = 0 at sea level, ocean = 0)
		const int32 Count = CachedLandMask.Num();
		CachedCombinedElevation.SetNumUninitialized(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			CachedCombinedElevation[i] = 0.0f;
		}
		ElevationSource = &CachedCombinedElevation;
	}

	if (!ElevationSource || ElevationSource->Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run at least Step1_GenerateLandmass before generating mesh"));
		return;
	}

	// Biome map is optional — pass nullptr if not yet generated
	const TArray<int32>* BiomeSource = (CachedBiomeMap.Num() > 0) ? &CachedBiomeMap : nullptr;

	BuildTerrainMesh(*ElevationSource, BiomeSource);
}

// ------------------------------------------------------------
// Clear All
// ------------------------------------------------------------
void AWorldGenerationActor::ClearAll()
{
	Debug_Landmass = nullptr;
	Debug_BaseElevation = nullptr;
	Debug_UpliftMap = nullptr;
	Debug_CombinedElevation = nullptr;
	Debug_PlateauMap = nullptr;
	Debug_Rivers = nullptr;
	Debug_Lakes = nullptr;
	Debug_Waterfalls = nullptr;
	Debug_FlowAccumulation = nullptr;
	Debug_ErodedElevation = nullptr;
	Debug_CanyonMask = nullptr;
	Debug_ErosionDelta = nullptr;
	Debug_Temperature = nullptr;
	Debug_Moisture = nullptr;
	Debug_Precipitation = nullptr;
	Debug_BiomeMap = nullptr;
	Debug_SlopeMap = nullptr;
	Debug_BiomeBlendWeights = nullptr;
	Debug_TerrainArchetypeMap = nullptr;
	Debug_SurfaceOverlayMap = nullptr;
	Debug_GeneratedFeatureMap = nullptr;
	Debug_FinalElevation = nullptr;

	CachedLandMask.Empty();
	CachedBaseElevation.Empty();
	CachedUpliftMap.Empty();
	CachedCombinedElevation.Empty();
	CachedRiverMap.Empty();
	CachedErodedElevation.Empty();
	CachedCanyonMask.Empty();
	CachedErosionDeltaMap.Empty();
	CachedTemperature.Empty();
	CachedMoisture.Empty();
	CachedPrecipitation.Empty();
	CachedFinalElevation.Empty();
	CachedLakeMap.Empty();
	CachedWaterfallMap.Empty();
	CachedFlowAccumulation.Empty();
	CachedFlowDirection.Empty();
	CachedPlateauMap.Empty();
	CachedBiomeMap.Empty();
	CachedTerrainArchetypeMap.Empty();
	CachedSurfaceOverlayMap.Empty();
	CachedGeneratedFeatureMap.Empty();
	CachedSlopeMap.Empty();
	CachedBiomeBlendWeights.Empty();
	CachedVolcanicCenters.Empty();

	ActualSeedUsed = 0;
	ResolutionUsed = 0;

	if (TerrainMesh)
	{
		TerrainMesh->ClearAllMeshSections();
	}

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: All generated data cleared"));
}

// ------------------------------------------------------------
// Validate Terrain
// ------------------------------------------------------------
void AWorldGenerationActor::ValidateTerrain()
{
	if (CachedBiomeMap.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run GenerateAll or complete all stages before validating"));
		return;
	}

	UTerrainValidator* Validator = NewObject<UTerrainValidator>(this);
	if (!Validator)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create UTerrainValidator"));
		return;
	}

	Validator->Initialize(TextureResolution);

	LastValidationResult = Validator->Validate(
		CachedLandMask,
		CachedCombinedElevation,
		CachedUpliftMap,
		CachedFlowDirection,
		CachedRiverMap,
		CachedLakeMap,
		CachedWaterfallMap,
		CachedCanyonMask,
		CachedErodedElevation,
		CachedTemperature,
		CachedMoisture,
		CachedBiomeMap,
		CachedSlopeMap,
		CachedVolcanicCenters);

	// Log each issue to the output log
	for (const FTerrainValidationError& Err : LastValidationResult.Errors)
	{
		const TCHAR* SeverityStr =
			(Err.Severity == EValidationSeverity::Error) ? TEXT("ERROR") :
			(Err.Severity == EValidationSeverity::Warning) ? TEXT("WARN") : TEXT("INFO");

		UE_LOG(LogTemp, Log,
			TEXT("  [%s] %s: %s | Fix: %s | Inspect: %s"),
			SeverityStr, *Err.CheckName, *Err.Description, *Err.SuggestedFix, *Err.DebugMapToInspect);
	}

	UE_LOG(LogTemp, Log,
		TEXT("AWorldGenerationActor: Validation complete — %d errors, %d warnings, %d info"),
		LastValidationResult.ErrorCount, LastValidationResult.WarningCount, LastValidationResult.InfoCount);
}

// ------------------------------------------------------------
// Auto-Regeneration: PostEditChangeProperty + Preset Helpers
// ------------------------------------------------------------
#if WITH_EDITOR

void AWorldGenerationActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!bAutoRegenerate) return;

	// Skip interactive changes (slider drag) to avoid regenerating on every frame
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive) return;

	const FName MemberName = PropertyChangedEvent.MemberProperty
		? PropertyChangedEvent.MemberProperty->GetFName()
		: NAME_None;

	if (MemberName == NAME_None) return;

	// --- Quick Preset toggles ---

	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeVolcano))
	{
		ApplyPreset_Volcano(bIncludeVolcano);
		RegenerateFromStage(2);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeMountains))
	{
		ApplyPreset_Mountains(bIncludeMountains);
		RegenerateFromStage(2);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeHills))
	{
		ApplyPreset_Hills(bIncludeHills);
		RegenerateFromStage(2);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludePlateaus))
	{
		ApplyPreset_Plateaus(bIncludePlateaus);
		RegenerateFromStage(2);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeRivers))
	{
		ApplyPreset_Rivers(bIncludeRivers);
		RegenerateFromStage(3);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeCanyons))
	{
		ApplyPreset_Canyons(bIncludeCanyons);
		RegenerateFromStage(4);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TerrainRoughness))
	{
		ApplyTerrainRoughness(TerrainRoughness);
		RegenerateFromStage(2);
		return;
	}

	// --- Per-stage settings struct changes ---

	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, LandmassSettings))
	{
		RegenerateFromStage(1);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, UpliftSettings))
	{
		RegenerateFromStage(2);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, HydrologySettings))
	{
		RegenerateFromStage(3);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, ErosionSettings))
	{
		RegenerateFromStage(4);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, ClimateSettings))
	{
		RegenerateFromStage(5);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, BiomeAssignmentSettings))
	{
		RegenerateFromStage(6);
		return;
	}
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, RefinementSettings))
	{
		RegenerateFromStage(7);
		return;
	}

	// --- Global settings that affect the full pipeline ---

	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, GlobalSeed)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TextureResolution)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, LandmassType)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TargetLandAreaSqKm)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, OceanPaddingMeters))
	{
		RegenerateFromStage(1);
		return;
	}

	// --- Mesh-only settings ---

	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, MaxMapHeight)
		|| MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, OceanLevel))
	{
		GenerateMesh();
		return;
	}

	// Sea level affects biome classification (stage 6)
	if (MemberName == GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, SeaLevel))
	{
		RegenerateFromStage(6);
		return;
	}
}

void AWorldGenerationActor::RegenerateFromStage(int32 StageIndex)
{
	if (StageIndex <= 1) Step1_GenerateLandmass();
	if (StageIndex <= 2) Step2_GenerateUplift();
	if (StageIndex <= 3) Step3_GenerateHydrology();
	if (StageIndex <= 4) Step4_GenerateErosion();
	if (StageIndex <= 5) Step5_GenerateClimate();
	if (StageIndex <= 6) Step6_GenerateBiomes();
	if (StageIndex <= 7) Step7_GenerateRefinement();
	GenerateMesh();
}

void AWorldGenerationActor::ApplyPreset_Volcano(bool bEnable)
{
	if (bEnable)
	{
		UpliftSettings.VolcanicHotspotCount = 1;
		UpliftSettings.VolcanicRadius = 0.08f;
		UpliftSettings.VolcanicPeakHeight = 0.65f;
		UpliftSettings.CraterDepth = 0.3f;
		UpliftSettings.CraterRadiusFraction = 0.3f;
	}
	else
	{
		UpliftSettings.VolcanicHotspotCount = 0;
	}
}

void AWorldGenerationActor::ApplyPreset_Mountains(bool bEnable)
{
	if (bEnable)
	{
		UpliftSettings.MountainRidgeFrequency = 2.5f;
		UpliftSettings.MountainRidgeAmplitude = 0.35f;
		UpliftSettings.MountainSharpness = 1.5f;
		UpliftSettings.MountainOctaves = 4;
	}
	else
	{
		UpliftSettings.MountainRidgeAmplitude = 0.0f;
	}
}

void AWorldGenerationActor::ApplyPreset_Hills(bool bEnable)
{
	if (bEnable)
	{
		UpliftSettings.HillFrequency = 3.0f;
		UpliftSettings.HillAmplitude = 0.2f;
		UpliftSettings.HillOctaves = 3;
	}
	else
	{
		UpliftSettings.HillAmplitude = 0.0f;
	}
}

void AWorldGenerationActor::ApplyPreset_Plateaus(bool bEnable)
{
	if (bEnable)
	{
		UpliftSettings.PlateauNoiseFrequency = 1.8f;
		UpliftSettings.PlateauThreshold = 0.55f;
		UpliftSettings.PlateauFlatness = 0.7f;
		UpliftSettings.PlateauElevation = 0.45f;
	}
	else
	{
		UpliftSettings.PlateauFlatness = 0.0f;
	}
}

void AWorldGenerationActor::ApplyPreset_Rivers(bool bEnable)
{
	if (bEnable)
	{
		HydrologySettings.RiverThreshold = 0.005f;
		HydrologySettings.FlowJitter = 0.002f;
		HydrologySettings.LakeThreshold = 0.01f;
		HydrologySettings.WaterfallMinElevationDrop = 0.05f;
	}
	else
	{
		HydrologySettings.RiverThreshold = 1.0f;
		HydrologySettings.LakeThreshold = 1.0f;
	}
}

void AWorldGenerationActor::ApplyPreset_Canyons(bool bEnable)
{
	if (bEnable)
	{
		ErosionSettings.RiverIncisionStrength = 0.3f;
		ErosionSettings.CanyonDepthMultiplier = 1.5f;
		ErosionSettings.CanyonWidth = 3;
	}
	else
	{
		ErosionSettings.RiverIncisionStrength = 0.0f;
	}
}

void AWorldGenerationActor::ApplyTerrainRoughness(float Roughness)
{
	// Clamp just in case
	Roughness = FMath::Clamp(Roughness, 0.0f, 1.0f);

	// Mountain ridge amplitude: 0 at roughness=0, up to 0.6 at roughness=1
	UpliftSettings.MountainRidgeAmplitude = FMath::Lerp(0.0f, 0.6f, Roughness);

	// Mountain sharpness: softer at low roughness, sharper at high
	UpliftSettings.MountainSharpness = FMath::Lerp(1.0f, 2.5f, Roughness);

	// Hill amplitude: gentle at low roughness, moderate at mid, reduced at extreme high (mountains dominate)
	// Peaks at roughness ~0.4
	const float HillCurve = FMath::Clamp(1.0f - FMath::Abs(Roughness - 0.4f) * 2.0f, 0.0f, 1.0f);
	UpliftSettings.HillAmplitude = FMath::Lerp(0.05f, 0.3f, HillCurve);

	// Base noise persistence: flatter terrain uses smoother noise
	UpliftSettings.BaseNoisePersistence = FMath::Lerp(0.25f, 0.5f, Roughness);

	// Coastline gradient: flat terrain gets wider coastal plains
	UpliftSettings.CoastlineGradientWidth = FMath::RoundToInt32(FMath::Lerp(140.0f, 50.0f, Roughness));

	// Coastal variation: rougher terrain produces more dramatic cliffs at the shore
	UpliftSettings.CoastalVariation = FMath::Lerp(0.5f, 1.0f, Roughness);

	// Cliff floor height: rougher terrain gets taller cliff faces
	UpliftSettings.CliffFloorHeight = FMath::Lerp(0.4f, 0.8f, Roughness);

	// Mountain coverage: rough terrain lets mountains extend closer to coast
	UpliftSettings.MountainCoverage = FMath::Lerp(0.5f, 1.8f, Roughness);

	// Plateau flatness: more prominent in mid-range, reduced at extremes
	const float PlateauCurve = FMath::Clamp(1.0f - FMath::Abs(Roughness - 0.5f) * 3.0f, 0.0f, 1.0f);
	UpliftSettings.PlateauFlatness = FMath::Lerp(0.0f, 0.7f, PlateauCurve);
}

#endif // WITH_EDITOR

// ------------------------------------------------------------
// Build Terrain Mesh
// ------------------------------------------------------------
void AWorldGenerationActor::BuildTerrainMesh(const TArray<float>& Elevation, const TArray<int32>* BiomeMap)
{
	const int32 Resolution = TextureResolution;
	const int32 TotalPixels = Resolution * Resolution;

	if (Elevation.Num() != TotalPixels || CachedLandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Data size mismatch - expected %d pixels"), TotalPixels);
		return;
	}

	const bool bHasBiomeMap = BiomeMap && BiomeMap->Num() == TotalPixels;

	// World size derived from target land area + ocean padding (computed during landmass generation)
	const float WorldSizeCm = CachedWorldSizeCm;

	// Prepare mesh arrays
	const int32 VertexCount = TotalPixels;
	const int32 TriangleCount = (Resolution - 1) * (Resolution - 1) * 2;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;

	Vertices.SetNumUninitialized(VertexCount);
	Normals.SetNumZeroed(VertexCount);
	UVs.SetNumUninitialized(VertexCount);
	VertexColors.SetNumUninitialized(VertexCount);
	Triangles.SetNumUninitialized(TriangleCount * 3);

	const float CellSize = WorldSizeCm / static_cast<float>(Resolution - 1);
	const FLinearColor OceanLinear = GetBiomeDebugColor(EBiomeType::Ocean);
	const FColor OceanColor = OceanLinear.ToFColor(false);

	// Convert meters to cm for UE world units
	const float MaxHeightCm = MaxMapHeight * 100.0f;
	const float OceanDepthCm = OceanLevel * 100.0f;

	// Build vertices
	for (int32 Y = 0; Y < Resolution; ++Y)
	{
		for (int32 X = 0; X < Resolution; ++X)
		{
			const int32 Index = Y * Resolution + X;

			const float WorldX = static_cast<float>(X) * CellSize;
			const float WorldY = static_cast<float>(Y) * CellSize;

			float WorldZ;
			if (CachedLandMask[Index] == 0)
			{
				WorldZ = -OceanDepthCm;
				VertexColors[Index] = OceanColor;
			}
			else
			{
				WorldZ = Elevation[Index] * MaxHeightCm;
				if (bHasBiomeMap)
				{
					const EBiomeType BiomeType = static_cast<EBiomeType>((*BiomeMap)[Index]);
					const FLinearColor BiomeColor = GetBiomeDebugColor(BiomeType);
					VertexColors[Index] = BiomeColor.ToFColor(false);
				}
				else
				{
					// Grayscale based on elevation when biome data is not available
					const uint8 Gray = static_cast<uint8>(FMath::Clamp(Elevation[Index] * 255.0f, 0.0f, 255.0f));
					VertexColors[Index] = FColor(Gray, Gray, Gray, 255);
				}
			}

			Vertices[Index] = FVector(WorldX, WorldY, WorldZ);

			UVs[Index] = FVector2D(
				static_cast<float>(X) / static_cast<float>(Resolution - 1),
				static_cast<float>(Y) / static_cast<float>(Resolution - 1));
		}
	}

	// Generate triangle indices (two triangles per grid cell)
	int32 TriIdx = 0;
	for (int32 Y = 0; Y < Resolution - 1; ++Y)
	{
		for (int32 X = 0; X < Resolution - 1; ++X)
		{
			const int32 TopLeft = Y * Resolution + X;
			const int32 TopRight = TopLeft + 1;
			const int32 BottomLeft = (Y + 1) * Resolution + X;
			const int32 BottomRight = BottomLeft + 1;

			Triangles[TriIdx++] = TopLeft;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = TopRight;

			Triangles[TriIdx++] = TopRight;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = BottomRight;
		}
	}

	// Compute normals
	for (int32 i = 0; i < TriangleCount; ++i)
	{
		const int32 I0 = Triangles[i * 3 + 0];
		const int32 I1 = Triangles[i * 3 + 1];
		const int32 I2 = Triangles[i * 3 + 2];

		const FVector Edge1 = Vertices[I1] - Vertices[I0];
		const FVector Edge2 = Vertices[I2] - Vertices[I0];
		const FVector FaceNormal = FVector::CrossProduct(Edge1, Edge2);

		Normals[I0] += FaceNormal;
		Normals[I1] += FaceNormal;
		Normals[I2] += FaceNormal;
	}

	for (int32 i = 0; i < VertexCount; ++i)
	{
		Normals[i] = Normals[i].GetSafeNormal();
	}

	// Create the mesh section
	if (!TerrainMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: TerrainMesh component is null"));
		return;
	}

	TerrainMesh->ClearAllMeshSections();

	TArray<FProcMeshTangent> Tangents;
	TerrainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Terrain mesh built - %d vertices, %d triangles, WorldSize=%.0fcm, MaxHeight=%.0fm, OceanLevel=%.0fm"),
		VertexCount, TriangleCount, WorldSizeCm, MaxMapHeight, OceanLevel);
}

// ------------------------------------------------------------
// Debug Texture Helpers
// ------------------------------------------------------------
UTexture2D* AWorldGenerationActor::CreateGrayscaleDebugTexture(int32 Res, const TArray<float>& Data)
{
	const int32 TotalPixels = Res * Res;
	if (Res <= 0 || Data.Num() != TotalPixels)
	{
		return nullptr;
	}

	TArray<FColor> PixelData;
	PixelData.SetNumUninitialized(TotalPixels);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		const uint8 Val = static_cast<uint8>(FMath::Clamp(Data[i] * 255.0f, 0.0f, 255.0f));
		PixelData[i] = FColor(Val, Val, Val, 255);
	}

	return CreateColorDebugTexture(Res, PixelData);
}

UTexture2D* AWorldGenerationActor::CreateColorDebugTexture(int32 Res, const TArray<FColor>& PixelData)
{
	if (Res <= 0 || PixelData.Num() != Res * Res)
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Res, Res, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Failed to create transient texture"));
		return nullptr;
	}

	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->Filter = TF_Nearest;
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_VectorDisplacementmap;

	FTexturePlatformData* PlatformData = Texture->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Texture has no platform data"));
		return nullptr;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, PixelData.GetData(), PixelData.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();

	Texture->UpdateResource();

	return Texture;
}
