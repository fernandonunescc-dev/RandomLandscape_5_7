// ContinentMapGenerator.cpp
// Implementation of the Continent map generator
// Version: 01.27.2026.22.10

#include "ContinentMapGenerator.h"
#include "LandmassGenerator.h"
#include "BiomeMapGenerator.h"
#include "Engine/Texture2D.h"

UContinentMapGenerator::UContinentMapGenerator()
{
	PreviewTexture = nullptr;
	LandmassGenerator = nullptr;
	BiomeGenerator = nullptr;
	Seed = 0;
	BiomeSeed = 0;
}

void UContinentMapGenerator::Initialize(const FMapGenerationSettings& InSettings)
{
	Super::Initialize(InSettings);
	
	TextureResolution = 512;
	
	RandomStream.Initialize(Seed);
	BiomeRandomStream.Initialize(BiomeSeed);
	
	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator initialized - Size: %d meters, Resolution: %d, LandmassSeed: %d, BiomeSeed: %d"),
		Settings.MapSizeInMeters, TextureResolution, Seed, BiomeSeed);
}

void UContinentMapGenerator::SetBiomeSettings(const FContinentBiomeSettings& InBiomeSettings)
{
	BiomeSettings = InBiomeSettings;
}

void UContinentMapGenerator::SetMeshSettings(const FMeshGenerationSettings& InMeshSettings)
{
	MeshSettings = InMeshSettings;
}

bool UContinentMapGenerator::Generate()
{
	if (!Super::Generate())
	{
		return false;
	}

	if (!GenerateLandmassOnly())
	{
		return false;
	}
	
	return GenerateBiomesOnly();
}

bool UContinentMapGenerator::GenerateLandmassOnly()
{
	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateLandmassOnly - Generating land mask"));
	
	GenerateLandMask();
	GeneratePreviewTexture();
	
	return true;
}

bool UContinentMapGenerator::GenerateBiomesOnly()
{
	if (LandMask.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GenerateBiomesOnly - No landmass data. Call GenerateLandmassOnly first."));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateBiomesOnly - Using biome seed: %d"), BiomeSeed);

	// Use the new BiomeMapGenerator
	AssignBiomesToLand();
	
	GeneratePreviewTexture();
	GenerateBiomeMaskTextures();
	
	return true;
}

void UContinentMapGenerator::GenerateLandMask()
{
	if (!LandmassGenerator)
	{
		LandmassGenerator = NewObject<ULandmassGenerator>(this);
	}

	FLandmassSettings LandmassSettings;
	LandmassSettings.Seed = Seed;
	LandmassSettings.TextureResolution = TextureResolution;
	LandmassSettings.LandCoveragePercent = BiomeSettings.LandCoveragePercent;

	LandmassGenerator->Initialize(LandmassSettings);

	if (LandmassGenerator->Generate())
	{
		LandMask = LandmassGenerator->GetLandMask();
		LandPixels = LandmassGenerator->GetLandPixels();

		UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator: Landmass generated - %d land pixels"), LandPixels.Num());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GenerateLandMask - LandmassGenerator failed"));
	}
}

void UContinentMapGenerator::AssignBiomesToLand()
{
	// Create and configure the BiomeMapGenerator
	if (!BiomeGenerator)
	{
		BiomeGenerator = NewObject<UBiomeMapGenerator>(this);
	}

	// Build layout settings from BiomeSettings
	FBiomeLayoutSettings LayoutSettings;
	LayoutSettings.BiomeSeed = BiomeSeed; // Use BiomeSeed for biome placement (independent from landmass seed)
	LayoutSettings.TextureResolution = TextureResolution;
	LayoutSettings.ConnectorBiomeType = EBiomeType::Forest; // Forest is the connector
	LayoutSettings.bUseEightWayAdjacency = true;

	// Convert FBiomeConfig to FBiomeLayerSettings
	LayoutSettings.Layers.Reset();
	for (const FBiomeConfig& Config : BiomeSettings.LandBiomes)
	{
		// Skip Forest - it's the connector biome
		if (Config.BiomeType == EBiomeType::Forest)
		{
			continue;
		}

		FBiomeLayerSettings Layer;
		Layer.BiomeType = Config.BiomeType;
		Layer.TargetPercentOfLand = Config.Percentage;
		Layer.SpreadNoiseFrequency = 6.0f;
		Layer.SpreadNoiseAmplitude = 1.25f;

		LayoutSettings.Layers.Add(Layer);
	}

	BiomeGenerator->Initialize(LayoutSettings);

	if (BiomeGenerator->Generate(LandMask))
	{
		BiomeMap = BiomeGenerator->GetBiomeMap();
		UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::AssignBiomesToLand - BiomeMapGenerator succeeded"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::AssignBiomesToLand - BiomeMapGenerator failed"));
		
		// Fallback: fill all land with Forest
		const int32 Size = TextureResolution * TextureResolution;
		BiomeMap.SetNum(Size);
		const int32 ForestId = static_cast<int32>(EBiomeType::Forest);
		for (int32 i = 0; i < Size; ++i)
		{
			BiomeMap[i] = ForestId;
		}
	}
}

