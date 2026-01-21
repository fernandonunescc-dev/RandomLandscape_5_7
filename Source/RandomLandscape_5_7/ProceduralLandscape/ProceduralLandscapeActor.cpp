// Copyright Fernando Araujo. All Rights Reserved.

#include "ProceduralLandscapeActor.h"
#include "LandscapeMeshBuilder.h"
#include "Engine/Engine.h"

// ============================================================================
// Constructor
// ============================================================================

AProceduralLandscapeActor::AProceduralLandscapeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;  // 20 Hz for async polling

	// Create root component
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootScene;
}

// ============================================================================
// AActor Interface
// ============================================================================

void AProceduralLandscapeActor::BeginPlay()
{
	Super::BeginPlay();
	
	InitializeServices();
	StartGeneration();
}

void AProceduralLandscapeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Process completed async chunks
	if (bIsGenerating && AsyncGenerator)
	{
		ProcessCompletedChunks();
		
		// Check if generation complete
		if (!AsyncGenerator->IsGenerating())
		{
			bIsGenerating = false;
			
			if (bDebugShowStats && GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green,
					FString::Printf(TEXT("Landscape generation complete! %d chunks created."), ChunksCreated));
			}
		}
	}

	// Debug stats
	if (bDebugShowStats)
	{
		PrintDebugStats();
	}
}

void AProceduralLandscapeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// In editor, regenerate if settings changed OR if no chunks exist yet (first spawn)
	bool bNeedsGeneration = HasSettingsChanged() || ChunkMeshes.Num() == 0;
	
	if (bAutoRegenerateInEditor && bNeedsGeneration)
	{
		// For editor, always use sync generation for immediate feedback
		InitializeServices();
		ClearLandscape();
		GenerateSynchronous();
		CacheCurrentSettings();
	}
}

void AProceduralLandscapeActor::Destroyed()
{
	// Cleanup async generator first (may have pending tasks)
	if (AsyncGenerator)
	{
		AsyncGenerator->CancelAllPending();
		AsyncGenerator->WaitForCompletion();
	}
	
	ClearLandscape();
	
	Super::Destroyed();
}

// ============================================================================
// Blueprint Functions
// ============================================================================

void AProceduralLandscapeActor::RegenerateLandscape()
{
	ClearLandscape();
	InitializeServices();
	StartGeneration();
}

void AProceduralLandscapeActor::ClearLandscape()
{
	// Cancel any pending async work
	if (AsyncGenerator)
	{
		AsyncGenerator->CancelAllPending();
	}

	// Destroy all mesh components
	for (auto& Pair : ChunkMeshes)
	{
		if (Pair.Value)
		{
			Pair.Value->DestroyComponent();
		}
	}
	ChunkMeshes.Empty();

	bIsGenerating = false;
	ChunksCreated = 0;
}

// ============================================================================
// Initialization
// ============================================================================

