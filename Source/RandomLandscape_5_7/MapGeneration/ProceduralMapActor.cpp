// ProceduralMapActor.cpp
// Implementation of the procedural map actor

#include "ProceduralMapActor.h"
#include "MapGeneratorBase.h"
#include "MapGeneratorFactory.h"
#include "ContinentMapGenerator.h"
#include "Engine/Texture2D.h"
#include "BiomeTerrainGenerators/BiomeTerrainGeneratorFactory.h"
#include "BiomeHeightMapGenerator.h"
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

	// Initialize biome noise settings with defaults (frequency, octaves, persistence)
	// Frequency is step size per pixel - 0.02 gives good variation at 4096 resolution
	OceanMeshSettings = FBiomeMeshSettings(EBiomeType::Ocean, TEXT("Ocean"), 0.02f, 3, 0.5f);
	ForestMeshSettings = FBiomeMeshSettings(EBiomeType::Forest, TEXT("Forest"), 0.5f, 4, 0.3f);
	MountainMeshSettings = FBiomeMeshSettings(EBiomeType::Mountain, TEXT("Mountain"), 1.5f, 5, 0.55f);
	DesertMeshSettings = FBiomeMeshSettings(EBiomeType::Desert, TEXT("Desert"), 0.8f, 3, 0.25f);
	SnowMeshSettings = FBiomeMeshSettings(EBiomeType::Snow, TEXT("Snow"), 1.2f, 4, 0.45f);
	VolcanicMeshSettings = FBiomeMeshSettings(EBiomeType::Volcanic, TEXT("Volcanic"), 2.0f, 5, 0.6f);
}

void AProceduralMapActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Automatically generate the map when the game starts
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::BeginPlay - Auto-generating map"));
	
	// Generate landmass first
	GenerateLandmass();
	
	// Generate biome colors on top of landmass
	GenerateBiomes();
	
	// Generate all biome textures (color, low-res, and high-res)
	GenerateBiomeTextures();
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::BeginPlay - Auto-generation complete (Landmass: %.4f sec, Biomes: %.4f sec, Textures: %.4f sec)"),
		LandmassDuration, BiomeDuration, MeshTexturesDuration);
}

void AProceduralMapActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
#if WITH_EDITOR
	// Only auto-generate in editor if we don't already have textures
	// This prevents regeneration every time a property changes
	if (!LandmassTexture || !BiomeTexture || !CombinedHeightMapTexture)
	{
		UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::OnConstruction - Auto-generating map in editor"));
		
		// Generate landmass first
		GenerateLandmass();
		
		// Generate biome colors on top of landmass
		GenerateBiomes();
		
		// Generate all biome textures (color, low-res, and high-res)
		GenerateBiomeTextures();
		
		UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::OnConstruction - Auto-generation complete (Landmass: %.4f sec, Biomes: %.4f sec, Textures: %.4f sec)"),
			LandmassDuration, BiomeDuration, MeshTexturesDuration);
	}
#endif
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
	MeshTexturesDuration = 0.0f;
	MeshGenerationDuration = 0.0f;

	// Generate the landmass first (this sets LandmassDuration)
	GenerateLandmass();

	// Generate biomes on top of the landmass (this sets BiomeDuration)
	GenerateBiomes();

	// Generate all biome textures (color, low-res, and high-res)
	GenerateBiomeTextures();

	// Generate the terrain mesh (this sets MeshGenerationDuration)
	GenerateMesh();
	
	// End total timing
	double TotalEndTime = FPlatformTime::Seconds();
	TotalDuration = static_cast<float>(TotalEndTime - TotalStartTime);

	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMap - Complete (Total: %.4f sec, Landmass: %.4f sec, Biomes: %.4f sec, Textures: %.4f sec, Mesh: %.4f sec)"), 
		TotalDuration, LandmassDuration, BiomeDuration, MeshTexturesDuration, MeshGenerationDuration);
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