void UContinentMapGenerator::GeneratePreviewTexture()
{
	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;
	const int32 ExpectedSize = Width * Height;

	if (LandMask.Num() != ExpectedSize)
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GeneratePreviewTexture - LandMask size mismatch"));
		return;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GeneratePreviewTexture - Failed to create texture"));
		return;
	}

	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->SRGB = false;
	Texture->Filter = TF_Nearest;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(TextureData);

	const bool bHasBiomes = BiomeMap.Num() == ExpectedSize && BiomeSettings.LandBiomes.Num() > 0;

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelIndex = (Y * Width + X) * 4;
			const int32 MapIndex = Y * Width + X;

			const bool bIsLand = (LandMask[MapIndex] != 0);
			FLinearColor PixelColor;

			if (!bIsLand)
			{
				PixelColor = BiomeSettings.OceanColor;
			}
			else if (bHasBiomes)
			{
				const int32 BiomeTypeInt = BiomeMap[MapIndex];
				const EBiomeType BiomeType = static_cast<EBiomeType>(BiomeTypeInt);
				const FBiomeConfig* BiomeConfig = BiomeSettings.FindBiomeConfig(BiomeType);

				if (BiomeConfig)
				{
					PixelColor = BiomeConfig->Color;
				}
				else
				{
					PixelColor = FLinearColor(0.2f, 0.6f, 0.2f, 1.0f); // Fallback green
				}
			}
			else
			{
				PixelColor = FLinearColor::White;
			}

			const FColor FinalColor = PixelColor.ToFColor(false);
			Pixels[PixelIndex + 0] = FinalColor.B;
			Pixels[PixelIndex + 1] = FinalColor.G;
			Pixels[PixelIndex + 2] = FinalColor.R;
			Pixels[PixelIndex + 3] = 255;
		}
	}

	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	PreviewTexture = Texture;

	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GeneratePreviewTexture - Created %dx%d texture"), Width, Height);
}

void UContinentMapGenerator::GenerateBiomeMaskTextures()
{
	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;
	const int32 ExpectedSize = Width * Height;

	if (LandMask.Num() != ExpectedSize || BiomeMap.Num() != ExpectedSize)
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - Data size mismatch"));
		return;
	}

	// Generate ocean mask
	{
		UTexture2D* OceanTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (OceanTexture)
		{
			OceanTexture->MipGenSettings = TMGS_NoMipmaps;
			OceanTexture->SRGB = false;
			OceanTexture->Filter = TF_Nearest;

			FTexture2DMipMap& Mip = OceanTexture->GetPlatformData()->Mips[0];
			void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
			uint8* Pixels = static_cast<uint8*>(TextureData);

			const FColor OceanColor = BiomeSettings.OceanColor.ToFColor(false);

			for (int32 i = 0; i < ExpectedSize; ++i)
			{
				const int32 PixelIndex = i * 4;
				const bool bIsOcean = (LandMask[i] == 0);

				if (bIsOcean)
				{
					Pixels[PixelIndex + 0] = OceanColor.B;
					Pixels[PixelIndex + 1] = OceanColor.G;
					Pixels[PixelIndex + 2] = OceanColor.R;
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

			Mip.BulkData.Unlock();
			OceanTexture->UpdateResource();
			MeshSettings.OceanSettings.MaskTexture = OceanTexture;
		}
	}

	// Generate mask for each land biome
	for (const FBiomeConfig& Biome : BiomeSettings.LandBiomes)
	{
		FBiomeMeshSettings* BiomeMeshSettings = MeshSettings.GetSettingsForBiomeMutable(Biome.BiomeType);
		if (!BiomeMeshSettings)
		{
			continue;
		}

		UTexture2D* MaskTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!MaskTexture)
		{
			continue;
		}

		MaskTexture->MipGenSettings = TMGS_NoMipmaps;
		MaskTexture->SRGB = false;
		MaskTexture->Filter = TF_Nearest;

		FTexture2DMipMap& Mip = MaskTexture->GetPlatformData()->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		uint8* Pixels = static_cast<uint8*>(TextureData);

		const FColor BiomeColor = Biome.Color.ToFColor(false);
		const int32 TargetBiomeId = static_cast<int32>(Biome.BiomeType);

		for (int32 i = 0; i < ExpectedSize; ++i)
		{
			const int32 PixelIndex = i * 4;
			const bool bIsThisBiome = (BiomeMap[i] == TargetBiomeId);

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

		Mip.BulkData.Unlock();
		MaskTexture->UpdateResource();
		BiomeMeshSettings->MaskTexture = MaskTexture;
	}

	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - Generated %d masks"), BiomeSettings.LandBiomes.Num() + 1);
}