void AProceduralLandscapeActor::InitializeServices()
{
	// Create and configure noise service
	NoiseService = MakeUnique<FLandscapeNoiseService>();
	NoiseService->NoiseFrequency = NoiseFrequency;
	NoiseService->NoiseOctaves = NoiseOctaves;
	NoiseService->NoiseLacunarity = NoiseLacunarity;
	NoiseService->NoiseGain = NoiseGain;
	NoiseService->HeightExponent = HeightExponent;
	NoiseService->MaxHeight = Resolution.MaxHeightMeters * METERS_TO_UU;
	NoiseService->MinHeight = 0.0f;
	NoiseService->Initialize(TerrainSeed);

	UE_LOG(LogTemp, Log, TEXT("========== LANDSCAPE INIT =========="));
	UE_LOG(LogTemp, Log, TEXT("Map: %.0fm x %.0fm | Max Height: %.0fm"), 
		Resolution.MapSizeMeters, Resolution.MapSizeMeters, Resolution.MaxHeightMeters);
	UE_LOG(LogTemp, Log, TEXT("Noise: Freq=%.4f, Octaves=%d, HeightExp=%.1f, Seed=%d"), 
		NoiseFrequency, NoiseOctaves, HeightExponent, TerrainSeed);
	if (bIslandMode)
	{
		UE_LOG(LogTemp, Log, TEXT("Island Mode: ON | Falloff: %.0fm | Exponent: %.2f"), 
			EdgeFalloffDistance, EdgeFalloffExponent);
	}
	UE_LOG(LogTemp, Log, TEXT("====================================="));

	// Calculate ACTUAL map size (may differ from requested due to chunk boundaries)
	const int32 ChunksPerSide = Resolution.GetChunksPerSide();
	const float ChunkSizeMeters = Resolution.GetChunkSizeMeters();
	const float ActualMapSizeMeters = ChunksPerSide * ChunkSizeMeters;
	const float ActualMapSizeUU = ActualMapSizeMeters * METERS_TO_UU;

	// Setup island edge configuration for async generator
	FIslandEdgeConfig EdgeConfig;
	EdgeConfig.bEnabled = bIslandMode;
	EdgeConfig.FalloffDistance = EdgeFalloffDistance * METERS_TO_UU;
	EdgeConfig.SeaLevel = SeaLevel * METERS_TO_UU;
	EdgeConfig.SkirtDepth = SkirtDepth * METERS_TO_UU;
	EdgeConfig.FalloffExponent = EdgeFalloffExponent;
	EdgeConfig.MapSizeUU = ActualMapSizeUU;
	EdgeConfig.MapCenter = FVector2D(GetActorLocation().X, GetActorLocation().Y);
	EdgeConfig.MaxTerrainHeight = Resolution.MaxHeightMeters * METERS_TO_UU;
	EdgeConfig.IslandShapeStrength = IslandDepth;

	// Create async generator with edge config
	AsyncGenerator = MakeUnique<FLandscapeAsyncGenerator>();
	AsyncGenerator->Initialize(NoiseService.Get(), Resolution, EdgeConfig);
}

// ============================================================================
// Generation
// ============================================================================

void AProceduralLandscapeActor::StartGeneration()
{
	if (bIsGenerating)
	{
		UE_LOG(LogTemp, Warning, TEXT("ProceduralLandscapeActor: Generation already in progress!"));
		return;
	}

	ChunksCreated = 0;

	if (bUseAsyncGeneration)
	{
		bIsGenerating = true;
		
		FVector2D Origin = GetMapOriginMeters();
		AsyncGenerator->QueueAllChunks(
			Resolution.GetChunksPerSide(),
			Origin.X,
			Origin.Y
		);

		if (bDebugShowStats && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
				FString::Printf(TEXT("Starting async generation of %d chunks..."), 
					Resolution.GetTotalChunks()));
		}
	}
	else
	{
		GenerateSynchronous();
	}

	CacheCurrentSettings();
}

