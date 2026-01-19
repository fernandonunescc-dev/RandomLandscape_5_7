// Copyright Epic Games, Inc. All Rights Reserved.

#include "GridBasedLandscapeActor.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AGridBasedLandscapeActor::AGridBasedLandscapeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;  // Update streaming every 0.5 seconds

	// Create root scene component
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;

	// Initialize systems
	GridManager.Initialize(MapSize, GridScale);
}

void AGridBasedLandscapeActor::BeginPlay()
{
	Super::BeginPlay();
	GenerateLandscape();
}

void AGridBasedLandscapeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	GridManager.Initialize(MapSize, GridScale);
	GenerateLandscape();
}

void AGridBasedLandscapeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update streaming around camera
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			UpdateStreamingAroundPoint(Pawn->GetActorLocation());
		}
	}

	// Debug stats
	if (bDebugShowStats)
	{
		PrintDebugStats();
	}
}

void AGridBasedLandscapeActor::GenerateLandscape()
{
	if (!GetWorld())
	{
		return;
	}

	UpdateNoiseSettings();
	GenerateLandscapeInternal();

	if (bGenerateWaterPlanes)
	{
		GenerateWaterPlanes();
	}
}

void AGridBasedLandscapeActor::ClearLandscape()
{
	// Clear mesh components
	for (auto& Pair : GridMeshes)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	GridMeshes.Empty();

	// Clear water planes
	for (AActor* WaterPlane : WaterPlanes)
	{
		if (WaterPlane)
		{
			WaterPlane->Destroy();
		}
	}
	WaterPlanes.Empty();

	// Clear grid cache
	GridManager.Clear();
}

void AGridBasedLandscapeActor::GenerateLandscapeInternal()
{
	int32 GridCountX = FMath::CeilToInt(MapSize / GridScale);
	int32 GridCountY = FMath::CeilToInt(MapSize / GridScale);

	// Generate all grid cells
	for (int32 Y = 0; Y < GridCountX; ++Y)
	{
		for (int32 X = 0; X < GridCountY; ++X)
		{
			GenerateGridCell(X, Y);
		}
	}
}

void AGridBasedLandscapeActor::GenerateGridCell(int32 InGridX, int32 InGridY)
{
	// Get or create cell data
	FGridCellData* CellData = GridManager.GetOrCreateGridCell(InGridX, InGridY);
	if (!CellData)
	{
		return;
	}

	// Generate height map if not already done
	if (!CellData->bIsDataValid)
	{
		NoiseGenerator.GenerateHeightMap(InGridX, InGridY, MapSize, GridScale, 
			CellData->HeightMap, VERTICES_PER_CELL);

		// Convert normalized heights to Unreal units and classify biomes
		CellData->MinHeight = FLT_MAX;
		CellData->MaxHeight = -FLT_MAX;

		int32 VertexCount = CellData->HeightMap.Num();
		CellData->BiomeMap.Reserve(VertexCount);

		for (float NormalizedHeight : CellData->HeightMap)
		{
			// Convert from -1..1 range to actual height in cm
			float ActualHeight = (NormalizedHeight * MaxHeightVariation) + HeightOffset;
			CellData->MinHeight = FMath::Min(CellData->MinHeight, ActualHeight);
			CellData->MaxHeight = FMath::Max(CellData->MaxHeight, ActualHeight);

			// Classify biome
			EBiomeType Biome = BiomeClassifier.GetBiomeAtHeight(ActualHeight);
			CellData->BiomeMap.Add(Biome);
		}

		CellData->bIsDataValid = true;
	}

	// Get or create mesh component
	UProceduralMeshComponent* Mesh = GetOrCreateCellMesh(InGridX, InGridY);
	if (Mesh)
	{
		CreateCellMesh(CellData, Mesh);

		// Determine dominant biome for this cell
		TMap<EBiomeType, int32> BiomeCount;
		for (EBiomeType Biome : CellData->BiomeMap)
		{
			BiomeCount.FindOrAdd(Biome)++;
		}

		EBiomeType DominantBiome = EBiomeType::Plains;
		int32 MaxCount = 0;
		for (const auto& Pair : BiomeCount)
		{
			if (Pair.Value > MaxCount)
			{
				MaxCount = Pair.Value;
				DominantBiome = Pair.Key;
			}
		}

		ApplyBiomeMaterial(Mesh, DominantBiome);
	}
}

