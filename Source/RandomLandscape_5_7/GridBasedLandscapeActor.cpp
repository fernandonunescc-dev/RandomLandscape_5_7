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

	// Calculate actual map size from grid count and cell size
	float ActualMapSize = MapGridCount * CellSizeMeters;

	// Initialize systems
	GridManager.Initialize(ActualMapSize, CellSizeMeters);

	// Initialize property cache
	CachedMapGridCount = MapGridCount;
	CachedCellSizeMeters = CellSizeMeters;
	bHasTerrainChanged = true;
}

void AGridBasedLandscapeActor::BeginPlay()
{
	Super::BeginPlay();
	GenerateLandscape();
}

void AGridBasedLandscapeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	// Only regenerate if terrain-relevant properties changed
	if (HasTerrainPropertiesChanged())
	{
		float ActualMapSize = MapGridCount * CellSizeMeters;
		GridManager.Initialize(ActualMapSize, CellSizeMeters);
		ClearLandscape();
		GenerateLandscape();
		bHasTerrainChanged = false;
	}
}

void AGridBasedLandscapeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Check if async generation finished
	if (bAsyncGenerationComplete)
	{
		OnAsyncGenerationComplete();
		bAsyncGenerationComplete = false;
	}

	// Process queued cells on main thread if not using async
	if (!bAsyncGeneration && PendingCellsToGenerate.Num() > 0)
	{
		ProcessQueuedCells();
	}

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
	
	// Queue cells for generation (either async or progressive)
	PendingCellsToGenerate.Empty();
	QueueCellsForGeneration();

	// Start async or main-thread processing
	if (bAsyncGeneration)
	{
		StartAsyncGeneration();
	}
	else
	{
		// For non-async, process all cells immediately in this frame
		// (They'll show up, might cause a single frame hitch but terrain will render)
		while (PendingCellsToGenerate.Num() > 0)
		{
			TPair<int32, int32> Cell = PendingCellsToGenerate[0];
			PendingCellsToGenerate.RemoveAt(0);
			
			// Generate height data
			FGridCellData* CellData = GridManager.GetOrCreateGridCell(Cell.Key, Cell.Value);
			if (CellData && !CellData->bIsDataValid)
			{
				float ActualMapSize = MapGridCount * CellSizeMeters;
				NoiseGenerator.GenerateHeightMap(Cell.Key, Cell.Value, ActualMapSize, CellSizeMeters, 
					CellData->HeightMap, VERTICES_PER_CELL);

				CellData->MinHeight = FLT_MAX;
				CellData->MaxHeight = -FLT_MAX;

				int32 VertexCount = CellData->HeightMap.Num();
				CellData->BiomeMap.Reserve(VertexCount);

				for (float NormalizedHeight : CellData->HeightMap)
				{
					float ActualHeight = (NormalizedHeight * MaxHeightVariation) + HeightOffset;
					CellData->MinHeight = FMath::Min(CellData->MinHeight, ActualHeight);
					CellData->MaxHeight = FMath::Max(CellData->MaxHeight, ActualHeight);
					EBiomeType Biome = BiomeClassifier.GetBiomeAtHeight(ActualHeight);
					CellData->BiomeMap.Add(Biome);
				}
				CellData->bIsDataValid = true;
			}

			// Create mesh
			if (CellData && CellData->bIsDataValid)
			{
				UProceduralMeshComponent* Mesh = GetOrCreateCellMesh(Cell.Key, Cell.Value);
				if (Mesh)
				{
					CreateCellMesh(CellData, Mesh);

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
		}
	}

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
	// MapGridCount is the grid count per side (e.g., 10 = 10x10 grid)
	int32 GridCountX = MapGridCount;
	int32 GridCountY = MapGridCount;

	// Generate all grid cells
	for (int32 Y = 0; Y < GridCountY; ++Y)
	{
		for (int32 X = 0; X < GridCountX; ++X)
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
		float ActualMapSize = MapGridCount * CellSizeMeters;
		NoiseGenerator.GenerateHeightMap(InGridX, InGridY, ActualMapSize, CellSizeMeters, 
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

	float CellSizeUnreal = CellSizeMeters * METERS_TO_UNREAL_UNITS;
	float ActualMapSize = MapGridCount * CellSizeMeters;
	float MapSizeUnreal = ActualMapSize * METERS_TO_UNREAL_UNITS;
	float OriginX = GetActorLocation().X + (InCellData->GridX * CellSizeUnreal) - (MapSizeUnreal / 2.0f);
	float OriginY = GetActorLocation().Y + (InCellData->GridY * CellSizeUnreal) - (MapSizeUnreal / 2.0f);

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
	float CellSizeUnreal = CellSizeMeters * METERS_TO_UNREAL_UNITS;
	float ActualMapSize = MapGridCount * CellSizeMeters;
	float MapSizeUnreal = ActualMapSize * METERS_TO_UNREAL_UNITS;
	float RelX = InWorldPos.X - GetActorLocation().X + (MapSizeUnreal / 2.0f);
	float RelY = InWorldPos.Y - GetActorLocation().Y + (MapSizeUnreal / 2.0f);

	OutGridX = FMath::Clamp(FMath::FloorToInt(RelX / CellSizeUnreal), 0, MapGridCount - 1);
	OutGridY = FMath::Clamp(FMath::FloorToInt(RelY / CellSizeUnreal), 0, MapGridCount - 1);
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

bool AGridBasedLandscapeActor::HasTerrainPropertiesChanged()
{
	bool bChanged = false;

	// Check grid count changes
	if (MapGridCount != CachedMapGridCount)
	{
		CachedMapGridCount = MapGridCount;
		bChanged = true;
	}

	// Check cell size changes
	if (CellSizeMeters != CachedCellSizeMeters)
	{
		CachedCellSizeMeters = CellSizeMeters;
		bChanged = true;
	}

	// Check height variation changes
	if (MaxHeightVariation != CachedMaxHeightVariation)
	{
		CachedMaxHeightVariation = MaxHeightVariation;
		bChanged = true;
	}

	// Check height offset changes
	if (HeightOffset != CachedHeightOffset)
	{
		CachedHeightOffset = HeightOffset;
		bChanged = true;
	}

	// Check noise frequency changes
	if (PrimaryNoiseFrequency != CachedPrimaryNoiseFrequency)
	{
		CachedPrimaryNoiseFrequency = PrimaryNoiseFrequency;
		bChanged = true;
	}

	// Check noise octave changes
	if (PrimaryNoiseOctaves != CachedPrimaryNoiseOctaves)
	{
		CachedPrimaryNoiseOctaves = PrimaryNoiseOctaves;
		bChanged = true;
	}

	if (SecondaryNoiseFrequency != CachedSecondaryNoiseFrequency)
	{
		CachedSecondaryNoiseFrequency = SecondaryNoiseFrequency;
		bChanged = true;
	}

	if (SecondaryNoiseOctaves != CachedSecondaryNoiseOctaves)
	{
		CachedSecondaryNoiseOctaves = SecondaryNoiseOctaves;
		bChanged = true;
	}

	if (TertiaryNoiseFrequency != CachedTertiaryNoiseFrequency)
	{
		CachedTertiaryNoiseFrequency = TertiaryNoiseFrequency;
		bChanged = true;
	}

	if (TertiaryNoiseOctaves != CachedTertiaryNoiseOctaves)
	{
		CachedTertiaryNoiseOctaves = TertiaryNoiseOctaves;
		bChanged = true;
	}

	return bChanged;
}

void AGridBasedLandscapeActor::QueueCellsForGeneration()
{
	// MapGridCount is the grid count per side (e.g., 10 = 10x10 grid)
	int32 GridCountX = MapGridCount;
	int32 GridCountY = MapGridCount;

	// Queue all cells for generation
	for (int32 Y = 0; Y < GridCountY; ++Y)
	{
		for (int32 X = 0; X < GridCountX; ++X)
		{
			PendingCellsToGenerate.Add(TPair<int32, int32>(X, Y));
		}
	}
}

void AGridBasedLandscapeActor::ProcessQueuedCells()
{
	// Process limited number of cells per frame to keep main thread responsive
	int32 CellsToProcessThisFrame = FMath::Min(CellsPerFrame, PendingCellsToGenerate.Num());
	
	for (int32 i = 0; i < CellsToProcessThisFrame && PendingCellsToGenerate.Num() > 0; ++i)
	{
		TPair<int32, int32> Cell = PendingCellsToGenerate[0];
		PendingCellsToGenerate.RemoveAt(0);
		GenerateGridCell(Cell.Key, Cell.Value);
	}
}

void AGridBasedLandscapeActor::StartAsyncGeneration()
{
	if (bIsGeneratingAsync || PendingCellsToGenerate.Num() == 0)
	{
		return;
	}

	bIsGeneratingAsync = true;
	bAsyncGenerationComplete = false;

	// Create a copy of pending cells for the async task
	TArray<TPair<int32, int32>> CellsToGenerate = PendingCellsToGenerate;
	PendingCellsToGenerate.Empty();

	// Launch async task on background thread
	// NOTE: Only generate height map data, NOT mesh components (must be main thread)
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, CellsToGenerate]()
	{
		// Generate height maps only on background thread (thread-safe)
		// Do NOT create mesh components here - that must be on main thread
		for (const TPair<int32, int32>& Cell : CellsToGenerate)
		{
			FGridCellData* CellData = GridManager.GetOrCreateGridCell(Cell.Key, Cell.Value);
			if (!CellData || CellData->bIsDataValid)
			{
				continue; // Skip if already generated
			}

			// Generate height map if not already done
			float ActualMapSize = MapGridCount * CellSizeMeters;
			NoiseGenerator.GenerateHeightMap(Cell.Key, Cell.Value, ActualMapSize, CellSizeMeters, 
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

		// Signal main thread that height map generation is complete
		// Main thread will create meshes in OnAsyncGenerationComplete
		bAsyncGenerationComplete = true;
	});
}

void AGridBasedLandscapeActor::OnAsyncGenerationComplete()
{
	// Height maps are now generated on background thread
	// Now create meshes on main thread (required by Unreal)
	bIsGeneratingAsync = false;

	// We need to process ALL pending cells that were queued, not just loaded ones
	// The background thread generated height data for cells that are in our pending list
	int32 GridCountX = MapGridCount;
	int32 GridCountY = MapGridCount;

	// Create meshes for all cells that should exist
	for (int32 Y = 0; Y < GridCountY; ++Y)
	{
		for (int32 X = 0; X < GridCountX; ++X)
		{
			FGridCellData* CellData = GridManager.GetGridCell(X, Y);
			if (CellData && CellData->bIsDataValid)
			{
				// Get or create mesh component
				UProceduralMeshComponent* Mesh = GetOrCreateCellMesh(X, Y);
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
		}
	}

	if (bDebugShowStats)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, 
				TEXT("Landscape generation complete (async)"));
		}
	}
}