void AProceduralLandscapeActor::GenerateSynchronous()
{
	FVector2D Origin = GetMapOriginMeters();
	
	const int32 ChunksPerSide = Resolution.GetChunksPerSide();
	const float ChunkSizeMeters = Resolution.GetChunkSizeMeters();
	const int32 VerticesPerSide = Resolution.GetVerticesPerChunkSide();
	
	// Calculate ACTUAL map size (may differ from requested due to chunk boundaries)
	const float ActualMapSizeMeters = ChunksPerSide * ChunkSizeMeters;
	const float ActualMapSizeUU = ActualMapSizeMeters * METERS_TO_UU;

	// Setup island edge configuration
	FIslandEdgeConfig EdgeConfig;
	EdgeConfig.bEnabled = bIslandMode;
	EdgeConfig.FalloffDistance = EdgeFalloffDistance * METERS_TO_UU;
	EdgeConfig.SeaLevel = SeaLevel * METERS_TO_UU;
	EdgeConfig.SkirtDepth = SkirtDepth * METERS_TO_UU;
	EdgeConfig.FalloffExponent = EdgeFalloffExponent;
	EdgeConfig.MapSizeUU = ActualMapSizeUU;  // Use actual size, not requested
	EdgeConfig.MapCenter = FVector2D(GetActorLocation().X, GetActorLocation().Y);
	EdgeConfig.MaxTerrainHeight = Resolution.MaxHeightMeters * METERS_TO_UU;
	EdgeConfig.IslandShapeStrength = IslandDepth;  // Controls terrain submersion

	for (int32 ChunkY = 0; ChunkY < ChunksPerSide; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < ChunksPerSide; ++ChunkX)
		{
			FLandscapeChunkData ChunkData;
			ChunkData.ChunkX = ChunkX;
			ChunkData.ChunkY = ChunkY;

			const float ChunkWorldX = Origin.X + ChunkX * ChunkSizeMeters;
			const float ChunkWorldY = Origin.Y + ChunkY * ChunkSizeMeters;

			ChunkData.Sections.SetNum(1);
			FLandscapeSectionData& Section = ChunkData.Sections[0];

			Section.LocalX = 0;
			Section.LocalY = 0;
			Section.GlobalX = ChunkX;
			Section.GlobalY = ChunkY;

			// Generate height data
			NoiseService->GenerateHeightMapRegion(
				ChunkWorldX,
				ChunkWorldY,
				ChunkSizeMeters,
				VerticesPerSide,
				Section.HeightMap
			);


			// Determine which edges of this chunk are at map boundary
			FChunkEdgeFlags EdgeFlags;
			EdgeFlags.bIsLeftEdge = (ChunkX == 0);
			EdgeFlags.bIsRightEdge = (ChunkX == ChunksPerSide - 1);
			EdgeFlags.bIsBottomEdge = (ChunkY == 0);
			EdgeFlags.bIsTopEdge = (ChunkY == ChunksPerSide - 1);

			// Build mesh data with edge configuration
			const float ChunkSizeUU = ChunkSizeMeters * METERS_TO_UU;
			const float ChunkWorldXUU = ChunkWorldX * METERS_TO_UU;
			const float ChunkWorldYUU = ChunkWorldY * METERS_TO_UU;

			FLandscapeMeshBuilder::BuildSectionMesh(
				Section,
				ChunkWorldXUU,
				ChunkWorldYUU,
				ChunkSizeUU,
				VerticesPerSide,
				EdgeConfig,
				EdgeFlags
			);


			ChunkData.bIsGenerated = true;
			CreateChunkMesh(ChunkData);
		}
	}
}

void AProceduralLandscapeActor::ProcessCompletedChunks()
{
	if (!AsyncGenerator)
	{
		return;
	}

	TArray<FLandscapeChunkData> CompletedChunks;
	AsyncGenerator->GetCompletedChunks(CompletedChunks);

	for (const FLandscapeChunkData& ChunkData : CompletedChunks)
	{
		CreateChunkMesh(ChunkData);
	}
}

// ============================================================================
// Mesh Creation
// ============================================================================

void AProceduralLandscapeActor::CreateChunkMesh(const FLandscapeChunkData& ChunkData)
{
	if (!ChunkData.bIsGenerated)
	{
		return;
	}

	FIntPoint ChunkCoord(ChunkData.ChunkX, ChunkData.ChunkY);

	// Remove existing mesh if any
	if (UProceduralMeshComponent** ExistingMesh = ChunkMeshes.Find(ChunkCoord))
	{
		if (*ExistingMesh)
		{
			(*ExistingMesh)->DestroyComponent();
		}
	}

	// Create new mesh component
	UProceduralMeshComponent* ChunkMesh = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
	ChunkMesh->SetupAttachment(RootScene);
	ChunkMesh->RegisterComponent();
	ChunkMesh->bUseAsyncCooking = true;

	ChunkMeshes.Add(ChunkCoord, ChunkMesh);

	int32 MeshSectionIndex = 0;

	for (const FLandscapeSectionData& Section : ChunkData.Sections)
	{
		if (!Section.bIsGenerated || Section.Vertices.Num() == 0)
		{
			continue;
		}

		// Create mesh section
		ChunkMesh->CreateMeshSection(
			MeshSectionIndex,
			Section.Vertices,
			Section.Triangles,
			Section.Normals,
			Section.UVs,
			TArray<FColor>(),
			TArray<FProcMeshTangent>(),
			true  // Create collision
		);

		// Apply material
		if (TerrainMaterial)
		{
			ChunkMesh->SetMaterial(MeshSectionIndex, TerrainMaterial);
		}

		++MeshSectionIndex;
	}

	++ChunksCreated;
}

// ============================================================================
// Settings Management
// ============================================================================

