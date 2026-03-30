// WorldGenerationActor.cpp
// Geology-driven world generation pipeline orchestrator

#include "WorldGenerationActor.h"
#include "UpliftGenerator.h"
#include "HydrologyGenerator.h"
#include "ErosionGenerator.h"
#include "ClimateGenerator.h"
#include "BiomeAssignmentGenerator.h"
#include "TerrainRefinementGenerator.h"
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
	LandmassSettings.MapType = MapType;
	LandmassSettings.MapSize = MapSize;
	LandmassSettings.LandCoveragePercent = LandCoveragePercent;
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
	Debug_ErodedElevation = CreateGrayscaleDebugTexture(TextureResolution, CachedErodedElevation);

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

	Generator->Initialize(BiomeAssignmentSettings, TextureResolution);

	if (!Generator->Generate(CachedErodedElevation, CachedTemperature, CachedMoisture,
		CachedPrecipitation, CachedLandMask, CachedRiverMap, CachedLakeMap, CachedVolcanicCenters))
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Biome assignment failed"));
		return;
	}

	CachedBiomeMap = Generator->GetBiomeMap();
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
// Generate Mesh
// ------------------------------------------------------------
void AWorldGenerationActor::GenerateMesh()
{
	if (CachedFinalElevation.Num() == 0 || CachedBiomeMap.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AWorldGenerationActor: Run GenerateAll or complete all stages first"));
		return;
	}

	BuildTerrainMesh();
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
	Debug_Temperature = nullptr;
	Debug_Moisture = nullptr;
	Debug_Precipitation = nullptr;
	Debug_BiomeMap = nullptr;
	Debug_SlopeMap = nullptr;
	Debug_BiomeBlendWeights = nullptr;
	Debug_FinalElevation = nullptr;

	CachedLandMask.Empty();
	CachedBaseElevation.Empty();
	CachedUpliftMap.Empty();
	CachedCombinedElevation.Empty();
	CachedRiverMap.Empty();
	CachedErodedElevation.Empty();
	CachedTemperature.Empty();
	CachedMoisture.Empty();
	CachedPrecipitation.Empty();
	CachedFinalElevation.Empty();
	CachedLakeMap.Empty();
	CachedWaterfallMap.Empty();
	CachedFlowAccumulation.Empty();
	CachedPlateauMap.Empty();
	CachedBiomeMap.Empty();
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
// Build Terrain Mesh
// ------------------------------------------------------------
void AWorldGenerationActor::BuildTerrainMesh()
{
	const int32 Resolution = TextureResolution;
	const int32 TotalPixels = Resolution * Resolution;

	if (CachedFinalElevation.Num() != TotalPixels || CachedBiomeMap.Num() != TotalPixels || CachedLandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error, TEXT("AWorldGenerationActor: Data size mismatch - expected %d pixels"), TotalPixels);
		return;
	}

	// World size from MapSize enum
	float WorldSizeCm;
	switch (MapSize)
	{
	case EMapSize::Large:  WorldSizeCm = 200000.0f; break;
	case EMapSize::Medium: WorldSizeCm = 100000.0f; break;
	case EMapSize::Small:  WorldSizeCm = 50000.0f;  break;
	default:               WorldSizeCm = 100000.0f; break;
	}

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
				WorldZ = 0.0f;
				VertexColors[Index] = OceanColor;
			}
			else
			{
				WorldZ = CachedFinalElevation[Index] * MaxMapHeight;
				const EBiomeType BiomeType = static_cast<EBiomeType>(CachedBiomeMap[Index]);
				const FLinearColor BiomeColor = GetBiomeDebugColor(BiomeType);
				VertexColors[Index] = BiomeColor.ToFColor(false);
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

	UE_LOG(LogTemp, Log, TEXT("AWorldGenerationActor: Terrain mesh built - %d vertices, %d triangles, WorldSize=%.0f, MaxHeight=%.0f"),
		VertexCount, TriangleCount, WorldSizeCm, MaxMapHeight);
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
