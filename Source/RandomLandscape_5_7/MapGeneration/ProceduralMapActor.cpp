// ProceduralMapActor.cpp
// Implementation of the procedural map actor

#include "ProceduralMapActor.h"
#include "MapGeneratorBase.h"
#include "MapGeneratorFactory.h"
#include "ContinentMapGenerator.h"
#include "Engine/Texture2D.h"
#include "KismetProceduralMeshLibrary.h"

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
	// Fractal Perlin noise implementation using per-biome settings
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = BiomeConfig.NoiseFrequency;
	float MaxValue = 0.0f;
	
	// Use seed to offset the noise
	float SeedOffsetX = (Seed % 10000) * 0.37f;
	float SeedOffsetY = (Seed % 10000) * 0.53f;
	
	for (int32 i = 0; i < BiomeConfig.NoiseOctaves; ++i)
	{
		// Simple hash-based noise (same as in ContinentMapGenerator)
		float X = (NormX + SeedOffsetX) * Frequency;
		float Y = (NormY + SeedOffsetY) * Frequency;
		
		int32 Xi = FMath::FloorToInt(X);
		int32 Yi = FMath::FloorToInt(Y);
		float Xf = X - Xi;
		float Yf = Y - Yi;
		
		// Smooth interpolation
		float U = Xf * Xf * (3.0f - 2.0f * Xf);
		float V = Yf * Yf * (3.0f - 2.0f * Yf);
		
		// Hash function
		auto Hash = [this](int32 X, int32 Y) -> float
		{
			int32 N = X + Y * 57 + Seed;
			N = (N << 13) ^ N;
			return (1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
		};
		
		float A = Hash(Xi, Yi);
		float B = Hash(Xi + 1, Yi);
		float C = Hash(Xi, Yi + 1);
		float D = Hash(Xi + 1, Yi + 1);
		
		float AB = FMath::Lerp(A, B, U);
		float CD = FMath::Lerp(C, D, U);
		float NoiseValue = FMath::Lerp(AB, CD, V);
		
		Total += NoiseValue * Amplitude;
		MaxValue += Amplitude;
		
		Amplitude *= BiomeConfig.NoisePersistence;
		Frequency *= 2.0f;
	}
	
	// Normalize to 0-1 range, apply base height and height multiplier
	float NormalizedNoise = (Total / MaxValue + 1.0f) * 0.5f;
	return BiomeConfig.BaseHeight + (NormalizedNoise * BiomeConfig.HeightMultiplier);
}

void AProceduralMapActor::GenerateTerrainMesh(UContinentMapGenerator* Generator)
{
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
	
	// Get map size in Unreal Units
	FVector MapSizeUU = GetMapSizeInUnrealUnits();
	
	int32 VerticesPerSide = TerrainVerticesPerSide;
	int32 NumVertices = VerticesPerSide * VerticesPerSide;
	int32 NumTriangles = (VerticesPerSide - 1) * (VerticesPerSide - 1) * 2;
	
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
	
	// Generate vertices
	for (int32 Y = 0; Y < VerticesPerSide; ++Y)
	{
		for (int32 X = 0; X < VerticesPerSide; ++X)
		{
			float NormX = static_cast<float>(X) / static_cast<float>(VerticesPerSide - 1);
			float NormY = static_cast<float>(Y) / static_cast<float>(VerticesPerSide - 1);
			
			// Sample biome map at this position
			int32 BiomeMapX = FMath::Clamp(FMath::FloorToInt(NormX * TextureRes), 0, TextureRes - 1);
			int32 BiomeMapY = FMath::Clamp(FMath::FloorToInt(NormY * TextureRes), 0, TextureRes - 1);
			int32 BiomeMapIndex = BiomeMapY * TextureRes + BiomeMapX;
			
			bool bIsLand = (BiomeMapIndex < LandMask.Num()) ? LandMask[BiomeMapIndex] : false;
			int32 BiomeIndex = (BiomeMapIndex < BiomeMap.Num()) ? BiomeMap[BiomeMapIndex] : -1;
			
			// Calculate height
			float Height = 0.0f;
			FColor VertColor;
			
			if (bIsLand && BiomeIndex >= 0 && BiomeIndex < ContinentBiomeSettings.LandBiomes.Num())
			{
				// Land - use Perlin noise for height with biome-specific settings
				const FBiomeConfig& BiomeConfig = ContinentBiomeSettings.LandBiomes[BiomeIndex];
				Height = CalculateTerrainHeight(NormX, NormY, BiomeConfig) * MapSizeUU.Z;
				
				// Get biome color - use false to keep in linear space (GPU handles gamma)
				VertColor = BiomeConfig.Color.ToFColor(false);
			}
			else if (bIsLand)
			{
				// Land but invalid biome index - use default
				FBiomeConfig DefaultConfig;
				Height = CalculateTerrainHeight(NormX, NormY, DefaultConfig) * MapSizeUU.Z;
				VertColor = FColor::Green;
			}
			else
			{
				// Ocean - flat at 0 height (or slightly below)
				Height = -MapSizeUU.Z * 0.05f; // Slightly below land
				VertColor = ContinentBiomeSettings.OceanColor.ToFColor(false);
			}
			
			// Position in world space (centered on actor)
			FVector Position(
				(NormX - 0.5f) * MapSizeUU.X,
				(NormY - 0.5f) * MapSizeUU.Y,
				Height
			);
			
			Vertices.Add(Position);
			UVs.Add(FVector2D(NormX, NormY));
			VertexColors.Add(VertColor);
		}
	}
	
	// Generate triangles
	for (int32 Y = 0; Y < VerticesPerSide - 1; ++Y)
	{
		for (int32 X = 0; X < VerticesPerSide - 1; ++X)
		{
			int32 TopLeft = Y * VerticesPerSide + X;
			int32 TopRight = TopLeft + 1;
			int32 BottomLeft = (Y + 1) * VerticesPerSide + X;
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
	
	// Calculate normals and tangents
	UKismetProceduralMeshLibrary::CalculateTangentsForMesh(Vertices, Triangles, UVs, Normals, Tangents);
	
	// Create the mesh section with vertex colors
	TerrainMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);
	
	// Apply material
	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}
	else
	{
		// Try to load a default vertex color material
		UMaterialInterface* DefaultMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_VertexColor.M_VertexColor"));
		if (DefaultMaterial)
		{
			TerrainMesh->SetMaterial(0, DefaultMaterial);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateTerrainMesh - No terrain material set. Create a material that uses Vertex Color node and assign it to TerrainMaterial."));
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("GenerateTerrainMesh - Created mesh with %d vertices, %d triangles"),
		Vertices.Num(), Triangles.Num() / 3);
}

