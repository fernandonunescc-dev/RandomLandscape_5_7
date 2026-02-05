// BiomeDataGenerationActor.cpp
// Manual editor-only actor for Landmass + Biome preview generation
// Version: 02.04.2026.15.30

#include "BiomeDataGenerationActor.h"
#include "Engine/Texture2D.h"

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

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: Landmass generated - Seed: %d, Resolution: %d, LandPixels: %d"),
		ActualLandmassSeedUsed, TextureResolutionUsed, CachedLandMask.Num());
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

void ABiomeDataGenerationActor::GenerateAll()
{
	GenerateLandmass();

	// Only continue if landmass was generated successfully
	if (CachedLandMask.Num() > 0)
	{
		GenerateBiomes();
	}
}

void ABiomeDataGenerationActor::ClearGeneratedData()
{
	LandmassPreviewTexture = nullptr;
	BiomePreviewTexture = nullptr;
	CachedLandMask.Empty();
	CachedBiomeMap.Empty();
	ActualLandmassSeedUsed = 0;
	ActualBiomeSeedUsed = 0;
	TextureResolutionUsed = 0;

	UE_LOG(LogTemp, Log, TEXT("ABiomeDataGenerationActor: All generated data cleared"));
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