void AGridBasedLandscapeActor::CreateCellMesh(FGridCellData* InCellData, UProceduralMeshComponent* InMeshComponent)
{
	if (!InCellData || InCellData->HeightMap.Num() == 0)
	{
		return;
	}

	// Calculate grid resolution
	int32 GridResolution = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(InCellData->HeightMap.Num())));

	// Create vertices
	TArray<FVector> Vertices;
	Vertices.Reserve(InCellData->HeightMap.Num());

	float CellSizeUnreal = GridScale * METERS_TO_UNREAL_UNITS;
	float OriginX = GetActorLocation().X + (InCellData->GridX * CellSizeUnreal) - (MapSize * METERS_TO_UNREAL_UNITS / 2.0f);
	float OriginY = GetActorLocation().Y + (InCellData->GridY * CellSizeUnreal) - (MapSize * METERS_TO_UNREAL_UNITS / 2.0f);

	float VertexSpacing = CellSizeUnreal / (GridResolution - 1);

	for (int32 Y = 0; Y < GridResolution; ++Y)
	{
		for (int32 X = 0; X < GridResolution; ++X)
		{
			int32 Index = Y * GridResolution + X;
			if (Index < InCellData->HeightMap.Num())
			{
				float NormalizedHeight = InCellData->HeightMap[Index];
				float ActualHeight = (NormalizedHeight * MaxHeightVariation) + HeightOffset;

				float PosX = OriginX + (X * VertexSpacing);
				float PosY = OriginY + (Y * VertexSpacing);

				Vertices.Add(FVector(PosX, PosY, ActualHeight));
			}
		}
	}

	// Create triangles
	TArray<int32> Triangles;
	for (int32 Y = 0; Y < GridResolution - 1; ++Y)
	{
		for (int32 X = 0; X < GridResolution - 1; ++X)
		{
			int32 A = Y * GridResolution + X;
			int32 B = A + 1;
			int32 C = A + GridResolution;
			int32 D = C + 1;

			// First triangle
			Triangles.Add(A);
			Triangles.Add(C);
			Triangles.Add(B);

			// Second triangle
			Triangles.Add(B);
			Triangles.Add(C);
			Triangles.Add(D);
		}
	}

	// Create UVs
	TArray<FVector2D> UVs;
	UVs.Reserve(Vertices.Num());
	for (int32 Y = 0; Y < GridResolution; ++Y)
	{
		for (int32 X = 0; X < GridResolution; ++X)
		{
			float U = static_cast<float>(X) / (GridResolution - 1);
			float V = static_cast<float>(Y) / (GridResolution - 1);
			UVs.Add(FVector2D(U, V));
		}
	}

	// Create normals (we'll let UE calculate them)
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;

	// Create mesh section
	InMeshComponent->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, 
		TArray<FColor>(), Tangents, true);
}

void AGridBasedLandscapeActor::GenerateWaterPlanes()
{
	// TODO: Implement water plane generation
	// This will create flat planes at the water level for ocean/lake areas
}

UProceduralMeshComponent* AGridBasedLandscapeActor::GetOrCreateCellMesh(int32 InGridX, int32 InGridY)
{
	FString Key = GetGridCellKey(InGridX, InGridY);

	if (GridMeshes.Contains(Key))
	{
		return GridMeshes[Key];
	}

	// Create new mesh component
	UProceduralMeshComponent* NewMesh = NewObject<UProceduralMeshComponent>(this);
	NewMesh->SetupAttachment(RootScene);
	NewMesh->RegisterComponent();

	GridMeshes.Add(Key, NewMesh);
	return NewMesh;
}

void AGridBasedLandscapeActor::UpdateStreamingAroundPoint(const FVector& InCenterPoint)
{
	int32 CenterGridX, CenterGridY;
	GetGridCellFromWorldPosition(InCenterPoint, CenterGridX, CenterGridY);

	// Unload cells outside streaming radius
	GridManager.StreamCells(CenterGridX, CenterGridY, StreamingLoadRadius);
}

void AGridBasedLandscapeActor::GetGridCellFromWorldPosition(const FVector& InWorldPos, int32& OutGridX, int32& OutGridY)
{
	float CellSizeUnreal = GridScale * METERS_TO_UNREAL_UNITS;
	float RelX = InWorldPos.X - GetActorLocation().X + (MapSize * METERS_TO_UNREAL_UNITS / 2.0f);
	float RelY = InWorldPos.Y - GetActorLocation().Y + (MapSize * METERS_TO_UNREAL_UNITS / 2.0f);

	OutGridX = FMath::Clamp(FMath::FloorToInt(RelX / CellSizeUnreal), 0, FMath::CeilToInt(MapSize / GridScale) - 1);
	OutGridY = FMath::Clamp(FMath::FloorToInt(RelY / CellSizeUnreal), 0, FMath::CeilToInt(MapSize / GridScale) - 1);
}

void AGridBasedLandscapeActor::ApplyBiomeMaterial(UProceduralMeshComponent* InMesh, EBiomeType InBiomeType)
{
	if (!InMesh)
	{
		return;
	}

	// If debug visualization is on, use biome-specific colors
	if (bDebugVisualizeBiomes)
	{
		FBiomeDefinition BiomeDef = BiomeClassifier.GetBiomeDefinition(InBiomeType);
		// Create a dynamic material with the biome color for visualization
		// TODO: Implement proper debug material assignment
	}
	else if (TerrainMaterial)
	{
		InMesh->SetMaterial(0, TerrainMaterial);
	}
}

void AGridBasedLandscapeActor::UpdateNoiseSettings()
{
	NoiseGenerator.PrimaryNoiseFrequency = PrimaryNoiseFrequency;
	NoiseGenerator.PrimaryNoiseOctaves = PrimaryNoiseOctaves;
	NoiseGenerator.SecondaryNoiseFrequency = SecondaryNoiseFrequency;
	NoiseGenerator.SecondaryNoiseOctaves = SecondaryNoiseOctaves;
	NoiseGenerator.TertiaryNoiseFrequency = TertiaryNoiseFrequency;
	NoiseGenerator.TertiaryNoiseOctaves = TertiaryNoiseOctaves;
}

FString AGridBasedLandscapeActor::GetGridCellKey(int32 InGridX, int32 InGridY) const
{
	return FString::Printf(TEXT("Cell_%d_%d"), InGridX, InGridY);
}

void AGridBasedLandscapeActor::PrintDebugStats()
{
	int32 CellsLoaded, CellsMax;
	GridManager.GetCacheStats(CellsLoaded, CellsMax);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Yellow, 
			FString::Printf(TEXT("Landscape Stats - Cells: %d/%d"), CellsLoaded, CellsMax));
	}
}
