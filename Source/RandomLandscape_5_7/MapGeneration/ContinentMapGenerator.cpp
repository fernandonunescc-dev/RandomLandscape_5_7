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
	
	// If seed is 0, generate a truly random seed for this generation
	if (Seed == 0)
	{
		Seed = FMath::RandRange(1, TNumericLimits<int32>::Max());
	}
	
	// Always initialize random stream with the current seed
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
	
	// Pass 2: Assign biomes to land pixels using flood-fill
	AssignBiomesToLand();
	
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

// Pass 2: Assign biomes to land pixels using flood-fill for exact percentages
void UContinentMapGenerator::AssignBiomesToLand()
{
	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;
	const int32 TotalLandPixels = LandPixels.Num();
	
	// Initialize biome map (-1 = unassigned/ocean)
	BiomeMap.Empty();
	BiomeMap.SetNum(Width * Height);
	for (int32& BiomeIndex : BiomeMap)
	{
		BiomeIndex = -1;
	}
	
	if (BiomeSettings.LandBiomes.Num() == 0 || TotalLandPixels == 0)
	{
		return;
	}
	
	const float TotalPercentage = BiomeSettings.GetTotalPercentage();
	
	// Calculate how many pixels each biome should get
	TArray<int32> TargetPixelCounts;
	TArray<int32> CurrentPixelCounts;
	TArray<FIntPoint> BiomeSeedPixels;
	int32 AssignedTotal = 0;
	
	for (int32 i = 0; i < BiomeSettings.LandBiomes.Num(); ++i)
	{
		const FBiomeConfig& Biome = BiomeSettings.LandBiomes[i];
		float NormalizedPercentage = Biome.Percentage / TotalPercentage;
		int32 TargetCount = FMath::RoundToInt(NormalizedPercentage * TotalLandPixels);
		
		// Ensure at least 1 pixel per biome
		TargetCount = FMath::Max(1, TargetCount);
		TargetPixelCounts.Add(TargetCount);
		CurrentPixelCounts.Add(0);
		AssignedTotal += TargetCount;
		
		// Pick a random starting pixel for this biome
		int32 RandomLandIndex = RandomStream.RandRange(0, LandPixels.Num() - 1);
		BiomeSeedPixels.Add(LandPixels[RandomLandIndex]);
		
		UE_LOG(LogTemp, Log, TEXT("Biome %s: target %d pixels (%.1f%%)"),
			*Biome.DisplayName, TargetCount, Biome.Percentage);
	}
	
	// Adjust for rounding errors - give extra to largest biome
	int32 Difference = TotalLandPixels - AssignedTotal;
	if (Difference != 0 && TargetPixelCounts.Num() > 0)
	{
		int32 LargestIdx = 0;
		for (int32 i = 1; i < TargetPixelCounts.Num(); ++i)
		{
			if (TargetPixelCounts[i] > TargetPixelCounts[LargestIdx])
			{
				LargestIdx = i;
			}
		}
		TargetPixelCounts[LargestIdx] += Difference;
	}
	
	// Flood-fill from each seed point simultaneously
	// Use a frontier-based approach where each biome expands from its seed
	TArray<TArray<FIntPoint>> Frontiers;
	Frontiers.SetNum(BiomeSettings.LandBiomes.Num());
	
	// Initialize frontiers with seed pixels
	for (int32 i = 0; i < BiomeSeedPixels.Num(); ++i)
	{
		FIntPoint SeedPixel = BiomeSeedPixels[i];
		int32 Index = SeedPixel.Y * Width + SeedPixel.X;
		
		if (LandMask[Index] && BiomeMap[Index] == -1)
		{
			BiomeMap[Index] = i;
			CurrentPixelCounts[i]++;
			Frontiers[i].Add(SeedPixel);
		}
	}
	
	// Direction offsets for 4-connectivity
	const FIntPoint Directions[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
	
	// Expand all biomes simultaneously until all land is assigned
	bool bAnyExpanded = true;
	while (bAnyExpanded)
	{
		bAnyExpanded = false;
		
		// Each biome tries to expand
		for (int32 BiomeIdx = 0; BiomeIdx < Frontiers.Num(); ++BiomeIdx)
		{
			// Skip if this biome has reached its target
			if (CurrentPixelCounts[BiomeIdx] >= TargetPixelCounts[BiomeIdx])
			{
				continue;
			}
			
			TArray<FIntPoint> NewFrontier;
			
			for (const FIntPoint& Pixel : Frontiers[BiomeIdx])
			{
				// Try to expand in all directions
				for (const FIntPoint& Dir : Directions)
				{
					FIntPoint Neighbor(Pixel.X + Dir.X, Pixel.Y + Dir.Y);
					
					// Check bounds
					if (Neighbor.X < 0 || Neighbor.X >= Width || Neighbor.Y < 0 || Neighbor.Y >= Height)
					{
						continue;
					}
					
					int32 NeighborIndex = Neighbor.Y * Width + Neighbor.X;
					
					// Check if it's unassigned land
					if (LandMask[NeighborIndex] && BiomeMap[NeighborIndex] == -1)
					{
						// Check if we still need more pixels
						if (CurrentPixelCounts[BiomeIdx] < TargetPixelCounts[BiomeIdx])
						{
							BiomeMap[NeighborIndex] = BiomeIdx;
							CurrentPixelCounts[BiomeIdx]++;
							NewFrontier.Add(Neighbor);
							bAnyExpanded = true;
						}
					}
				}
			}
			
			Frontiers[BiomeIdx] = MoveTemp(NewFrontier);
		}
	}
	
	// Assign any remaining unassigned land pixels to nearest biome
	for (const FIntPoint& LandPixel : LandPixels)
	{
		int32 Index = LandPixel.Y * Width + LandPixel.X;
		if (BiomeMap[Index] == -1)
		{
			// Find nearest assigned pixel and use its biome
			float MinDist = TNumericLimits<float>::Max();
			int32 NearestBiome = 0;
			
			for (int32 BiomeIdx = 0; BiomeIdx < BiomeSeedPixels.Num(); ++BiomeIdx)
			{
				float Dist = FVector2D::DistSquared(
					FVector2D(LandPixel.X, LandPixel.Y),
					FVector2D(BiomeSeedPixels[BiomeIdx].X, BiomeSeedPixels[BiomeIdx].Y)
				);
				if (Dist < MinDist)
				{
					MinDist = Dist;
					NearestBiome = BiomeIdx;
				}
			}
			
			BiomeMap[Index] = NearestBiome;
		}
	}
	
	// Log final counts
	for (int32 i = 0; i < BiomeSettings.LandBiomes.Num(); ++i)
	{
		UE_LOG(LogTemp, Log, TEXT("Biome %s: got %d pixels (target: %d)"),
			*BiomeSettings.LandBiomes[i].DisplayName, CurrentPixelCounts[i], TargetPixelCounts[i]);
	}
}

// Simple hash-based noise function
float UContinentMapGenerator::Noise2D(float X, float Y) const
{
	// Use seed to offset the noise - larger multiplier for more variation
	X += (Seed % 10000) * 0.37f;
	Y += (Seed % 10000) * 0.53f;
	
	int32 Xi = FMath::FloorToInt(X);
	int32 Yi = FMath::FloorToInt(Y);
	float Xf = X - Xi;
	float Yf = Y - Yi;
	
	// Smooth interpolation
	float U = Xf * Xf * (3.0f - 2.0f * Xf);
	float V = Yf * Yf * (3.0f - 2.0f * Yf);
	
	// Hash function for pseudo-random values at grid points - include seed
	int32 SeedHash = Seed;
	auto Hash = [SeedHash](int32 X, int32 Y) -> float
	{
		int32 N = X + Y * 57 + SeedHash;
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
			const int32 MapIndex = Y * Width + X;
			
			FLinearColor PixelColor;
			
			// Get biome index from pre-computed BiomeMap (-1 = ocean)
			int32 BiomeIndex = (MapIndex < BiomeMap.Num()) ? BiomeMap[MapIndex] : -1;
			
			if (BiomeIndex < 0)
			{
				// Ocean
				PixelColor = BiomeSettings.OceanColor;
			}
			else if (BiomeIndex < BiomeSettings.LandBiomes.Num())
			{
				// Valid biome
				PixelColor = BiomeSettings.LandBiomes[BiomeIndex].Color;
			}
			else
			{
				// Fallback
				PixelColor = FLinearColor::Green;
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