bool AProceduralLandscapeActor::HasSettingsChanged() const
{
	if (CachedSeed != TerrainSeed) return true;
	if (!FMath::IsNearlyEqual(CachedNoiseFrequency, NoiseFrequency)) return true;
	if (CachedNoiseOctaves != NoiseOctaves) return true;
	if (!FMath::IsNearlyEqual(CachedHeightExponent, HeightExponent)) return true;
	if (!FMath::IsNearlyEqual(CachedResolution.MapSizeMeters, Resolution.MapSizeMeters)) return true;
	if (!FMath::IsNearlyEqual(CachedResolution.MaxHeightMeters, Resolution.MaxHeightMeters)) return true;
	if (CachedResolution.Resolution != Resolution.Resolution) return true;
	
	// Island settings
	if (CachedIslandMode != bIslandMode) return true;
	if (!FMath::IsNearlyEqual(CachedIslandDepth, IslandDepth)) return true;
	if (!FMath::IsNearlyEqual(CachedEdgeFalloffDistance, EdgeFalloffDistance)) return true;
	if (!FMath::IsNearlyEqual(CachedSeaLevel, SeaLevel)) return true;
	if (!FMath::IsNearlyEqual(CachedSkirtDepth, SkirtDepth)) return true;
	if (!FMath::IsNearlyEqual(CachedEdgeFalloffExponent, EdgeFalloffExponent)) return true;

	return false;
}

void AProceduralLandscapeActor::CacheCurrentSettings()
{
	CachedSeed = TerrainSeed;
	CachedNoiseFrequency = NoiseFrequency;
	CachedNoiseOctaves = NoiseOctaves;
	CachedHeightExponent = HeightExponent;
	CachedResolution = Resolution;
	
	// Island settings
	CachedIslandMode = bIslandMode;
	CachedIslandDepth = IslandDepth;
	CachedEdgeFalloffDistance = EdgeFalloffDistance;
	CachedSeaLevel = SeaLevel;
	CachedSkirtDepth = SkirtDepth;
	CachedEdgeFalloffExponent = EdgeFalloffExponent;
}

FVector2D AProceduralLandscapeActor::GetMapOriginMeters() const
{
	// Calculate ACTUAL map size (may differ from requested due to chunk boundaries)
	const int32 ChunksPerSide = Resolution.GetChunksPerSide();
	const float ChunkSizeMeters = Resolution.GetChunkSizeMeters();
	const float ActualMapSizeMeters = ChunksPerSide * ChunkSizeMeters;
	
	// Center the map on the actor
	float HalfMapSize = ActualMapSizeMeters / 2.0f;
	FVector ActorLocation = GetActorLocation();
	
	return FVector2D(
		(ActorLocation.X / METERS_TO_UU) - HalfMapSize,
		(ActorLocation.Y / METERS_TO_UU) - HalfMapSize
	);
}

// ============================================================================
// Debug
// ============================================================================

void AProceduralLandscapeActor::PrintDebugStats()
{
	if (!GEngine)
	{
		return;
	}

	FString StatusStr = bIsGenerating ? TEXT("GENERATING") : TEXT("IDLE");
	int32 PendingChunks = AsyncGenerator ? AsyncGenerator->GetPendingCount() : 0;

	GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::White,
		FString::Printf(TEXT("Landscape Status: %s"), *StatusStr));
	
	GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::White,
		FString::Printf(TEXT("Chunks: %d/%d (Pending: %d)"), 
			ChunksCreated, Resolution.GetTotalChunks(), PendingChunks));

	GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::White,
		FString::Printf(TEXT("Map: %.0fm x %.0fm | Height: %.0fm"), 
			Resolution.MapSizeMeters, Resolution.MapSizeMeters, Resolution.MaxHeightMeters));

	GEngine->AddOnScreenDebugMessage(4, 0.0f, FColor::White,
		FString::Printf(TEXT("Triangles: %s | Resolution: %d"), 
			*FString::FormatAsNumber(GetTotalTriangleCount()), Resolution.Resolution));

	GEngine->AddOnScreenDebugMessage(5, 0.0f, FColor::White,
		FString::Printf(TEXT("Noise: Freq=%.4f, Octaves=%d, Seed=%d"),
			NoiseFrequency, NoiseOctaves, TerrainSeed));
}
