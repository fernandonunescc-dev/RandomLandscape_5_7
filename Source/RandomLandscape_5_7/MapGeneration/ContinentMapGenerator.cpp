// ContinentMapGenerator.cpp
// Implementation of the Continent map generator

#include "ContinentMapGenerator.h"
#include "Engine/Texture2D.h"

UContinentMapGenerator::UContinentMapGenerator()
{
	PreviewTexture = nullptr;
	Seed = 0;
	BiomeSeed = 0;
}

void UContinentMapGenerator::Initialize(const FMapGenerationSettings& InSettings)
{
	Super::Initialize(InSettings);
	
	// Use 512 for high biome detail - performance impact is minimal
	TextureResolution = 512;
	
	// Initialize random streams with provided seeds
	// (Actor is responsible for generating random seeds when seed == 0)
	RandomStream.Initialize(Seed);
	BiomeRandomStream.Initialize(BiomeSeed);
	
	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator initialized - Size: %d meters (%.0f UU), Resolution: %d, LandmassSeed: %d, BiomeSeed: %d"),
		Settings.MapSizeInMeters, 
		Settings.GetMapSizeInUnrealUnits(),
		TextureResolution,
		Seed,
		BiomeSeed);

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

	// Full generation: landmass + biomes
	if (!GenerateLandmassOnly())
	{
		return false;
	}
	
	return GenerateBiomesOnly();
}

bool UContinentMapGenerator::GenerateLandmassOnly()
{
	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateLandmassOnly - Generating land mask"));
	
	// Pass 1: Generate the land mask (determine which pixels are land)
	GenerateLandMask();
	
	// Generate a black/white preview texture showing just the landmass
	GeneratePreviewTexture();
	
	return true;
}

bool UContinentMapGenerator::GenerateBiomesOnly()
{
	if (LandMask.Num() == 0 || LandPixels.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GenerateBiomesOnly - No landmass data. Call GenerateLandmassOnly first."));
		return false;
	}

	// Initialize biome random stream with the provided seed
	BiomeRandomStream.Initialize(BiomeSeed);
	
	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateBiomesOnly - Using biome seed: %d"), BiomeSeed);

	// Validate biome percentages
	if (!BiomeSettings.ArePercentagesValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("ContinentMapGenerator::GenerateBiomesOnly - Biome percentages don't add up to 100%% (Total: %.1f%%). Normalizing..."),
			BiomeSettings.GetTotalPercentage());
		BiomeSettings.NormalizePercentages();
	}

	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateBiomesOnly - Assigning %d biomes to %d land pixels"),
		BiomeSettings.LandBiomes.Num(), LandPixels.Num());
	
	// Pass 2: Assign biomes to land pixels using flood-fill
	AssignBiomesToLand();
	
	// Pass 3: Generate the final preview texture with biome colors
	GeneratePreviewTexture();
	
	// Pass 4: Generate individual mask textures for each biome
	GenerateBiomeMaskTextures();
	
	return true;
}

