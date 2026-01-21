// ContinentMapGenerator.cpp
// Implementation of the Continent map generator

#include "ContinentMapGenerator.h"
#include "Engine/Texture2D.h"

UContinentMapGenerator::UContinentMapGenerator()
{
	PreviewTexture = nullptr;
	Seed = 0;
}

void UContinentMapGenerator::Initialize(const FMapGenerationSettings& InSettings)
{
	Super::Initialize(InSettings);
	
	// Calculate texture resolution based on MapResolution (1-100 maps to 64-512)
	TextureResolution = FMath::Clamp(64 + (Settings.MapResolution * 448 / 100), 64, 512);
	
	// Generate a random seed if not set
	if (Seed == 0)
	{
		Seed = FMath::Rand();
	}
	
	// Initialize random stream with seed
	RandomStream.Initialize(Seed);
	
	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator initialized - Size: %s meters (%s UU), Resolution: %d, TextureRes: %d, Seed: %d"),
		*Settings.MapSizeInMeters.ToString(), 
		*Settings.GetMapSizeInUnrealUnits().ToString(),
		Settings.MapResolution,
		TextureResolution,
		Seed);

	// Log biome settings
	UE_LOG(LogTemp, Log, TEXT("Biome Settings - Total Percentage: %.1f%%"), BiomeSettings.GetTotalPercentage());
	for (const FBiomeConfig& Biome : BiomeSettings.LandBiomes)
	{
		UE_LOG(LogTemp, Log, TEXT("  - %s: %.1f%%"), *Biome.DisplayName, Biome.Percentage);
	}
}

void UContinentMapGenerator::SetBiomeSettings(const FContinentBiomeSettings& InBiomeSettings)
{
	BiomeSettings = InBiomeSettings;
}

bool UContinentMapGenerator::Generate()
{
	if (!Super::Generate())
	{
		return false;
	}

	// Validate biome percentages
	if (!BiomeSettings.ArePercentagesValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ContinentMapGenerator::Generate - Biome percentages don't add up to 100%% (Total: %.1f%%). Normalizing..."),
			BiomeSettings.GetTotalPercentage());
		BiomeSettings.NormalizePercentages();
	}

	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::Generate - Generating continent with %d biomes"),
		BiomeSettings.LandBiomes.Num());
	
	// Pass 1: Generate the land mask (determine which pixels are land)
	GenerateLandMask();
	
	// Pass 2: Generate biome seed points distributed across land pixels
	GenerateBiomeSeedPoints();
	
	// Pass 3: Generate the final preview texture with biome colors
	GeneratePreviewTexture();
	
	return true;
}

// Pass 1: Generate the land mask
void UContinentMapGenerator::GenerateLandMask()
{
	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;
	const float LandThreshold = 0.5f;
	
	// Clear previous data
	LandMask.Empty();
	LandMask.SetNum(Width * Height);
	LandPixels.Empty();
	
	// Generate land mask and collect land pixel coordinates
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			float NormX = static_cast<float>(X) / static_cast<float>(Width);
			float NormY = static_cast<float>(Y) / static_cast<float>(Height);
			
			float ContinentMask = GetContinentMask(NormX, NormY);
			bool bIsLand = ContinentMask >= LandThreshold;
			
			int32 Index = Y * Width + X;
			LandMask[Index] = bIsLand;
			
			if (bIsLand)
			{
				LandPixels.Add(FIntPoint(X, Y));
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Land mask generated: %d land pixels out of %d total (%.1f%%)"),
		LandPixels.Num(), Width * Height, 
		(float)LandPixels.Num() / (float)(Width * Height) * 100.0f);
}