void AProceduralMapActor::GenerateMesh()
{
	double MeshStartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMesh - Generating terrain mesh (Detail Level: %d)"), MeshDetailLevel);
	
	if (!CurrentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateMesh - No generator. Generate landmass and biomes first."));
		MeshGenerationDuration = 0.0f;
		return;
	}
	
	UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
	if (ContinentGenerator)
	{
		GenerateTerrainMesh(ContinentGenerator);
	}
	
	double MeshEndTime = FPlatformTime::Seconds();
	MeshGenerationDuration = static_cast<float>(MeshEndTime - MeshStartTime);
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMesh - Complete (%.4f sec)"), MeshGenerationDuration);
}

void AProceduralMapActor::GenerateBiomeMaskTexture(EBiomeType BiomeType)
{
	if (!CurrentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomeMaskTexture - No generator. Generate landmass and biomes first."));
		return;
	}

	UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
	if (!ContinentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomeMaskTexture - Generator is not a ContinentMapGenerator"));
		return;
	}

	const TArray<int32>& BiomeMap = ContinentGenerator->GetBiomeMap();
	const TArray<bool>& LandMask = ContinentGenerator->GetLandMask();
	int32 BiomeTextureRes = ContinentGenerator->GetTextureResolution(); // Biome assignment resolution (e.g., 512)

	if (BiomeMap.Num() == 0 || LandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomeMaskTexture - No biome data. Generate biomes first."));
		return;
	}

	// Get the mesh settings for this biome
	FBiomeMeshSettings* MeshSettingsForBiome = GetMeshSettingsForBiome(BiomeType);
	if (!MeshSettingsForBiome)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomeMaskTexture - No mesh settings for biome type %d"), (int32)BiomeType);
		return;
	}
	
	// If seed is 0, generate a random one and store it
	if (MeshSettingsForBiome->Seed == 0)
	{
		MeshSettingsForBiome->Seed = FMath::RandRange(1, TNumericLimits<int32>::Max());
		UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomeMaskTexture - Generated random seed %d for %s"),
			MeshSettingsForBiome->Seed, *MeshSettingsForBiome->DisplayName);
	}
	
	// Create the MASK texture at biome texture resolution (for biome boundaries)
	UTexture2D* MaskTexture = UTexture2D::CreateTransient(BiomeTextureRes, BiomeTextureRes, PF_B8G8R8A8);
	if (!MaskTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateBiomeMaskTexture - Failed to create texture for %s"), *MeshSettingsForBiome->DisplayName);
		return;
	}
	
	MaskTexture->MipGenSettings = TMGS_NoMipmaps;
	MaskTexture->SRGB = false;
	MaskTexture->Filter = TF_Nearest;

	FTexture2DMipMap& Mip = MaskTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(TextureData);

	// Get the color for this biome
	FColor BiomeColor = FColor::White;
	if (BiomeType == EBiomeType::Ocean)
	{
		BiomeColor = BiomeSettings.OceanColor.ToFColor(false);
	}
	else
	{
		// Find the biome config to get the color
		for (const FBiomeConfig& Config : BiomeSettings.LandBiomes)
		{
			if (Config.BiomeType == BiomeType)
			{
				BiomeColor = Config.Color.ToFColor(false);
				break;
			}
		}
	}

	// Find the biome index for land biomes
	int32 TargetBiomeIndex = -1;
	if (BiomeType != EBiomeType::Ocean)
	{
		for (int32 i = 0; i < BiomeSettings.LandBiomes.Num(); ++i)
		{
			if (BiomeSettings.LandBiomes[i].BiomeType == BiomeType)
			{
				TargetBiomeIndex = i;
				break;
			}
		}
	}

	for (int32 Y = 0; Y < BiomeTextureRes; ++Y)
	{
		for (int32 X = 0; X < BiomeTextureRes; ++X)
		{
			const int32 PixelIndex = (Y * BiomeTextureRes + X) * 4;
			const int32 MapIndex = Y * BiomeTextureRes + X;

			bool bIsThisBiome = false;

			if (BiomeType == EBiomeType::Ocean)
			{
				// Ocean is where it's not land
				bIsThisBiome = (MapIndex < LandMask.Num()) && !LandMask[MapIndex];
			}
			else
			{
				// Land biome - check if this pixel matches the target biome index
				if (MapIndex < BiomeMap.Num() && MapIndex < LandMask.Num())
				{
					bIsThisBiome = LandMask[MapIndex] && (BiomeMap[MapIndex] == TargetBiomeIndex);
				}
			}

			if (bIsThisBiome)
			{
				Pixels[PixelIndex + 0] = BiomeColor.B;
				Pixels[PixelIndex + 1] = BiomeColor.G;
				Pixels[PixelIndex + 2] = BiomeColor.R;
				Pixels[PixelIndex + 3] = 255;
			}
			else
			{
				Pixels[PixelIndex + 0] = 0;
				Pixels[PixelIndex + 1] = 0;
				Pixels[PixelIndex + 2] = 0;
				Pixels[PixelIndex + 3] = 255;
			}
		}
	}

	Mip.BulkData.Unlock();
	MaskTexture->UpdateResource();

	MeshSettingsForBiome->MaskTexture = MaskTexture;

	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomeMaskTexture - Generated mask texture (%dx%d) for %s (Seed: %d)"),
		BiomeTextureRes, BiomeTextureRes,
		*MeshSettingsForBiome->DisplayName, MeshSettingsForBiome->Seed);
}