// Pass 1: Generate the land mask
void UContinentMapGenerator::GenerateLandMask()
{
	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;
	const int32 TotalPixels = Width * Height;
	
	// Clear previous data
	LandMask.Empty();
	LandMask.SetNum(TotalPixels);
	LandPixels.Empty();
	
	// First pass: collect all continent mask values
	TArray<float> MaskValues;
	MaskValues.SetNum(TotalPixels);
	
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			float NormX = static_cast<float>(X) / static_cast<float>(Width);
			float NormY = static_cast<float>(Y) / static_cast<float>(Height);
			
			int32 Index = Y * Width + X;
			MaskValues[Index] = GetContinentMask(NormX, NormY);
		}
	}
	
	// Calculate the threshold to achieve desired land coverage
	// Sort a copy of mask values to find the percentile threshold
	TArray<float> SortedValues = MaskValues;
	SortedValues.Sort([](float A, float B) { return A > B; }); // Sort descending (highest first)
	
	// Calculate how many pixels should be land (clamped to 5-75% to prevent edge case issues)
	float TargetLandPercent = FMath::Clamp(BiomeSettings.LandCoveragePercent, 5.0f, 75.0f) / 100.0f;
	int32 TargetLandPixels = FMath::RoundToInt(TotalPixels * TargetLandPercent);
	TargetLandPixels = FMath::Clamp(TargetLandPixels, 1, TotalPixels - 1);
	
	// The threshold is the value at the target land pixel count index
	// All pixels with values >= this threshold will be land
	float LandThreshold = SortedValues[TargetLandPixels - 1];
	
	UE_LOG(LogTemp, Log, TEXT("Land coverage target: %.1f%% (%d pixels), threshold: %.3f"),
		BiomeSettings.LandCoveragePercent, TargetLandPixels, LandThreshold);
	
	// Second pass: apply threshold to create land mask
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			int32 Index = Y * Width + X;
			bool bIsLand = MaskValues[Index] >= LandThreshold;
			
			LandMask[Index] = bIsLand;
			
			if (bIsLand)
			{
				LandPixels.Add(FIntPoint(X, Y));
			}
		}
	}

	// Note: Edge padding is now handled organically in GetContinentMask() 
	// using noise-modulated falloff for natural-looking irregular coastlines

	UE_LOG(LogTemp, Log, TEXT("Land mask generated: %d land pixels out of %d total (%.1f%%)"),
		LandPixels.Num(), TotalPixels, 
		(float)LandPixels.Num() / (float)TotalPixels * 100.0f);
}

