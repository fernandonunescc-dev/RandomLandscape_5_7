// ProceduralMapActor.cpp
// Implementation of the procedural map actor

#include "ProceduralMapActor.h"
#include "MapGeneratorBase.h"
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

void AProceduralMapActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AProceduralMapActor::GenerateMap()
{
	// Mesh generation intentionally disabled (temporary).
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMap - Mesh generation disabled by configuration"));
	return;

	/* Original implementation commented out while generation is disabled

	// Clear any existing map
	ClearMap();

	// Create the appropriate generator based on map type
	CurrentGenerator = FMapGeneratorFactory::CreateGenerator(this, MapType);
	
	if (!CurrentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateMap - Failed to create generator"));
		return;
	}

	// If this is a continent generator, pass the biome settings and seed
	if (MapType == EMapType::Continent)
	{
		UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
		if (ContinentGenerator)
		{
			ContinentGenerator->SetBiomeSettings(ContinentBiomeSettings);
			// Pass the seed - 0 means "generate random", non-zero means use that exact seed
			ContinentGenerator->SetSeed(Seed);
		}
	}

	// Initialize and generate
	FMapGenerationSettings Settings = CreateSettings();
	CurrentGenerator->Initialize(Settings);
	
	if (CurrentGenerator->Generate())
	{
		UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMap - Map generation successful"));

		// Retrieve the preview texture from the generator
		if (MapType == EMapType::Continent)
		{
			UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
			if (ContinentGenerator)
			{
				PreviewTexture = ContinentGenerator->GetPreviewTexture();
				
				// Generate the 3D terrain mesh
				GenerateTerrainMesh(ContinentGenerator);
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateMap - Map generation failed"));
	}

	*/
}

void AProceduralMapActor::ClearMap()
{
	if (CurrentGenerator)
	{
		CurrentGenerator = nullptr;
	}
	
	PreviewTexture = nullptr;
	
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
	ContinentBiomeSettings.NormalizePercentages();
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
		
		if (bIsLand && BiomeIndex >= 0 && BiomeIndex < ContinentBiomeSettings.LandBiomes.Num())
		{
			const FBiomeConfig& BiomeConfig = ContinentBiomeSettings.LandBiomes[BiomeIndex];
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
			return ContinentBiomeSettings.OceanColor;
		}
		
		bool bIsLand = LandMask[Index];
		int32 BiomeIndex = (Index < BiomeMap.Num()) ? BiomeMap[Index] : -1;
		
		if (bIsLand && BiomeIndex >= 0 && BiomeIndex < ContinentBiomeSettings.LandBiomes.Num())
		{
			const FBiomeConfig& BiomeConfig = ContinentBiomeSettings.LandBiomes[BiomeIndex];
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
			if (ContinentBiomeSettings.bHighlightOcean)
			{
				return ContinentBiomeSettings.OceanColor;
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
