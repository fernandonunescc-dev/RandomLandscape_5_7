// BiomeDataGenerationActor.cpp
// Manual editor-only actor for Landmass + Biome + Heightmap preview generation

#include "BiomeDataGenerationActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

ABiomeDataGenerationActor::ABiomeDataGenerationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Initialize output pointers
	LandmassPreviewTexture = nullptr;
	BiomePreviewTexture = nullptr;
}

void ABiomeDataGenerationActor::GenerateLandmass()
{
	// Create generator instance
	ULandmassGenerator* Generator = NewObject<ULandmassGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create ULandmassGenerator"));
		return;
	}

	// Initialize and generate
	Generator->Initialize(LandmassSettings);
	
	if (!Generator->Generate())
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Landmass generation failed"));
		return;
	}

	// Cache results
	CachedLandMask = Generator->GetLandMask();
	ActualLandmassSeedUsed = Generator->GetSeed();
	TextureResolutionUsed = Generator->GetTextureResolution();

	// Get preview texture from generator
	LandmassPreviewTexture = Generator->GetPreviewTexture();

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Landmass generated - Seed: %d, Resolution: %d, LandPixels: %d, MapType: %d, MapSize: %d"),
		ActualLandmassSeedUsed, TextureResolutionUsed, CachedLandMask.Num(),
		static_cast<int32>(LandmassSettings.MapType), static_cast<int32>(LandmassSettings.MapSize));
}

void ABiomeDataGenerationActor::GenerateBiomes()
{
	// Check prerequisite
	if (CachedLandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: GenerateLandmass first - no land mask available"));
		return;
	}

	// Sync resolution from landmass settings
	BiomeLayoutSettings.TextureResolution = LandmassSettings.TextureResolution;

	// Create generator instance
	UBiomeMapGenerator* Generator = NewObject<UBiomeMapGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create UBiomeMapGenerator"));
		return;
	}

	// Initialize with our settings
	Generator->Initialize(BiomeLayoutSettings);

	// Generate biome map
	if (!Generator->Generate(CachedLandMask))
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Biome generation failed"));
		return;
	}

	// Cache results
	CachedBiomeMap = Generator->GetBiomeMap();
	ActualBiomeSeedUsed = Generator->GetBiomeSeed();

	// Build preview texture
	BuildBiomePreviewTexture();

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Biomes generated - Seed: %d"), ActualBiomeSeedUsed);
}

void ABiomeDataGenerationActor::GenerateHeightmaps()
{
	// Check prerequisites
	if (CachedLandMask.Num() == 0 || CachedBiomeMap.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: Generate Landmass and Biomes first"));
		return;
	}

	// Sync resolution from landmass settings
	HeightmapSettings.TextureResolution = LandmassSettings.TextureResolution;

	// Create generator instance
	UHeightmapGenerator* Generator = NewObject<UHeightmapGenerator>(this);
	if (!Generator)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create UHeightmapGenerator"));
		return;
	}

	// Initialize and generate
	Generator->Initialize(HeightmapSettings);

	if (!Generator->Generate(CachedBiomeMap, CachedLandMask, BiomeLayoutSettings.Layers))
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Heightmap generation failed"));
		return;
	}

	// Store results
	HeightmapResults = Generator->GetResults();
	ActualHeightmapSeedUsed = Generator->GetSeed();

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Heightmaps generated - Seed: %d, Biomes: %d"),
		ActualHeightmapSeedUsed, HeightmapResults.Num());
}

void ABiomeDataGenerationActor::GenerateAll()
{
	GenerateLandmass();

	// Only continue if landmass was generated successfully
	if (CachedLandMask.Num() > 0)
	{
		GenerateBiomes();

		// Only continue if biomes were generated successfully
		if (CachedBiomeMap.Num() > 0)
		{
			GenerateHeightmaps();
		}
	}
}

void ABiomeDataGenerationActor::ClearGeneratedData()
{
	LandmassPreviewTexture = nullptr;
	BiomePreviewTexture = nullptr;
	HeightmapResults.Empty();
	CachedLandMask.Empty();
	CachedBiomeMap.Empty();
	ActualLandmassSeedUsed = 0;
	ActualBiomeSeedUsed = 0;
	ActualHeightmapSeedUsed = 0;
	TextureResolutionUsed = 0;

	// Destroy spawned mesh actor
	if (SpawnedMeshActor)
	{
		SpawnedMeshActor->Destroy();
		SpawnedMeshActor = nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: All generated data cleared"));
}

void ABiomeDataGenerationActor::GenerateMesh()
{
	// Check prerequisites
	if (CachedLandMask.Num() == 0 || CachedBiomeMap.Num() == 0 || HeightmapResults.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABiomeDataGenerationActor: Generate Landmass, Biomes, and Heightmaps first before generating mesh"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: No world available for mesh generation"));
		return;
	}

	// Spawn or reuse the mesh actor
	if (!SpawnedMeshActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnedMeshActor = World->SpawnActor<ALandscapeMeshActor>(GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
	}

	if (!SpawnedMeshActor)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to spawn ALandscapeMeshActor"));
		return;
	}

	const int32 Resolution = LandmassSettings.TextureResolution;
	const float WorldSizeCm = LandmassSettings.GetWorldSizeCm();
	const float MaxMapHeight = LandmassSettings.MaxMapHeight;

	SpawnedMeshActor->BuildMesh(
		CachedBiomeMap,
		CachedLandMask,
		HeightmapResults,
		BiomeLayoutSettings.Layers,
		BiomeLayoutSettings.OceanColor,
		Resolution,
		WorldSizeCm,
		MaxMapHeight);

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Mesh generated - Resolution: %d, WorldSize: %.0f cm, MaxHeight: %.0f cm"),
		Resolution, WorldSizeCm, MaxMapHeight);
}

void ABiomeDataGenerationActor::BuildBiomePreviewTexture()
{
	const int32 Resolution = BiomeLayoutSettings.TextureResolution;
	const int32 TotalPixels = Resolution * Resolution;

	if (CachedBiomeMap.Num() != TotalPixels || CachedLandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Data size mismatch for biome preview"));
		return;
	}

	// Build pixel data
	TArray<FColor> PixelData;
	PixelData.SetNumUninitialized(TotalPixels);

	// Convert ocean color once
	const FColor OceanColor = BiomeLayoutSettings.OceanColor.ToFColor(false);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (CachedLandMask[i] == 0)
		{
			// Ocean pixel
			PixelData[i] = OceanColor;
		}
		else
		{
			// Land pixel - get biome color
			const EBiomeType BiomeType = static_cast<EBiomeType>(CachedBiomeMap[i]);
			const FLinearColor BiomeColor = BiomeLayoutSettings.GetBiomeColor(BiomeType);
			PixelData[i] = BiomeColor.ToFColor(false);
		}
	}

	// Create texture
	BiomePreviewTexture = CreatePreviewTexture(Resolution, PixelData);
}

UTexture2D* ABiomeDataGenerationActor::CreatePreviewTexture(int32 Resolution, const TArray<FColor>& PixelData)
{
	if (Resolution <= 0 || PixelData.Num() != Resolution * Resolution)
	{
		return nullptr;
	}

	// Create transient texture
	UTexture2D* Texture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("ABiomeDataGenerationActor: Failed to create transient texture"));
		return nullptr;
	}

	// Configure texture settings
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->Filter = TF_Nearest;
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_VectorDisplacementmap; // No compression

	// Lock and write pixel data
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, PixelData.GetData(), PixelData.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();

	// Update GPU resource
	Texture->UpdateResource();

	return Texture;
}
