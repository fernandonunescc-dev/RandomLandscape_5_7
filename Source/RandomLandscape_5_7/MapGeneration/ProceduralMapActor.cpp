// ProceduralMapActor.cpp
// Implementation of the procedural map actor

#include "ProceduralMapActor.h"
#include "MapGeneratorBase.h"
#include "MapGeneratorFactory.h"
#include "ContinentMapGenerator.h"
#include "Engine/Texture2D.h"
#include "BiomeTerrainGenerators/BiomeTerrainGeneratorFactory.h"

AProceduralMapActor::AProceduralMapActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create a default scene root
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Create the procedural mesh component for terrain
	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	TerrainMesh->SetupAttachment(SceneRoot);
	TerrainMesh->bUseAsyncCooking = true;
}

void AProceduralMapActor::BeginPlay()
{
	Super::BeginPlay();
}

#if WITH_EDITOR
void AProceduralMapActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// When landmass seed or percentage changes, regenerate landmass and biomes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMapActor, Seed) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMapActor, LandmassPercentage))
	{
		// Store current biome seed (preserve it across landmass regeneration)
		int32 CurrentBiomeSeed = BiomeSeed;
		
		GenerateLandmass();
		
		// Restore biome seed and regenerate biomes
		BiomeSeed = CurrentBiomeSeed;
		if (CurrentGenerator)
		{
			GenerateBiomes();
		}
	}
	// When biome seed changes, regenerate only biomes (keep same landmass)
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AProceduralMapActor, BiomeSeed))
	{
		if (CurrentGenerator)
		{
			GenerateBiomes();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ProceduralMapActor::PostEditChangeProperty - No landmass generated yet. Generate landmass first."));
		}
	}
}
#endif

void AProceduralMapActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AProceduralMapActor::GenerateMap()
{
	// Start total timing
	double TotalStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMap - Running full map generation"));

	// Reset durations
	TotalDuration = 0.0f;
	LandmassDuration = 0.0f;
	BiomeDuration = 0.0f;

	// Generate the landmass first (this sets LandmassDuration)
	GenerateLandmass();

	// Generate biomes on top of the landmass (this sets BiomeDuration)
	GenerateBiomes();

	// TODO: Future steps will go here (terrain mesh, etc.)
	
	// End total timing
	double TotalEndTime = FPlatformTime::Seconds();
	TotalDuration = static_cast<float>(TotalEndTime - TotalStartTime);

	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMap - Complete (Total: %.4f sec, Landmass: %.4f sec, Biomes: %.4f sec)"), 
		TotalDuration, LandmassDuration, BiomeDuration);
}

void AProceduralMapActor::GenerateLandmass()
{
	// Start landmass timing
	double LandmassStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateLandmass - Generating landmass texture"));

	// Reset landmass duration
	LandmassDuration = 0.0f;

	// Clear any existing preview
	PreviewTexture = nullptr;
	LandmassTexture = nullptr;
	BiomeTexture = nullptr;

	// Create generator
	CurrentGenerator = FMapGeneratorFactory::CreateGenerator(this, MapType);
	if (!CurrentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateLandmass - Failed to create generator"));
		return;
	}

	// If this is a continent generator, pass biome settings and seed
	if (MapType == EMapType::Continent)
	{
		UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
		if (ContinentGenerator)
		{
			// If Seed is 0, generate a random seed and store it so subsequent calls use the same seed
			int32 SeedToUse = Seed;
			if (SeedToUse == 0)
			{
				SeedToUse = FMath::RandRange(1, TNumericLimits<int32>::Max());
				Seed = SeedToUse; // Store it back so regenerating uses the same seed
				UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateLandmass - Generated random landmass seed: %d"), SeedToUse);
			}

			// Copy biome settings, but override land coverage with the user-visible LandmassPercentage
			FContinentBiomeSettings SettingsToUse = BiomeSettings;
			SettingsToUse.LandCoveragePercent = LandmassPercentage;
			ContinentGenerator->SetBiomeSettings(SettingsToUse);
			ContinentGenerator->SetSeed(SeedToUse);
		}
	}

	// Initialize and generate only the landmass
	FMapGenerationSettings Settings = CreateSettings();
	CurrentGenerator->Initialize(Settings);

	if (MapType == EMapType::Continent)
	{
		UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
		if (ContinentGenerator && ContinentGenerator->GenerateLandmassOnly())
		{
			UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateLandmass - Landmass generation successful"));

			// Get the preview texture and expose it as the LandmassTexture property
			UTexture2D* GenTex = ContinentGenerator->GetPreviewTexture();
			if (GenTex)
			{
				PreviewTexture = GenTex;
				LandmassTexture = GenTex;
				UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateLandmass - Assigned generated texture to LandmassTexture"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("ProceduralMapActor::GenerateLandmass - Generator produced no preview texture"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateLandmass - Landmass generation failed"));
		}
	}

	// End landmass timing
	double LandmassEndTime = FPlatformTime::Seconds();
	LandmassDuration = static_cast<float>(LandmassEndTime - LandmassStartTime);

	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateLandmass - Complete (%.4f sec)"), LandmassDuration);
}