void AProceduralMapActor::GenerateBiomeTextures()
{
	double StartTime = FPlatformTime::Seconds();
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomeTextures - Generating biome textures directly from FastNoise2 (Resolution: %dx%d)"), 
		HeightMapResolution, HeightMapResolution);
	
	// Generate mask (color) textures for each biome at biome texture resolution
	GenerateBiomeMaskTexture(EBiomeType::Ocean);
	GenerateBiomeMaskTexture(EBiomeType::Forest);
	GenerateBiomeMaskTexture(EBiomeType::Mountain);
	GenerateBiomeMaskTexture(EBiomeType::Desert);
	GenerateBiomeMaskTexture(EBiomeType::Snow);
	GenerateBiomeMaskTexture(EBiomeType::Volcanic);
	
	// Pre-generate all biome noise maps at full resolution using FastNoise2
	PreGenerateBiomeHeightMaps(HeightMapResolution);
	
	// Lambda to generate high-res texture directly from cached noise data
	auto GenerateHighResTextureFromCache = [this](FBiomeMeshSettings& MeshSettingsRef) -> void
	{
		const TArray<float>* NoiseMapPtr = CachedBiomeHeightMaps.Find(MeshSettingsRef.BiomeType);
		if (!NoiseMapPtr || NoiseMapPtr->Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateBiomeTextures - No cached noise map for %s, skipping"), *MeshSettingsRef.DisplayName);
			return;
		}
		
		// Create the high-res texture
		UTexture2D* HighResTexture = UTexture2D::CreateTransient(HeightMapResolution, HeightMapResolution, PF_B8G8R8A8);
		if (!HighResTexture)
		{
			UE_LOG(LogTemp, Error, TEXT("GenerateBiomeTextures - Failed to create high-res texture for %s"), *MeshSettingsRef.DisplayName);
			return;
		}
		
		HighResTexture->MipGenSettings = TMGS_NoMipmaps;
		HighResTexture->SRGB = false;
		HighResTexture->Filter = TF_Bilinear;
		
		FTexture2DMipMap& HighResMip = HighResTexture->GetPlatformData()->Mips[0];
		void* HighResData = HighResMip.BulkData.Lock(LOCK_READ_WRITE);
		uint8* HighResPixels = static_cast<uint8*>(HighResData);
		
		// Convert noise values (0 to 1) to grayscale texture (0 to 255)
		for (int32 i = 0; i < NoiseMapPtr->Num(); ++i)
		{
			float NoiseValue = (*NoiseMapPtr)[i];
			uint8 GrayValue = static_cast<uint8>(FMath::Clamp(NoiseValue * 255.0f, 0.0f, 255.0f));
			
			const int32 PixelIndex = i * 4;
			HighResPixels[PixelIndex + 0] = GrayValue; // B
			HighResPixels[PixelIndex + 1] = GrayValue; // G
			HighResPixels[PixelIndex + 2] = GrayValue; // R
			HighResPixels[PixelIndex + 3] = 255;       // A
		}
		
		HighResMip.BulkData.Unlock();
		HighResTexture->UpdateResource();
		
		MeshSettingsRef.HighResTexture = HighResTexture;
		
		UE_LOG(LogTemp, Verbose, TEXT("GenerateBiomeTextures - Generated %s noise texture at %dx%d"), 
			*MeshSettingsRef.DisplayName, HeightMapResolution, HeightMapResolution);
	};
	
	// Generate high-res texture for each biome directly from cached FastNoise2 data
	GenerateHighResTextureFromCache(OceanMeshSettings);
	GenerateHighResTextureFromCache(ForestMeshSettings);
	GenerateHighResTextureFromCache(MountainMeshSettings);
	GenerateHighResTextureFromCache(DesertMeshSettings);
	GenerateHighResTextureFromCache(SnowMeshSettings);
	GenerateHighResTextureFromCache(VolcanicMeshSettings);
	
	// Generate the combined height map texture using the cached high-res heightmaps
	GenerateCombinedHeightMapTexture();
	
	double EndTime = FPlatformTime::Seconds();
	MeshTexturesDuration = static_cast<float>(EndTime - StartTime);
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateBiomeTextures - Generated 6 high-res textures (%dx%d) + combined height map in %.4f seconds"), 
		HeightMapResolution, HeightMapResolution, MeshTexturesDuration);
}