// Pass 2: Generate ONE Voronoi seed point per biome for contiguous regions
void UContinentMapGenerator::GenerateBiomeSeedPoints()
{
	BiomeSeedPoints.Empty();
	
	if (BiomeSettings.LandBiomes.Num() == 0 || LandPixels.Num() == 0)
	{
		return;
	}
	
	const float TotalPercentage = BiomeSettings.GetTotalPercentage();
	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;
	
	// ONE seed point per biome = one contiguous region per biome
	for (int32 i = 0; i < BiomeSettings.LandBiomes.Num(); ++i)
	{
		const FBiomeConfig& Biome = BiomeSettings.LandBiomes[i];
		float NormalizedPercentage = Biome.Percentage / TotalPercentage;
		
		// Pick a random land pixel for this biome's seed point
		int32 RandomLandIndex = RandomStream.RandRange(0, LandPixels.Num() - 1);
		FIntPoint LandPixel = LandPixels[RandomLandIndex];
		
		// Convert to normalized coordinates
		FVector2D Position;
		Position.X = static_cast<float>(LandPixel.X) / static_cast<float>(Width);
		Position.Y = static_cast<float>(LandPixel.Y) / static_cast<float>(Height);
		
		// Weight is INVERSE of percentage - smaller weight = LARGER region
		// This makes biomes with higher percentages claim more area
		float Weight = 1.0f / FMath::Max(0.01f, NormalizedPercentage);
		
		BiomeSeedPoints.Add(FBiomeSeedPoint(Position, i, Weight));
		
		UE_LOG(LogTemp, Log, TEXT("Biome %s: seed at (%.2f, %.2f), percentage: %.1f%%, weight: %.2f"),
			*Biome.DisplayName, Position.X, Position.Y, Biome.Percentage, Weight);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Generated %d biome seed points (one per biome) on %d land pixels"), 
		BiomeSeedPoints.Num(), LandPixels.Num());
}

// Simple hash-based noise function
float UContinentMapGenerator::Noise2D(float X, float Y) const
{
	// Use seed to offset the noise
	X += Seed * 0.1f;
	Y += Seed * 0.1f;
	
	int32 Xi = FMath::FloorToInt(X);
	int32 Yi = FMath::FloorToInt(Y);
	float Xf = X - Xi;
	float Yf = Y - Yi;
	
	// Smooth interpolation
	float U = Xf * Xf * (3.0f - 2.0f * Xf);
	float V = Yf * Yf * (3.0f - 2.0f * Yf);
	
	// Hash function for pseudo-random values at grid points
	auto Hash = [](int32 X, int32 Y) -> float
	{
		int32 N = X + Y * 57;
		N = (N << 13) ^ N;
		return (1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
	};
	
	// Bilinear interpolation of corner values
	float A = Hash(Xi, Yi);
	float B = Hash(Xi + 1, Yi);
	float C = Hash(Xi, Yi + 1);
	float D = Hash(Xi + 1, Yi + 1);
	
	float AB = FMath::Lerp(A, B, U);
	float CD = FMath::Lerp(C, D, U);
	
	return FMath::Lerp(AB, CD, V);
}

// Fractal Brownian Motion - layered noise for natural-looking terrain
float UContinentMapGenerator::FBM(float X, float Y, int32 Octaves, float Persistence) const
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = 1.0f;
	float MaxValue = 0.0f;
	
	for (int32 i = 0; i < Octaves; ++i)
	{
		Total += Noise2D(X * Frequency, Y * Frequency) * Amplitude;
		MaxValue += Amplitude;
		Amplitude *= Persistence;
		Frequency *= 2.0f;
	}
	
	return Total / MaxValue;
}

// Get continent mask with organic, irregular edges
float UContinentMapGenerator::GetContinentMask(float NormX, float NormY) const
{
	// Center the coordinates (-0.5 to 0.5)
	float CX = NormX - 0.5f;
	float CY = NormY - 0.5f;
	
	// Base distance from center (creates general continent shape)
	float DistFromCenter = FMath::Sqrt(CX * CX + CY * CY) * 2.0f;
	
	// Add noise to create irregular coastline
	float NoiseScale = 3.0f;
	float CoastNoise = FBM(NormX * NoiseScale, NormY * NoiseScale, 4, 0.5f) * 0.4f;
	
	// Add larger-scale noise for continent shape variation
	float ShapeNoise = FBM(NormX * 1.5f + 100.0f, NormY * 1.5f + 100.0f, 3, 0.6f) * 0.3f;
	
	// Combine distance with noise
	float ContinentValue = 1.0f - DistFromCenter + CoastNoise + ShapeNoise;
	
	// Add some bumps and indentations for more interesting coastline
	float DetailNoise = FBM(NormX * 8.0f + 50.0f, NormY * 8.0f + 50.0f, 2, 0.5f) * 0.15f;
	ContinentValue += DetailNoise;
	
	// Threshold to create land/water distinction
	// Values > 0.5 are land, < 0.5 are water
	return FMath::Clamp(ContinentValue, 0.0f, 1.0f);
}