// Pass 2: Assign biomes to land pixels - start from edges, grow organically
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
	
	// Direction offsets for 8-connectivity
	const FIntPoint Directions[] = { 
		FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1),
		FIntPoint(1, 1), FIntPoint(-1, 1), FIntPoint(1, -1), FIntPoint(-1, -1)
	};
	const int32 NumDirections = 8;
	
	// Find edge pixels (land pixels adjacent to ocean)
	TArray<FIntPoint> EdgePixels;
	for (const FIntPoint& Pixel : LandPixels)
	{
		//int32 Index = Pixel.Y * Width + Pixel.X; // unused
		bool bIsEdge = false;

		for (int32 d = 0; d < NumDirections; ++d)
		{
			FIntPoint Neighbor(Pixel.X + Directions[d].X, Pixel.Y + Directions[d].Y);

			// Check if neighbor is out of bounds or ocean
			if (Neighbor.X < 0 || Neighbor.X >= Width || Neighbor.Y < 0 || Neighbor.Y >= Height)
			{
				bIsEdge = true;
				break;
			}

			int32 NeighborIndex = Neighbor.Y * Width + Neighbor.X;
			if (!LandMask[NeighborIndex])
			{
				bIsEdge = true;
				break;
			}
		}

		if (bIsEdge)
		{
			EdgePixels.Add(Pixel);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Found %d edge pixels out of %d land pixels"), EdgePixels.Num(), TotalLandPixels);
	
	// Calculate target pixel counts for each biome and sort by size (smallest first)
	const float TotalPercentage = BiomeSettings.GetTotalPercentage();
	
	struct FBiomeTarget
	{
		int32 BiomeIndex;
		int32 TargetCount;
		int32 CurrentCount;
		float Percentage;
	};
	
	TArray<FBiomeTarget> BiomeTargets;
	int32 AssignedTotal = 0;
	
	for (int32 i = 0; i < BiomeSettings.LandBiomes.Num(); ++i)
	{
		const FBiomeConfig& Biome = BiomeSettings.LandBiomes[i];
		float NormalizedPercentage = Biome.Percentage / TotalPercentage;
		int32 TargetCount = FMath::RoundToInt(NormalizedPercentage * TotalLandPixels);
		TargetCount = FMath::Max(1, TargetCount);
		
		FBiomeTarget Target;
		Target.BiomeIndex = i;
		Target.TargetCount = TargetCount;
		Target.CurrentCount = 0;
		Target.Percentage = Biome.Percentage;
		BiomeTargets.Add(Target);
		AssignedTotal += TargetCount;
		
		UE_LOG(LogTemp, Log, TEXT("Biome %s: target %d pixels (%.1f%%)"),
			*Biome.DisplayName, TargetCount, Biome.Percentage);
	}
	
	// Sort by target count (smallest first)
	BiomeTargets.Sort([](const FBiomeTarget& A, const FBiomeTarget& B) {
		return A.TargetCount < B.TargetCount;
	});
	
	// Adjust for rounding errors - give extra to largest biome (last in sorted array)
	int32 Difference = TotalLandPixels - AssignedTotal;
	if (Difference != 0 && BiomeTargets.Num() > 0)
	{
		BiomeTargets.Last().TargetCount += Difference;
	}
	
	// Track unassigned land pixels
	TSet<int32> UnassignedLandIndices;
	for (const FIntPoint& Pixel : LandPixels)
	{
		UnassignedLandIndices.Add(Pixel.Y * Width + Pixel.X);
	}
	
	// Process each biome from smallest to largest
	for (FBiomeTarget& Target : BiomeTargets)
	{
		int32 BiomeIdx = Target.BiomeIndex;
		int32 PixelsNeeded = Target.TargetCount;
		
		while (Target.CurrentCount < PixelsNeeded && UnassignedLandIndices.Num() > 0)
		{
			// Find a starting point - prefer edge pixels that are unassigned
			FIntPoint StartPixel(-1, -1);
			
			// Shuffle edge pixels and find an unassigned one
			for (int32 Attempt = 0; Attempt < EdgePixels.Num(); ++Attempt)
			{
				int32 RandIdx = BiomeRandomStream.RandRange(0, EdgePixels.Num() - 1);
				FIntPoint CandidatePixel = EdgePixels[RandIdx];
				int32 CandidateIndex = CandidatePixel.Y * Width + CandidatePixel.X;
				
				if (UnassignedLandIndices.Contains(CandidateIndex))
				{
					StartPixel = CandidatePixel;
					break;
				}
			}
			
			// If no edge pixel available, pick any unassigned land pixel
			if (StartPixel.X < 0)
			{
				for (int32 Index : UnassignedLandIndices)
				{
					StartPixel.X = Index % Width;
					StartPixel.Y = Index / Width;
					break;
				}
			}
			
			if (StartPixel.X < 0)
			{
				break; // No more unassigned pixels
			}
			
			// Grow from this starting point using random walk / blob growth
			TArray<FIntPoint> Frontier;
			int32 StartIndex = StartPixel.Y * Width + StartPixel.X;
			
			BiomeMap[StartIndex] = BiomeIdx;
			Target.CurrentCount++;
			UnassignedLandIndices.Remove(StartIndex);
			Frontier.Add(StartPixel);
			
			// Grow organically until we reach target or run out of space
			while (Target.CurrentCount < PixelsNeeded && Frontier.Num() > 0)
			{
				// Pick a random frontier pixel to expand from
				int32 FrontierIdx = BiomeRandomStream.RandRange(0, Frontier.Num() - 1);
				FIntPoint CurrentPixel = Frontier[FrontierIdx];
				
				// Collect all valid neighbors
				TArray<FIntPoint> ValidNeighbors;
				for (int32 d = 0; d < NumDirections; ++d)
				{
					FIntPoint Neighbor(CurrentPixel.X + Directions[d].X, CurrentPixel.Y + Directions[d].Y);
					
					if (Neighbor.X < 0 || Neighbor.X >= Width || Neighbor.Y < 0 || Neighbor.Y >= Height)
					{
						continue;
					}
					
					int32 NeighborIndex = Neighbor.Y * Width + Neighbor.X;
					if (UnassignedLandIndices.Contains(NeighborIndex))
					{
						ValidNeighbors.Add(Neighbor);
					}
				}
				
				if (ValidNeighbors.Num() > 0)
				{
					// Pick a random neighbor to claim
					int32 RandNeighbor = BiomeRandomStream.RandRange(0, ValidNeighbors.Num() - 1);
					FIntPoint ChosenNeighbor = ValidNeighbors[RandNeighbor];
					int32 ChosenIndex = ChosenNeighbor.Y * Width + ChosenNeighbor.X;
					
					BiomeMap[ChosenIndex] = BiomeIdx;
					Target.CurrentCount++;
					UnassignedLandIndices.Remove(ChosenIndex);
					Frontier.Add(ChosenNeighbor);
				}
				else
				{
					// This frontier pixel has no more valid neighbors, remove it
					Frontier.RemoveAt(FrontierIdx);
				}
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("Biome %d: assigned %d pixels (target: %d)"),
			BiomeIdx, Target.CurrentCount, Target.TargetCount);
	}
	
	// Assign any remaining unassigned pixels to the largest biome
	if (UnassignedLandIndices.Num() > 0 && BiomeTargets.Num() > 0)
	{
		int32 LargestBiomeIdx = BiomeTargets.Last().BiomeIndex;
		for (int32 Index : UnassignedLandIndices)
		{
			BiomeMap[Index] = LargestBiomeIdx;
		}
		UE_LOG(LogTemp, Log, TEXT("Assigned %d remaining pixels to largest biome"), UnassignedLandIndices.Num());
	}
	
	// Smoothing pass - remove spiky edges by checking if a pixel is "surrounded" by another biome
	// Run multiple passes for smoother results
	const int32 SmoothingPasses = 3;
	
	for (int32 Pass = 0; Pass < SmoothingPasses; ++Pass)
	{
		TArray<int32> NewBiomeMap = BiomeMap;
		
		for (const FIntPoint& Pixel : LandPixels)
		{
			int32 Index = Pixel.Y * Width + Pixel.X;
			int32 CurrentBiome = BiomeMap[Index];
			
			if (CurrentBiome < 0) continue; // Skip ocean
			
			// Count neighbors of each biome type
			TMap<int32, int32> NeighborCounts;
			int32 TotalLandNeighbors = 0;
			
			for (int32 d = 0; d < NumDirections; ++d)
			{
				FIntPoint Neighbor(Pixel.X + Directions[d].X, Pixel.Y + Directions[d].Y);
				
				if (Neighbor.X < 0 || Neighbor.X >= Width || Neighbor.Y < 0 || Neighbor.Y >= Height)
				{
					continue;
				}
				
				int32 NeighborIndex = Neighbor.Y * Width + Neighbor.X;
				int32 NeighborBiome = BiomeMap[NeighborIndex];
				
				if (NeighborBiome >= 0) // Only count land neighbors
				{
					NeighborCounts.FindOrAdd(NeighborBiome)++;
					TotalLandNeighbors++;
				}
			}
			
			// If this pixel is mostly surrounded by a different biome, change it
			// Threshold: if more than 5 out of 8 neighbors (or 62.5%) are a different biome
			if (TotalLandNeighbors >= 5)
			{
				int32 DominantBiome = CurrentBiome;
				int32 DominantCount = 0;
				
				for (const auto& Pair : NeighborCounts)
				{
					if (Pair.Value > DominantCount)
					{
						DominantCount = Pair.Value;
						DominantBiome = Pair.Key;
					}
				}
				
				// Change if dominant biome is different and has strong majority
				if (DominantBiome != CurrentBiome && DominantCount >= 5)
				{
					NewBiomeMap[Index] = DominantBiome;
				}
			}
		}
		
		BiomeMap = MoveTemp(NewBiomeMap);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Applied %d smoothing passes to biome boundaries"), SmoothingPasses);
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
	
	// ========== Organic edge falloff ==========
	// Create natural-looking edges near texture borders using noise-modulated falloff
	// Calculate distance from each edge (0 at edge, increases toward center)
	float DistFromLeft = NormX;
	float DistFromRight = 1.0f - NormX;
	float DistFromTop = NormY;
	float DistFromBottom = 1.0f - NormY;
	
	// Find minimum distance to any edge
	float MinEdgeDist = FMath::Min(FMath::Min(DistFromLeft, DistFromRight), FMath::Min(DistFromTop, DistFromBottom));
	
	// Add noise to the edge distance for irregular coastlines near borders
	float EdgeNoise = FBM(NormX * 6.0f + 200.0f, NormY * 6.0f + 200.0f, 3, 0.5f) * 0.5f + 0.5f; // 0 to 1 range
	
	// Base padding of ~5% of texture, modulated by noise (varies between 2.5% and 7.5%)
	float NoisyPadding = 0.05f * (0.5f + EdgeNoise);
	
	// Create smooth falloff: 0 at edge, 1 when past the noisy padding zone
	float EdgeFalloff = FMath::Clamp(MinEdgeDist / FMath::Max(NoisyPadding, 0.001f), 0.0f, 1.0f);
	
	// Apply smooth falloff curve (smoothstep for gradual transition)
	EdgeFalloff = EdgeFalloff * EdgeFalloff * (3.0f - 2.0f * EdgeFalloff);
	
	// Reduce continent value near edges
	ContinentValue *= EdgeFalloff;
	
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

	// For a landmask texture we don't want sRGB correction and prefer nearest filtering for crisp pixels
	PreviewTexture->MipGenSettings = TMGS_NoMipmaps;
	PreviewTexture->SRGB = false;
	PreviewTexture->Filter = TF_Nearest;

	// Lock the texture for writing
	FTexture2DMipMap& Mip = PreviewTexture->GetPlatformData()->Mips[0];
	void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(TextureData);

	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;

	// Check if biomes have been assigned
	bool bHasBiomes = BiomeMap.Num() > 0 && BiomeSettings.LandBiomes.Num() > 0;

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 PixelIndex = (Y * Width + X) * 4;
			const int32 MapIndex = Y * Width + X;
			
			bool bIsLand = false;
			if (MapIndex < LandMask.Num())
			{
				bIsLand = LandMask[MapIndex];
			}
			
			FLinearColor PixelColor;
			
			if (!bIsLand)
			{
				// Ocean - use ocean color if we have biomes, otherwise black
				if (bHasBiomes)
				{
					PixelColor = BiomeSettings.OceanColor;
				}
				else
				{
					PixelColor = FLinearColor::Black;
				}
			}
			else if (bHasBiomes)
			{
				// Land with biomes - use biome color
				int32 BiomeIndex = (MapIndex < BiomeMap.Num()) ? BiomeMap[MapIndex] : -1;
				
				if (BiomeIndex >= 0 && BiomeIndex < BiomeSettings.LandBiomes.Num())
				{
					PixelColor = BiomeSettings.LandBiomes[BiomeIndex].Color;
				}
				else
				{
					// Fallback - default green for unassigned land
					PixelColor = FLinearColor(0.2f, 0.6f, 0.2f, 1.0f);
				}
			}
			else
			{
				// Land without biomes - white
				PixelColor = FLinearColor::White;
			}
			
			// Convert to 8-bit and write (BGRA format)
			FColor FinalColor = PixelColor.ToFColor(false);
			Pixels[PixelIndex + 0] = FinalColor.B;
			Pixels[PixelIndex + 1] = FinalColor.G;
			Pixels[PixelIndex + 2] = FinalColor.R;
			Pixels[PixelIndex + 3] = 255;
		}
	}

	// Unlock and update the texture
	Mip.BulkData.Unlock();
	PreviewTexture->UpdateResource();

	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GeneratePreviewTexture - Created %dx%d %s texture"),
		TextureResolution, TextureResolution, bHasBiomes ? TEXT("biome") : TEXT("landmask"));
}