void AProceduralMapActor::GenerateCombinedHeightMapTexture()
{
	if (!CurrentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateCombinedHeightMapTexture - No generator. Generate landmass and biomes first."));
		return;
	}
	
	UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
	if (!ContinentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateCombinedHeightMapTexture - Generator is not a ContinentMapGenerator"));
		return;
	}
	
	const TArray<int32>& BiomeMap = ContinentGenerator->GetBiomeMap();
	const TArray<bool>& LandMask = ContinentGenerator->GetLandMask();
	int32 BiomeTextureRes = ContinentGenerator->GetTextureResolution(); // Biome assignment resolution
	
	if (BiomeMap.Num() == 0 || LandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateCombinedHeightMapTexture - No biome data. Generate biomes first."));
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateCombinedHeightMapTexture - Creating combined noise texture (Resolution: %dx%d)"),
		HeightMapResolution, HeightMapResolution);

	// Only regenerate noise maps if not already cached at the correct resolution
	if (CachedBiomeHeightMaps.Num() == 0 || CachedHeightMapResolution != HeightMapResolution)
	{
		PreGenerateBiomeHeightMaps(HeightMapResolution);
	}
	
	// Create the combined texture at HeightMapResolution (high-res)
	UTexture2D* CombinedTexture = UTexture2D::CreateTransient(HeightMapResolution, HeightMapResolution, PF_B8G8R8A8);
	if (!CombinedTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateCombinedHeightMapTexture - Failed to create combined texture"));
		return;
	}
	
	CombinedTexture->MipGenSettings = TMGS_NoMipmaps;
	CombinedTexture->SRGB = false;
	CombinedTexture->Filter = TF_Bilinear;
	
	FTexture2DMipMap& Mip = CombinedTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(TextureData);
	
	// Generate the combined noise texture using pre-generated noise maps with biome blending
	// Raw noise values are in -1 to 1 range
	for (int32 Y = 0; Y < HeightMapResolution; ++Y)
	{
		for (int32 X = 0; X < HeightMapResolution; ++X)
		{
			const int32 PixelIndex = (Y * HeightMapResolution + X) * 4;
			
			// Calculate normalized position (0-1)
			float NormX = static_cast<float>(X) / static_cast<float>(HeightMapResolution - 1);
			float NormY = static_cast<float>(Y) / static_cast<float>(HeightMapResolution - 1);
			
			// Sample blended noise from pre-generated noise maps
			// Returns noise value in 0 to 1 range
			float NoiseValue = FBiomeHeightMapGenerator::SampleBlendedHeight(
				NormX, NormY,
				HeightMapResolution,    // High-res noise map resolution
				BiomeTextureRes,        // Biome assignment resolution
				CachedBiomeHeightMaps,
				BiomeMap,
				LandMask,
				BiomeSettings.LandBiomes,
				0.02f  // Blend radius
			);
			
			// Convert noise (0 to 1) to grayscale (0 to 255)
			uint8 GrayValue = static_cast<uint8>(FMath::Clamp(NoiseValue * 255.0f, 0.0f, 255.0f));
			
			Pixels[PixelIndex + 0] = GrayValue; // B
			Pixels[PixelIndex + 1] = GrayValue; // G
			Pixels[PixelIndex + 2] = GrayValue; // R
			Pixels[PixelIndex + 3] = 255;       // A
		}
	}
	
	Mip.BulkData.Unlock();
	CombinedTexture->UpdateResource();
	
	CombinedHeightMapTexture = CombinedTexture;
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateCombinedHeightMapTexture - Combined noise texture (%dx%d) generated with biome blending"),
		HeightMapResolution, HeightMapResolution);
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
	CombinedHeightMapTexture = nullptr;
	
	// Clear high-res textures from mesh settings
	OceanMeshSettings.HighResTexture = nullptr;
	ForestMeshSettings.HighResTexture = nullptr;
	MountainMeshSettings.HighResTexture = nullptr;
	DesertMeshSettings.HighResTexture = nullptr;
	SnowMeshSettings.HighResTexture = nullptr;
	VolcanicMeshSettings.HighResTexture = nullptr;
	
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

