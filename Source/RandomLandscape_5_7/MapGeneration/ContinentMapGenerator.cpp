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
	
	// Generate the preview texture
	GeneratePreviewTexture();
	
	return true;
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

// Determine which biome a land position belongs to
int32 UContinentMapGenerator::GetBiomeAtPosition(float NormX, float NormY, float ContinentMask) const
{
	if (BiomeSettings.LandBiomes.Num() == 0)
	{
		return -1;
	}
	
	// Use noise to create organic biome boundaries
	float BiomeNoiseScale = 2.5f;
	
	// Create multiple noise layers for biome selection
	float BiomeNoise1 = FBM(NormX * BiomeNoiseScale + 200.0f, NormY * BiomeNoiseScale + 200.0f, 3, 0.5f);
	float BiomeNoise2 = FBM(NormX * BiomeNoiseScale * 0.7f + 300.0f, NormY * BiomeNoiseScale * 0.7f + 300.0f, 3, 0.5f);
	
	// Combine noises to get a value from 0 to 1
	float BiomeValue = (BiomeNoise1 + BiomeNoise2 + 2.0f) / 4.0f; // Normalize to 0-1
	BiomeValue = FMath::Clamp(BiomeValue, 0.0f, 0.999f);
	
	// Map the noise value to biome based on percentages
	float AccumulatedPercentage = 0.0f;
	const float TotalPercentage = BiomeSettings.GetTotalPercentage();
	
	for (int32 i = 0; i < BiomeSettings.LandBiomes.Num(); ++i)
	{
		float NormalizedPercentage = BiomeSettings.LandBiomes[i].Percentage / TotalPercentage;
		AccumulatedPercentage += NormalizedPercentage;
		
		if (BiomeValue < AccumulatedPercentage)
		{
			return i;
		}
	}
	
	// Fallback to last biome
	return BiomeSettings.LandBiomes.Num() - 1;
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
	const float LandThreshold = 0.5f; // Values above this are land

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelIndex = (Y * Width + X) * 4;
			
			// Normalize coordinates to 0-1 range
			float NormX = static_cast<float>(X) / static_cast<float>(Width);
			float NormY = static_cast<float>(Y) / static_cast<float>(Height);
			
			// Get continent mask (determines land vs ocean)
			float ContinentMask = GetContinentMask(NormX, NormY);
			
			FLinearColor PixelColor;
			
			if (ContinentMask < LandThreshold)
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
				int32 BiomeIndex = GetBiomeAtPosition(NormX, NormY, ContinentMask);
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