void AProceduralMapActor::GenerateBiomes()
{
	// Start biome timing
	double BiomeStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomes - Generating biome distribution"));

	// Reset biome duration
	BiomeDuration = 0.0f;

	// Check if we have a valid generator with landmass data
	if (!CurrentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomes - No generator. Generate landmass first."));
		return;
	}

	UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
	if (!ContinentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomes - Generator is not a ContinentMapGenerator"));
		return;
	}

	// If BiomeSeed is 0, generate a random seed and store it so subsequent calls use the same seed
	int32 SeedToUse = BiomeSeed;
	if (SeedToUse == 0)
	{
		SeedToUse = FMath::RandRange(1, TNumericLimits<int32>::Max());
		BiomeSeed = SeedToUse; // Store it back so regenerating uses the same seed
		UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomes - Generated random biome seed: %d"), SeedToUse);
	}

	// Update biome settings and seed
	FContinentBiomeSettings SettingsToUse = BiomeSettings;
	SettingsToUse.LandCoveragePercent = LandmassPercentage;
	ContinentGenerator->SetBiomeSettings(SettingsToUse);
	ContinentGenerator->SetBiomeSeed(SeedToUse);

	// Generate biomes on the existing landmass
	if (ContinentGenerator->GenerateBiomesOnly())
	{
		UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomes - Biome generation successful"));

		// Get the preview texture with biome colors
		UTexture2D* GenTex = ContinentGenerator->GetPreviewTexture();
		if (GenTex)
		{
			PreviewTexture = GenTex;
			BiomeTexture = GenTex;
			UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomes - Assigned generated texture to BiomeTexture"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ProceduralMapActor::GenerateBiomes - Generator produced no preview texture"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomes - Biome generation failed"));
	}

	// End biome timing
	double BiomeEndTime = FPlatformTime::Seconds();
	BiomeDuration = static_cast<float>(BiomeEndTime - BiomeStartTime);

	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomes - Complete (%.4f sec)"), BiomeDuration);
}


void AProceduralMapActor::ClearMap()
{
	if (CurrentGenerator)
	{
		CurrentGenerator = nullptr;
	}
	
	PreviewTexture = nullptr;
	LandmassTexture = nullptr;
	BiomeTexture = nullptr;
	
	// Clear the terrain mesh
	if (TerrainMesh)
	{
		TerrainMesh->ClearAllMeshSections();
	}
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::ClearMap - Map cleared"));
}

UTexture2D* AProceduralMapActor::GetPreviewTexture() const
{
	return PreviewTexture;
}

void AProceduralMapActor::NormalizeBiomePercentages()
{
	BiomeSettings.NormalizePercentages();
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::NormalizeBiomePercentages - Percentages normalized to 100%%"));
}

FMapGenerationSettings AProceduralMapActor::CreateSettings() const
{
	FMapGenerationSettings Settings;
	Settings.MapType = MapType;
	Settings.MapSizeInMeters = MapSizeInMeters;
	Settings.MapResolution = MapResolution;
	return Settings;
}

float AProceduralMapActor::CalculateTerrainHeight(float NormX, float NormY, const FBiomeConfig& BiomeConfig) const
{
	// Delegate to the biome-specific terrain generator via factory
	// Pass MapSizeInMeters for proper frequency scaling
	return FBiomeTerrainGeneratorFactory::Get().CalculateHeightForBiome(
		BiomeConfig.BiomeType, NormX, NormY, BiomeConfig, Seed, static_cast<float>(MapSizeInMeters));
}

float AProceduralMapActor::CalculateBlendedTerrainHeight(float NormX, float NormY, int32 TextureRes,
	const TArray<int32>& BiomeMap, const TArray<bool>& LandMask) const
{
	// Clamp coordinates to valid range
	NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
	NormY = FMath::Clamp(NormY, 0.0f, 1.0f);
	
	// Calculate float position in biome map
	float BiomeX = NormX * (TextureRes - 1);
	float BiomeY = NormY * (TextureRes - 1);
	
	// Get integer coordinates and fractional parts for bilinear interpolation
	int32 X0 = FMath::FloorToInt(BiomeX);
	int32 Y0 = FMath::FloorToInt(BiomeY);
	int32 X1 = FMath::Min(X0 + 1, TextureRes - 1);
	int32 Y1 = FMath::Min(Y0 + 1, TextureRes - 1);
	
	float FracX = BiomeX - X0;
	float FracY = BiomeY - Y0;
	
	// Apply smoothstep for smoother interpolation
	FracX = FracX * FracX * (3.0f - 2.0f * FracX);
	FracY = FracY * FracY * (3.0f - 2.0f * FracY);
	
	// Sample heights at 4 corners
	auto GetHeightAtPixel = [&](int32 PX, int32 PY) -> float
	{
		int32 Index = PY * TextureRes + PX;
		if (Index < 0 || Index >= LandMask.Num())
		{
			return -50.0f; // Ocean depth
		}
		
		bool bIsLand = LandMask[Index];
		int32 BiomeIndex = (Index < BiomeMap.Num()) ? BiomeMap[Index] : -1;
		
		// Calculate normalized position for this pixel
		float PixelNormX = static_cast<float>(PX) / static_cast<float>(TextureRes - 1);
		float PixelNormY = static_cast<float>(PY) / static_cast<float>(TextureRes - 1);
		
		if (bIsLand && BiomeIndex >= 0 && BiomeIndex < BiomeSettings.LandBiomes.Num())
		{
			const FBiomeConfig& BiomeConfig = BiomeSettings.LandBiomes[BiomeIndex];
			return CalculateTerrainHeight(PixelNormX, PixelNormY, BiomeConfig);
		}
		else if (bIsLand)
		{
			FBiomeConfig DefaultConfig;
			return CalculateTerrainHeight(PixelNormX, PixelNormY, DefaultConfig);
		}
		else
		{
			return -50.0f; // Ocean
		}
	};
	
	// Get heights at 4 corners
	float H00 = GetHeightAtPixel(X0, Y0);
	float H10 = GetHeightAtPixel(X1, Y0);
	float H01 = GetHeightAtPixel(X0, Y1);
	float H11 = GetHeightAtPixel(X1, Y1);
	
	// Bilinear interpolation
	float H0 = FMath::Lerp(H00, H10, FracX);
	float H1 = FMath::Lerp(H01, H11, FracX);
	
	return FMath::Lerp(H0, H1, FracY);
}

FColor AProceduralMapActor::GetBlendedBiomeColor(float NormX, float NormY, int32 TextureRes,
	const TArray<int32>& BiomeMap, const TArray<bool>& LandMask) const
{
	// Clamp coordinates to valid range
	NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
	NormY = FMath::Clamp(NormY, 0.0f, 1.0f);
	
	// Calculate float position in biome map
	float BiomeX = NormX * (TextureRes - 1);
	float BiomeY = NormY * (TextureRes - 1);
	
	// Get integer coordinates and fractional parts for bilinear interpolation
	int32 X0 = FMath::FloorToInt(BiomeX);
	int32 Y0 = FMath::FloorToInt(BiomeY);
	int32 X1 = FMath::Min(X0 + 1, TextureRes - 1);
	int32 Y1 = FMath::Min(Y0 + 1, TextureRes - 1);
	
	float FracX = BiomeX - X0;
	float FracY = BiomeY - Y0;
	
	// Apply smoothstep for smoother interpolation
	FracX = FracX * FracX * (3.0f - 2.0f * FracX);
	FracY = FracY * FracY * (3.0f - 2.0f * FracY);
	
	// Sample colors at 4 corners
	auto GetColorAtPixel = [&](int32 PX, int32 PY) -> FLinearColor
	{
		int32 Index = PY * TextureRes + PX;
		if (Index < 0 || Index >= LandMask.Num())
		{
			return BiomeSettings.OceanColor;
		}
		
		bool bIsLand = LandMask[Index];
		int32 BiomeIndex = (Index < BiomeMap.Num()) ? BiomeMap[Index] : -1;
		
		if (bIsLand && BiomeIndex >= 0 && BiomeIndex < BiomeSettings.LandBiomes.Num())
		{
			const FBiomeConfig& BiomeConfig = BiomeSettings.LandBiomes[BiomeIndex];
			if (BiomeConfig.bHighlightColor)
			{
				return BiomeConfig.Color;
			}
			return FLinearColor::White;
		}
		else if (bIsLand)
		{
			return FLinearColor::White;
		}
		else
		{
			if (BiomeSettings.bHighlightOcean)
			{
				return BiomeSettings.OceanColor;
			}
			return FLinearColor::White;
		}
	};
	
	// Get colors at 4 corners
	FLinearColor C00 = GetColorAtPixel(X0, Y0);
	FLinearColor C10 = GetColorAtPixel(X1, Y0);
	FLinearColor C01 = GetColorAtPixel(X0, Y1);
	FLinearColor C11 = GetColorAtPixel(X1, Y1);
	
	// Bilinear interpolation
	FLinearColor C0 = FMath::Lerp(C00, C10, FracX);
	FLinearColor C1 = FMath::Lerp(C01, C11, FracX);
	FLinearColor Blended = FMath::Lerp(C0, C1, FracY);
	
	return Blended.ToFColor(false);
}

void AProceduralMapActor::GenerateTerrainMesh(UContinentMapGenerator* Generator)
{
	// Mesh creation disabled (temporary safe-guard).
	UE_LOG(LogTemp, Log, TEXT("GenerateTerrainMesh - Mesh creation disabled by configuration"));
	return;

	/* Original implementation commented out while mesh creation is disabled

	if (!Generator || !TerrainMesh)
	{
		return;
	}
	
	// Initialize random stream
	TerrainRandomStream.Initialize(Seed != 0 ? Seed : FMath::Rand());
	
	const TArray<int32>& BiomeMap = Generator->GetBiomeMap();
	const TArray<bool>& LandMask = Generator->GetLandMask();
	int32 TextureRes = Generator->GetTextureResolution();
	
	if (BiomeMap.Num() == 0 || LandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateTerrainMesh - No biome map data"));
		return;
	}
	
	// Get map size in Unreal Units (same for X and Y since it's a square map)
	float MapSizeUU = GetMapSizeInUnrealUnits();
	float ChunkSizeUU = MapSizeUU / static_cast<float>(ChunksPerSide);
	
	int32 TotalChunks = ChunksPerSide * ChunksPerSide;
	int32 TotalVertices = 0;
	int32 TotalTriangles = 0;
	
	// Generate each chunk as a separate mesh section
	for (int32 ChunkY = 0; ChunkY < ChunksPerSide; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < ChunksPerSide; ++ChunkX)
		{
			int32 ChunkIndex = ChunkY * ChunksPerSide + ChunkX;
			
			// Calculate the normalized coordinate range for this chunk
			float ChunkNormStartX = static_cast<float>(ChunkX) / static_cast<float>(ChunksPerSide);
			float ChunkNormEndX = static_cast<float>(ChunkX + 1) / static_cast<float>(ChunksPerSide);
			float ChunkNormStartY = static_cast<float>(ChunkY) / static_cast<float>(ChunksPerSide);
			float ChunkNormEndY = static_cast<float>(ChunkY + 1) / static_cast<float>(ChunksPerSide);
			
			int32 VertsPerSide = VerticesPerChunkSide;
			int32 NumVertices = VertsPerSide * VertsPerSide;
			int32 NumTriangles = (VertsPerSide - 1) * (VertsPerSide - 1) * 2;
			
			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UVs;
			TArray<FColor> VertexColors;
			TArray<FProcMeshTangent> Tangents;
			
			Vertices.Reserve(NumVertices);
			UVs.Reserve(NumVertices);
			VertexColors.Reserve(NumVertices);
			Triangles.Reserve(NumTriangles * 3);
			
			// Generate vertices for this chunk
			for (int32 Y = 0; Y < VertsPerSide; ++Y)
			{
				for (int32 X = 0; X < VertsPerSide; ++X)
				{
					// Local normalized position within chunk (0-1)
					float LocalNormX = static_cast<float>(X) / static_cast<float>(VertsPerSide - 1);
					float LocalNormY = static_cast<float>(Y) / static_cast<float>(VertsPerSide - 1);
					
					// Global normalized position on entire map (0-1)
					float GlobalNormX = FMath::Lerp(ChunkNormStartX, ChunkNormEndX, LocalNormX);
					float GlobalNormY = FMath::Lerp(ChunkNormStartY, ChunkNormEndY, LocalNormY);
					
					// Use bilinear interpolation for smooth height blending between biomes
					float Height = CalculateBlendedTerrainHeight(GlobalNormX, GlobalNormY, TextureRes, BiomeMap, LandMask);
					
					// Get smoothly blended vertex color
					FColor VertColor = GetBlendedBiomeColor(GlobalNormX, GlobalNormY, TextureRes, BiomeMap, LandMask);
					
					// Position in world space (centered on actor)
					FVector Position(
						(GlobalNormX - 0.5f) * MapSizeUU,
						(GlobalNormY - 0.5f) * MapSizeUU,
						Height
					);
					
					Vertices.Add(Position);
					UVs.Add(FVector2D(GlobalNormX, GlobalNormY));
					VertexColors.Add(VertColor);
				}
			}
			
			// Generate triangles for this chunk
			for (int32 Y = 0; Y < VertsPerSide - 1; ++Y)
			{
				for (int32 X = 0; X < VertsPerSide - 1; ++X)
				{
					int32 TopLeft = Y * VertsPerSide + X;
					int32 TopRight = TopLeft + 1;
					int32 BottomLeft = (Y + 1) * VertsPerSide + X;
					int32 BottomRight = BottomLeft + 1;
					
					// First triangle
					Triangles.Add(TopLeft);
					Triangles.Add(BottomLeft);
					Triangles.Add(TopRight);
					
					// Second triangle
					Triangles.Add(TopRight);
					Triangles.Add(BottomLeft);
					Triangles.Add(BottomRight);
				}
			}
			
			// Calculate normals and tangents for this chunk
			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
			
			// Create the mesh section for this chunk
			TerrainMesh->CreateMeshSection(ChunkIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
			
			TotalVertices += Vertices.Num();
			TotalTriangles += Triangles.Num() / 3;
		}
	}
	
	// Apply material to all chunks
	if (TerrainMaterial)
	{
		for (int32 i = 0; i < TotalChunks; ++i)
		{
			TerrainMesh->SetMaterial(i, TerrainMaterial);
		}
	}
	else
	{
		// Try to load a default vertex color material
		UMaterialInterface* DefaultMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_VertexColor.M_VertexColor"));
		if (DefaultMaterial)
		{
			for (int32 i = 0; i < TotalChunks; ++i)
			{
				TerrainMesh->SetMaterial(i, DefaultMaterial);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateTerrainMesh - No terrain material set. Create a material that uses Vertex Color node and assign it to TerrainMaterial."));
		}
	}
	
	// Calculate effective resolution
	int32 EffectiveVertsPerSide = ChunksPerSide * (VerticesPerChunkSide - 1) + 1;
	float MetersPerVertex = static_cast<float>(MapSizeInMeters) / static_cast<float>(EffectiveVertsPerSide - 1);
	
	UE_LOG(LogTemp, Log, TEXT("GenerateTerrainMesh - Created %d chunks with %d total vertices, %d triangles. Resolution: ~%.2f meters per vertex"),
		TotalChunks, TotalVertices, TotalTriangles, MetersPerVertex);

	*/
}