// Determine which biome a land position belongs to using Voronoi regions
int32 UContinentMapGenerator::GetBiomeAtPosition(float NormX, float NormY) const
{
	if (BiomeSeedPoints.Num() == 0 || BiomeSettings.LandBiomes.Num() == 0)
	{
		return 0;
	}
	
	FVector2D Position(NormX, NormY);
	
	// Add noise offset to create organic, wavy boundaries between biomes
	float NoiseScale = 3.0f;
	float NoiseStrength = 0.05f;
	float NoiseOffsetX = FBM(NormX * NoiseScale + 500.0f, NormY * NoiseScale + 500.0f, 3, 0.5f) * NoiseStrength;
	float NoiseOffsetY = FBM(NormX * NoiseScale + 600.0f, NormY * NoiseScale + 600.0f, 3, 0.5f) * NoiseStrength;
	Position.X += NoiseOffsetX;
	Position.Y += NoiseOffsetY;
	
	// Find the closest seed point (weighted Voronoi)
	int32 ClosestBiome = 0;
	float ClosestDist = TNumericLimits<float>::Max();
	
	for (const FBiomeSeedPoint& SeedPoint : BiomeSeedPoints)
	{
		// Calculate weighted distance (smaller weight = larger region)
		float Dist = FVector2D::DistSquared(Position, SeedPoint.Position) * SeedPoint.Weight;
		
		if (Dist < ClosestDist)
		{
			ClosestDist = Dist;
			ClosestBiome = SeedPoint.BiomeIndex;
		}
	}
	
	return ClosestBiome;
}

void UContinentMapGenerator::GeneratePreviewTexture()
{
	// Create the texture
	PreviewTexture = UTexture2D::CreateTransient(TextureResolution, TextureResolution, PF_B8G8R8A8);
	if (!PreviewTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GeneratePreviewTexture - Failed to create texture"));
		return;
	}

	PreviewTexture->MipGenSettings = TMGS_NoMipmaps;
	PreviewTexture->SRGB = true;
	PreviewTexture->Filter = TF_Bilinear;

	// Lock the texture for writing
	FTexture2DMipMap& Mip = PreviewTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(TextureData);

	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelIndex = (Y * Width + X) * 4;
			const int32 MaskIndex = Y * Width + X;
			
			// Normalize coordinates to 0-1 range
			float NormX = static_cast<float>(X) / static_cast<float>(Width);
			float NormY = static_cast<float>(Y) / static_cast<float>(Height);
			
			FLinearColor PixelColor;
			
			// Use pre-computed land mask
			bool bIsLand = (MaskIndex < LandMask.Num()) ? LandMask[MaskIndex] : false;
			
			if (!bIsLand)
			{
				// Ocean
				PixelColor = BiomeSettings.OceanColor;
			}
			else if (BiomeSettings.LandBiomes.Num() == 0)
			{
				// No biomes defined, use default green
				PixelColor = FLinearColor::Green;
			}
			else
			{
				// Get biome for this land position
				int32 BiomeIndex = GetBiomeAtPosition(NormX, NormY);
				if (BiomeIndex >= 0 && BiomeIndex < BiomeSettings.LandBiomes.Num())
				{
					PixelColor = BiomeSettings.LandBiomes[BiomeIndex].Color;
				}
				else
				{
					PixelColor = FLinearColor::Green;
				}
			}

			// Convert to BGRA8
			Pixels[PixelIndex + 0] = FMath::Clamp(static_cast<int32>(PixelColor.B * 255.0f), 0, 255); // B
			Pixels[PixelIndex + 1] = FMath::Clamp(static_cast<int32>(PixelColor.G * 255.0f), 0, 255); // G
			Pixels[PixelIndex + 2] = FMath::Clamp(static_cast<int32>(PixelColor.R * 255.0f), 0, 255); // R
			Pixels[PixelIndex + 3] = 255; // A
		}
	}

	// Unlock and update the texture
	Mip.BulkData.Unlock();
	PreviewTexture->UpdateResource();

	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GeneratePreviewTexture - Created %dx%d preview texture with %d biomes"),
		TextureResolution, TextureResolution, BiomeSettings.LandBiomes.Num());
}