FBiomeMeshSettings* AProceduralMapActor::GetMeshSettingsForBiome(EBiomeType BiomeType)
{
	switch (BiomeType)
	{
		case EBiomeType::Ocean: return &OceanMeshSettings;
		case EBiomeType::Forest: return &ForestMeshSettings;
		case EBiomeType::Mountain: return &MountainMeshSettings;
		case EBiomeType::Desert: return &DesertMeshSettings;
		case EBiomeType::Snow: return &SnowMeshSettings;
		case EBiomeType::Volcanic: return &VolcanicMeshSettings;
		default: return nullptr;
	}
}

const FBiomeMeshSettings* AProceduralMapActor::GetMeshSettingsForBiome(EBiomeType BiomeType) const
{
	switch (BiomeType)
	{
		case EBiomeType::Ocean: return &OceanMeshSettings;
		case EBiomeType::Forest: return &ForestMeshSettings;
		case EBiomeType::Mountain: return &MountainMeshSettings;
		case EBiomeType::Desert: return &DesertMeshSettings;
		case EBiomeType::Snow: return &SnowMeshSettings;
		case EBiomeType::Volcanic: return &VolcanicMeshSettings;
		default: return nullptr;
	}
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
			return BiomeConfig.Color;
		}
		else if (bIsLand)
		{
			return FLinearColor::White;
		}
		else
		{
			return BiomeSettings.OceanColor;
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
	if (!Generator || !TerrainMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateTerrainMesh - Generator or TerrainMesh is null"));
		return;
	}
	
	// Calculate mesh parameters based on MeshDetailLevel (1-100)
	// Scaling for reasonable performance:
	// At level 1:   1x1 chunks, 16 vertices per side = ~256 total vertices
	// At level 50:  5x5 chunks, 48 vertices per side = ~35k total vertices  
	// At level 100: 10x10 chunks, 80 vertices per side = ~640k total vertices
	int32 CalculatedChunksPerSide = FMath::Clamp(1 + (MeshDetailLevel * 9 / 100), 1, 10);
	int32 CalculatedVerticesPerChunk = FMath::Clamp(16 + (MeshDetailLevel * 64 / 100), 16, 80);
	
	// Update internal settings based on detail level
	ChunksPerSide = CalculatedChunksPerSide;
	VerticesPerChunkSide = CalculatedVerticesPerChunk;
	
	int32 EstimatedTotalVerts = ChunksPerSide * ChunksPerSide * VerticesPerChunkSide * VerticesPerChunkSide;
	
	UE_LOG(LogTemp, Log, TEXT("GenerateTerrainMesh - MeshDetailLevel %d maps to: %d chunks per side, %d vertices per chunk (~%d total vertices)"), 
		MeshDetailLevel, ChunksPerSide, VerticesPerChunkSide, EstimatedTotalVerts);
	
	// Initialize random stream
	TerrainRandomStream.Initialize(Seed != 0 ? Seed : FMath::Rand());
	
	const TArray<int32>& BiomeMap = Generator->GetBiomeMap();
	const TArray<bool>& LandMask = Generator->GetLandMask();
	int32 BiomeTextureRes = Generator->GetTextureResolution(); // Resolution of biome assignment map
	
	if (BiomeMap.Num() == 0 || LandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateTerrainMesh - No biome map data"));
		return;
	}
	
	// PRE-GENERATE all biome heightmaps using SIMD-optimized GenUniformGrid2D
	// Use HeightMapResolution (configurable up to 8K) for high-detail noise sampling
	// This is independent of the biome texture resolution
	UE_LOG(LogTemp, Log, TEXT("GenerateTerrainMesh - Using HeightMapResolution: %d (BiomeTextureRes: %d)"), 
		HeightMapResolution, BiomeTextureRes);
	PreGenerateBiomeHeightMaps(HeightMapResolution);
	
	// Clear any existing mesh
	TerrainMesh->ClearAllMeshSections();
	
	// Get map size in Unreal Units (MapSizeInMeters * 100 to convert to cm)
	float MapSizeUU = static_cast<float>(MapSizeInMeters) * 100.0f;
	
	int32 TotalChunks = ChunksPerSide * ChunksPerSide;
	int32 TotalVertices = 0;
	int32 TotalTriangles = 0;
	
	// Generate each chunk as a separate mesh section
	for (int32 ChunkY = 0; ChunkY < ChunksPerSide; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < ChunksPerSide; ++ChunkX)
		{
			int32 ChunkIndex = ChunkY * ChunksPerSide + ChunkX;
			
			UE_LOG(LogTemp, Log, TEXT("GenerateTerrainMesh - Generating chunk %d/%d"), ChunkIndex + 1, TotalChunks);
			
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
					
					// Sample from pre-generated heightmaps (FAST - uses cached GenUniformGrid2D data)
					// Uses HeightMapResolution (up to 8K) for high-detail noise
					// BiomeTextureRes is used for biome assignment lookup
					float Height = 10.f;
					
					// Get smoothly blended vertex color (uses biome texture resolution)
					FColor VertColor = GetBlendedBiomeColor(GlobalNormX, GlobalNormY, BiomeTextureRes, BiomeMap, LandMask);
					
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
}

void AProceduralMapActor::PreGenerateBiomeHeightMaps(int32 Resolution)
{
	// Calculate memory usage: Resolution^2 floats per biome
	int64 BytesPerHeightMap = static_cast<int64>(Resolution) * Resolution * sizeof(float);
	int64 TotalBytes = BytesPerHeightMap * 6; // All 6 biomes
	float TotalMB = TotalBytes / (1024.0f * 1024.0f);
	
	UE_LOG(LogTemp, Log, TEXT("PreGenerateBiomeHeightMaps - Generating %dx%d heightmaps for all biomes (%.1f MB) using GenUniformGrid2D"), 
		Resolution, Resolution, TotalMB);
	
	double StartTime = FPlatformTime::Seconds();
	
	// Generate heightmaps for all biomes
	TArray<FBiomeMeshSettings> AllBiomeSettings;
	AllBiomeSettings.Add(OceanMeshSettings);
	AllBiomeSettings.Add(ForestMeshSettings);
	AllBiomeSettings.Add(MountainMeshSettings);
	AllBiomeSettings.Add(DesertMeshSettings);
	AllBiomeSettings.Add(SnowMeshSettings);
	AllBiomeSettings.Add(VolcanicMeshSettings);
	
	// Generate heightmap using SIMD-optimized GenUniformGrid2D
	// Set bTileable = true for seamless world wrapping terrain
	CachedBiomeHeightMaps = FBiomeHeightMapGenerator::GenerateAllBiomeHeightMaps(
		AllBiomeSettings,
		Resolution,
		Seed,
		static_cast<float>(MapSizeInMeters),
		bUseTileableNoise  // Use tileable noise for seamless wrapping
	);
	
	CachedHeightMapResolution = Resolution;
	
	double Duration = FPlatformTime::Seconds() - StartTime;
	UE_LOG(LogTemp, Log, TEXT("PreGenerateBiomeHeightMaps - Generated %d biome heightmaps (%dx%d, %.1f MB) in %.4f seconds"), 
		AllBiomeSettings.Num(), Resolution, Resolution, TotalMB, Duration);
}