void UContinentMapGenerator::GenerateBiomeMaskTextures()
{
	const int32 Width = TextureResolution;
	const int32 Height = TextureResolution;
	
	if (BiomeMap.Num() == 0 || BiomeSettings.LandBiomes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - No biome data available"));
		return;
	}
	
	// Generate ocean mask texture first
	{
		UTexture2D* OceanMaskTexture = UTexture2D::CreateTransient(TextureResolution, TextureResolution, PF_B8G8R8A8);
		if (OceanMaskTexture)
		{
			OceanMaskTexture->MipGenSettings = TMGS_NoMipmaps;
			OceanMaskTexture->SRGB = false;
			OceanMaskTexture->Filter = TF_Nearest;
			
			FTexture2DMipMap& Mip = OceanMaskTexture->GetPlatformData()->Mips[0];
			void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
			uint8* Pixels = static_cast<uint8*>(TextureData);
			
			FColor OceanColor = BiomeSettings.OceanColor.ToFColor(false);
			
			for (int32 Y = 0; Y < Height; ++Y)
			{
				for (int32 X = 0; X < Width; ++X)
				{
					const int32 PixelIndex = (Y * Width + X) * 4;
					const int32 MapIndex = Y * Width + X;
					
					bool bIsLand = (MapIndex < LandMask.Num()) ? LandMask[MapIndex] : false;
					
					if (!bIsLand)
					{
						// Ocean pixel - use ocean color
						Pixels[PixelIndex + 0] = OceanColor.B;
						Pixels[PixelIndex + 1] = OceanColor.G;
						Pixels[PixelIndex + 2] = OceanColor.R;
						Pixels[PixelIndex + 3] = 255;
					}
					else
					{
						// Land - black
						Pixels[PixelIndex + 0] = 0;
						Pixels[PixelIndex + 1] = 0;
						Pixels[PixelIndex + 2] = 0;
						Pixels[PixelIndex + 3] = 255;
					}
				}
			}
			
			Mip.BulkData.Unlock();
			OceanMaskTexture->UpdateResource();
			
			MeshSettings.OceanSettings.MaskTexture = OceanMaskTexture;
			UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - Created ocean mask texture"));
		}
	}
	
	// Generate a mask texture for each land biome
	for (int32 BiomeIdx = 0; BiomeIdx < BiomeSettings.LandBiomes.Num(); ++BiomeIdx)
	{
		const FBiomeConfig& Biome = BiomeSettings.LandBiomes[BiomeIdx];
		
		// Find matching mesh settings for this biome
		FBiomeMeshSettings* MeshSettingsForBiome = MeshSettings.GetSettingsForBiomeMutable(Biome.BiomeType);
		if (!MeshSettingsForBiome)
		{
			UE_LOG(LogTemp, Warning, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - No mesh settings for biome %s"), *Biome.DisplayName);
			continue;
		}
		
		// Create the texture
		UTexture2D* MaskTexture = UTexture2D::CreateTransient(TextureResolution, TextureResolution, PF_B8G8R8A8);
		if (!MaskTexture)
		{
			UE_LOG(LogTemp, Error, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - Failed to create mask texture for biome %s"), *Biome.DisplayName);
			continue;
		}
		
		// Configure texture settings
		MaskTexture->MipGenSettings = TMGS_NoMipmaps;
		MaskTexture->SRGB = false;
		MaskTexture->Filter = TF_Nearest;
		
		// Lock the texture for writing
		FTexture2DMipMap& Mip = MaskTexture->GetPlatformData()->Mips[0];
		void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
		uint8* Pixels = static_cast<uint8*>(TextureData);
		
		// Get biome color
		FColor BiomeColor = Biome.Color.ToFColor(false);
		
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				const int32 PixelIndex = (Y * Width + X) * 4;
				const int32 MapIndex = Y * Width + X;
				
				// Check if this pixel belongs to this biome
				int32 PixelBiomeIdx = (MapIndex < BiomeMap.Num()) ? BiomeMap[MapIndex] : -1;
				
				if (PixelBiomeIdx == BiomeIdx)
				{
					// This pixel belongs to this biome - use biome color
					Pixels[PixelIndex + 0] = BiomeColor.B;
					Pixels[PixelIndex + 1] = BiomeColor.G;
					Pixels[PixelIndex + 2] = BiomeColor.R;
					Pixels[PixelIndex + 3] = 255;
				}
				else
				{
					// Not this biome - black
					Pixels[PixelIndex + 0] = 0;
					Pixels[PixelIndex + 1] = 0;
					Pixels[PixelIndex + 2] = 0;
					Pixels[PixelIndex + 3] = 255;
				}
			}
		}
		
		// Unlock and update the texture
		Mip.BulkData.Unlock();
		MaskTexture->UpdateResource();
		
		// Assign to the mesh settings
		MeshSettingsForBiome->MaskTexture = MaskTexture;
		
		UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - Created mask texture for biome %s"), *Biome.DisplayName);
	}
	
	UE_LOG(LogTemp, Log, TEXT("ContinentMapGenerator::GenerateBiomeMaskTextures - Generated %d biome mask textures + ocean"), BiomeSettings.LandBiomes.Num());
}
