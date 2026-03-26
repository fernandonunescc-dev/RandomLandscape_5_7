// HeightmapGenerator.cpp
// Generates per-biome heightmap textures with biome-type-specific noise.
// Includes smooth boundary blending between adjacent biomes.

#include "HeightmapGenerator.h"
#include "Engine/Texture2D.h"
#include "Async/ParallelFor.h"

// ------------------------------------------------------------
// Constructor
// ------------------------------------------------------------
UHeightmapGenerator::UHeightmapGenerator()
{
}

// ------------------------------------------------------------
// Initialize
// ------------------------------------------------------------
void UHeightmapGenerator::Initialize(const FHeightmapSettings& InSettings)
{
	Settings = InSettings;

	if (Settings.HeightmapSeed == 0)
	{
		ActualSeed = static_cast<int32>(FPlatformTime::Cycles() & 0x7FFFFFFF);
		if (ActualSeed == 0) ActualSeed = 1;
	}
	else
	{
		ActualSeed = Settings.HeightmapSeed;
	}

	RandomStream.Initialize(ActualSeed);
}

// ------------------------------------------------------------
// Main generation
// ------------------------------------------------------------
bool UHeightmapGenerator::Generate(
	const TArray<int32>& BiomeMap,
	const TArray<uint8>& LandMask,
	const TArray<FBiomeLayerSettings>& BiomeLayers)
{
	const int32 Res = Settings.TextureResolution;
	const int32 TotalPixels = Res * Res;

	if (BiomeMap.Num() != TotalPixels || LandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error, TEXT("HeightmapGenerator::Generate - Size mismatch: expected %d, BiomeMap=%d, LandMask=%d"),
			TotalPixels, BiomeMap.Num(), LandMask.Num());
		return false;
	}

	Results.Reset();

	for (const FBiomeLayerSettings& Layer : BiomeLayers)
	{
		const int32 BiomeId = static_cast<int32>(Layer.BiomeType);

		// Find biome centroid and radius for volcano/mountain placement
		FVector2D Centroid(0.5f, 0.5f);
		float BiomeRadius = 0.2f;
		FindBiomeExtents(BiomeMap, Res, BiomeId, Centroid, BiomeRadius);

		// Compute distance field for boundary blending
		TArray<float> DistanceField;
		ComputeDistanceField(BiomeMap, Res, BiomeId, DistanceField);

		// Generate heightmap data for this biome
		TArray<float> HeightData;
		HeightData.SetNumZeroed(TotalPixels);

		const float BlendDist = static_cast<float>(Settings.BlendRadius);

		ParallelFor(TotalPixels, [&](int32 Index)
		{
			// Only compute for land pixels that belong to this biome or are near its boundary
			if (LandMask[Index] == 0)
			{
				HeightData[Index] = 0.0f;
				return;
			}

			const int32 PixelBiome = BiomeMap[Index];
			const float Dist = DistanceField[Index];

			// Skip if too far from this biome (beyond blend radius)
			if (PixelBiome != BiomeId && Dist > BlendDist)
			{
				HeightData[Index] = 0.0f;
				return;
			}

			const int32 X = Index % Res;
			const int32 Y = Index / Res;
			const float NormX = (Res > 1) ? static_cast<float>(X) / static_cast<float>(Res - 1) : 0.5f;
			const float NormY = (Res > 1) ? static_cast<float>(Y) / static_cast<float>(Res - 1) : 0.5f;

			// Compute raw height for this biome
			float Height = ComputeBiomeHeight(NormX, NormY, Layer.BiomeType,
				Layer.TerrainSettings, Centroid, BiomeRadius);

			if (PixelBiome == BiomeId)
			{
				// Inside the biome - retain full height for proper shape without attenuation
			}
			else
			{
				// Outside the biome but within blend radius - declining contribution for smooth merge
				float BlendFactor = FMath::Clamp(1.0f - (Dist / BlendDist), 0.0f, 1.0f);
				BlendFactor = BlendFactor * BlendFactor * (3.0f - 2.0f * BlendFactor);
				Height *= BlendFactor * 0.3f; // Reduced contribution outside own biome
			}

			HeightData[Index] = FMath::Clamp(Height, 0.0f, 1.0f);
		});

		// Create texture from height data
		FBiomeHeightmapResult Result;
		Result.BiomeType = Layer.BiomeType;
		Result.DisplayName = Layer.DisplayName;
		Result.HeightmapTexture = CreateHeightmapTexture(Res, HeightData);

		Results.Add(MoveTemp(Result));

		UE_LOG(LogTemp, Log, TEXT("HeightmapGenerator: Generated heightmap for %s (HeightScale=%.2f)"),
			*Layer.DisplayName, Layer.TerrainSettings.HeightScale);
	}

	UE_LOG(LogTemp, Log, TEXT("HeightmapGenerator::Generate - Generated %d biome heightmaps"), Results.Num());
	return true;
}

// ------------------------------------------------------------
// Biome-specific height computation
// ------------------------------------------------------------
float UHeightmapGenerator::ComputeBiomeHeight(
	float NormX, float NormY,
	EBiomeType BiomeType,
	const FBiomeTerrainSettings& Terrain,
	const FVector2D& BiomeCentroid,
	float BiomeRadius) const
{
	// Unique noise offset per biome type to decorrelate patterns
	const float BiomeOffset = static_cast<float>(static_cast<int32>(BiomeType)) * 137.0f + ActualSeed * 0.37f;

	float Height = 0.0f;

	switch (BiomeType)
	{
	case EBiomeType::Land:
	{
		// Very flat with occasional small hills
		float BaseNoise = FBM(
			NormX * Terrain.NoiseFrequency + BiomeOffset,
			NormY * Terrain.NoiseFrequency,
			Terrain.NoiseOctaves, Terrain.NoisePersistence);

		// Apply flatness: higher flatness = more suppressed noise
		float FlattenedNoise = BaseNoise * (1.0f - Terrain.Flatness);

		// Small hills
		float HillNoise = FBM(
			NormX * Terrain.HillFrequency + BiomeOffset + 50.0f,
			NormY * Terrain.HillFrequency,
			2, 0.4f);
		float Hills = FMath::Max(0.0f, HillNoise) * Terrain.HillHeight;

		Height = (FlattenedNoise * 0.5f + 0.5f) * 0.3f + Hills * 0.7f;
		break;
	}

	case EBiomeType::Forest:
	{
		// Taller hills and more uneven terrain than Land
		float BaseNoise = FBM(
			NormX * Terrain.NoiseFrequency + BiomeOffset,
			NormY * Terrain.NoiseFrequency,
			Terrain.NoiseOctaves, Terrain.NoisePersistence);

		float FlattenedNoise = BaseNoise * (1.0f - Terrain.Flatness);

		// More prominent hills
		float HillNoise = FBM(
			NormX * Terrain.HillFrequency + BiomeOffset + 50.0f,
			NormY * Terrain.HillFrequency,
			3, 0.5f);
		float Hills = FMath::Max(0.0f, HillNoise) * Terrain.HillHeight;

		// Extra variation for uneven terrain
		float DetailNoise = FBM(
			NormX * Terrain.NoiseFrequency * 3.0f + BiomeOffset + 100.0f,
			NormY * Terrain.NoiseFrequency * 3.0f,
			2, 0.3f) * 0.15f;

		Height = (FlattenedNoise * 0.5f + 0.5f) * 0.25f + Hills * 0.6f + DetailNoise + 0.1f;
		break;
	}

	case EBiomeType::Desert:
	{
		// Flat base with dune patterns
		float BaseNoise = FBM(
			NormX * Terrain.NoiseFrequency + BiomeOffset,
			NormY * Terrain.NoiseFrequency,
			Terrain.NoiseOctaves, Terrain.NoisePersistence);

		float FlatBase = BaseNoise * (1.0f - Terrain.Flatness) * 0.1f;

		// Dune ridges using absolute value of sine-modulated noise for ridge patterns
		float DuneAngle = FBM(
			NormX * 0.5f + BiomeOffset + 200.0f,
			NormY * 0.5f, 2, 0.5f) * PI;

		float DuneCoord = NormX * FMath::Cos(DuneAngle) + NormY * FMath::Sin(DuneAngle);
		float DuneWave = FMath::Sin(DuneCoord * Terrain.DuneFrequency * PI * 2.0f);

		// Sharpen the ridges
		float DuneRidge = FMath::Pow(FMath::Abs(DuneWave), 1.0f / Terrain.DuneRidgeSharpness);
		float Dunes = DuneRidge * Terrain.DuneHeight;

		// Add some noise variation to dunes
		float DuneNoise = FBM(
			NormX * Terrain.DuneFrequency * 0.5f + BiomeOffset + 300.0f,
			NormY * Terrain.DuneFrequency * 0.5f,
			2, 0.4f) * 0.15f + 0.85f;

		Height = FlatBase + 0.05f + Dunes * DuneNoise * 0.8f;
		break;
	}

	case EBiomeType::Snow:
	{
		// Same structure as Forest (per requirements: "Same as Forest")
		float BaseNoise = FBM(
			NormX * Terrain.NoiseFrequency + BiomeOffset,
			NormY * Terrain.NoiseFrequency,
			Terrain.NoiseOctaves, Terrain.NoisePersistence);

		float FlattenedNoise = BaseNoise * (1.0f - Terrain.Flatness);

		float HillNoise = FBM(
			NormX * Terrain.HillFrequency + BiomeOffset + 50.0f,
			NormY * Terrain.HillFrequency,
			3, 0.5f);
		float Hills = FMath::Max(0.0f, HillNoise) * Terrain.HillHeight;

		float DetailNoise = FBM(
			NormX * Terrain.NoiseFrequency * 3.0f + BiomeOffset + 100.0f,
			NormY * Terrain.NoiseFrequency * 3.0f,
			2, 0.3f) * 0.15f;

		Height = (FlattenedNoise * 0.5f + 0.5f) * 0.25f + Hills * 0.6f + DetailNoise + 0.1f;
		break;
	}

	case EBiomeType::Ice:
	{
		// Super flat with occasional iceberg/ice mountain peaks
		float BaseNoise = FBM(
			NormX * Terrain.NoiseFrequency + BiomeOffset,
			NormY * Terrain.NoiseFrequency,
			Terrain.NoiseOctaves, Terrain.NoisePersistence);

		float FlatBase = BaseNoise * (1.0f - Terrain.Flatness) * 0.05f;

		// Iceberg peaks: sparse, sharp features
		float IcebergNoise = FBM(
			NormX * Terrain.IcebergFrequency + BiomeOffset + 400.0f,
			NormY * Terrain.IcebergFrequency,
			3, 0.6f);

		// Only keep positive peaks and make them sparse
		float IcebergPeak = FMath::Max(0.0f, IcebergNoise - 0.4f) / 0.6f;
		IcebergPeak = FMath::Pow(IcebergPeak, 2.0f) * Terrain.IcebergHeight;

		Height = FlatBase + 0.02f + IcebergPeak;
		break;
	}

	case EBiomeType::Mountain:
	{
		// Strong peaked terrain - can reach full white (HeightScale=1.0)
		// Base mountain noise
		float BaseNoise = FBM(
			NormX * Terrain.NoiseFrequency + BiomeOffset,
			NormY * Terrain.NoiseFrequency,
			Terrain.NoiseOctaves, Terrain.NoisePersistence);

		// Create peak structure using ridged noise
		float RidgedNoise = 1.0f - FMath::Abs(BaseNoise);
		RidgedNoise = FMath::Pow(RidgedNoise, Terrain.PeakSharpness);

		// Additional peaks
		float PeakNoise = FBM(
			NormX * Terrain.NoiseFrequency * 0.7f + BiomeOffset + 500.0f,
			NormY * Terrain.NoiseFrequency * 0.7f,
			3, 0.5f);

		float Peaks = FMath::Max(0.0f, PeakNoise) * Terrain.MountainPeakHeight;

		// Combine ridged terrain with peaks
		Height = RidgedNoise * 0.5f + Peaks * 0.5f;

		// Add fine detail
		float Detail = FBM(
			NormX * Terrain.NoiseFrequency * 4.0f + BiomeOffset + 600.0f,
			NormY * Terrain.NoiseFrequency * 4.0f,
			2, 0.4f) * 0.1f;

		Height += Detail;
		break;
	}

	case EBiomeType::Volcanic:
	{
		// Volcano cone with crater
		// Distance from biome centroid in normalized space
		float DX = NormX - BiomeCentroid.X;
		float DY = NormY - BiomeCentroid.Y;
		float DistFromCenter = FMath::Sqrt(DX * DX + DY * DY);

		// Normalize by biome radius
		float NormDist = DistFromCenter / FMath::Max(BiomeRadius, 0.01f);

		// Volcano cone shape: rises from edges to center
		float ConeHeight = FMath::Max(0.0f, 1.0f - NormDist) * Terrain.VolcanoHeight;

		// Crater at the top
		float CraterDist = NormDist / FMath::Max(Terrain.CraterRadius, 0.01f);
		if (CraterDist < 1.0f)
		{
			// Inside crater - dip down
			float CraterFactor = 1.0f - CraterDist;
			CraterFactor = CraterFactor * CraterFactor; // Smooth bowl shape
			ConeHeight -= CraterFactor * Terrain.CraterDepth * Terrain.VolcanoHeight;
		}

		// Add noise for natural irregularity
		float VolcanoNoise = FBM(
			NormX * Terrain.NoiseFrequency + BiomeOffset,
			NormY * Terrain.NoiseFrequency,
			Terrain.NoiseOctaves, Terrain.NoisePersistence);

		Height = FMath::Max(0.0f, ConeHeight + VolcanoNoise * 0.15f);

		// Base terrain around volcano
		float BaseNoise = FBM(
			NormX * 2.0f + BiomeOffset + 700.0f,
			NormY * 2.0f, 3, 0.4f);
		float BaseTerrain = (BaseNoise * 0.5f + 0.5f) * 0.15f;

		Height = FMath::Max(Height, BaseTerrain);
		break;
	}

	default: // Ocean
		Height = 0.0f;
		break;
	}

	// Scale by biome's height range
	Height = FMath::Clamp(Height, 0.0f, 1.0f) * Terrain.HeightScale;

	return Height;
}

// ------------------------------------------------------------
// Noise2D
// ------------------------------------------------------------
float UHeightmapGenerator::Noise2D(float X, float Y) const
{
	X += (ActualSeed % 10000) * 0.37f;
	Y += (ActualSeed % 10000) * 0.53f;

	int32 Xi = FMath::FloorToInt(X);
	int32 Yi = FMath::FloorToInt(Y);
	float Xf = X - Xi;
	float Yf = Y - Yi;

	float U = Xf * Xf * (3.0f - 2.0f * Xf);
	float V = Yf * Yf * (3.0f - 2.0f * Yf);

	int32 SeedHash = ActualSeed;
	auto Hash = [SeedHash](int32 X, int32 Y) -> float
	{
		uint32 N = static_cast<uint32>(X) + static_cast<uint32>(Y) * 57u + static_cast<uint32>(SeedHash);
		N = (N << 13) ^ N;
		N = N * (N * N * 15731u + 789221u) + 1376312589u;
		return 1.0f - static_cast<float>(N & 0x7fffffffu) / 1073741824.0f;
	};

	float A = Hash(Xi, Yi);
	float B = Hash(Xi + 1, Yi);
	float C = Hash(Xi, Yi + 1);
	float D = Hash(Xi + 1, Yi + 1);

	float AB = FMath::Lerp(A, B, U);
	float CD = FMath::Lerp(C, D, U);
	return FMath::Lerp(AB, CD, V);
}

// ------------------------------------------------------------
// FBM
// ------------------------------------------------------------
float UHeightmapGenerator::FBM(float X, float Y, int32 Octaves, float Persistence) const
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

// ------------------------------------------------------------
// Compute distance field for blending at biome boundaries
// Uses BFS from boundary pixels to compute approximate distance
// ------------------------------------------------------------
void UHeightmapGenerator::ComputeDistanceField(
	const TArray<int32>& BiomeMap,
	int32 Resolution,
	int32 BiomeId,
	TArray<float>& OutDistances) const
{
	const int32 TotalPixels = Resolution * Resolution;
	OutDistances.SetNumUninitialized(TotalPixels);

	// Initialize: 0 for boundary pixels of this biome, large value elsewhere
	constexpr float LargeDistance = 99999.0f;
	TArray<int32> Queue;
	Queue.Reserve(TotalPixels / 4);

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		if (BiomeMap[i] == BiomeId)
		{
			// Check if this is a boundary pixel (has a non-same-biome neighbor)
			int32 X = i % Resolution;
			int32 Y = i / Resolution;
			bool bIsBoundary = false;

			const int32 DX[4] = { 1, -1, 0, 0 };
			const int32 DY[4] = { 0, 0, 1, -1 };

			for (int32 d = 0; d < 4; ++d)
			{
				int32 NX = X + DX[d];
				int32 NY = Y + DY[d];
				if (NX >= 0 && NX < Resolution && NY >= 0 && NY < Resolution)
				{
					int32 NI = NY * Resolution + NX;
					if (BiomeMap[NI] != BiomeId)
					{
						bIsBoundary = true;
						break;
					}
				}
				else
				{
					bIsBoundary = true;
					break;
				}
			}

			if (bIsBoundary)
			{
				OutDistances[i] = 0.0f;
				Queue.Add(i);
			}
			else
			{
				OutDistances[i] = LargeDistance;
			}
		}
		else
		{
			OutDistances[i] = LargeDistance;
		}
	}

	// BFS to propagate distances
	int32 QueueHead = 0;
	while (QueueHead < Queue.Num())
	{
		int32 CurrentIdx = Queue[QueueHead++];
		int32 CX = CurrentIdx % Resolution;
		int32 CY = CurrentIdx / Resolution;
		float CurrentDist = OutDistances[CurrentIdx];

		const int32 DX[4] = { 1, -1, 0, 0 };
		const int32 DY[4] = { 0, 0, 1, -1 };

		for (int32 d = 0; d < 4; ++d)
		{
			int32 NX = CX + DX[d];
			int32 NY = CY + DY[d];

			if (NX < 0 || NX >= Resolution || NY < 0 || NY >= Resolution)
			{
				continue;
			}

			int32 NI = NY * Resolution + NX;
			float NewDist = CurrentDist + 1.0f;

			if (NewDist < OutDistances[NI])
			{
				OutDistances[NI] = NewDist;
				// Only continue BFS within blend radius to keep it efficient
				if (NewDist < static_cast<float>(Settings.BlendRadius) * 2.0f)
				{
					Queue.Add(NI);
				}
			}
		}
	}
}

// ------------------------------------------------------------
// Find biome centroid and approximate radius
// ------------------------------------------------------------
void UHeightmapGenerator::FindBiomeExtents(
	const TArray<int32>& BiomeMap,
	int32 Resolution,
	int32 BiomeId,
	FVector2D& OutCentroid,
	float& OutRadius) const
{
	int32 Count = 0;
	float SumX = 0.0f;
	float SumY = 0.0f;

	for (int32 i = 0; i < BiomeMap.Num(); ++i)
	{
		if (BiomeMap[i] == BiomeId)
		{
			int32 X = i % Resolution;
			int32 Y = i / Resolution;
			SumX += static_cast<float>(X);
			SumY += static_cast<float>(Y);
			Count++;
		}
	}

	if (Count == 0)
	{
		OutCentroid = FVector2D(0.5f, 0.5f);
		OutRadius = 0.1f;
		return;
	}

	// Centroid in normalized coordinates
	float CentroidX = SumX / static_cast<float>(Count);
	float CentroidY = SumY / static_cast<float>(Count);
	OutCentroid = FVector2D(
		CentroidX / static_cast<float>(Resolution - 1),
		CentroidY / static_cast<float>(Resolution - 1)
	);

	// Approximate radius: average distance from centroid
	float SumDist = 0.0f;
	float MaxDist = 0.0f;
	for (int32 i = 0; i < BiomeMap.Num(); ++i)
	{
		if (BiomeMap[i] == BiomeId)
		{
			float X = static_cast<float>(i % Resolution) / static_cast<float>(Resolution - 1);
			float Y = static_cast<float>(i / Resolution) / static_cast<float>(Resolution - 1);
			float Dist = FMath::Sqrt(
				(X - OutCentroid.X) * (X - OutCentroid.X) +
				(Y - OutCentroid.Y) * (Y - OutCentroid.Y));
			SumDist += Dist;
			MaxDist = FMath::Max(MaxDist, Dist);
		}
	}

	// Use a blend of average and max distance for radius
	OutRadius = FMath::Max(0.01f, (SumDist / static_cast<float>(Count)) * 0.7f + MaxDist * 0.3f);
}

// ------------------------------------------------------------
// Create heightmap texture from float data
// ------------------------------------------------------------
UTexture2D* UHeightmapGenerator::CreateHeightmapTexture(int32 Resolution, const TArray<float>& HeightData) const
{
	if (Resolution <= 0 || HeightData.Num() != Resolution * Resolution)
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Resolution, Resolution, PF_B8G8R8A8);
	if (!Texture)
	{
		UE_LOG(LogTemp, Error, TEXT("HeightmapGenerator: Failed to create transient texture"));
		return nullptr;
	}

	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->Filter = TF_Nearest;
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_VectorDisplacementmap;

	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Pixels = static_cast<uint8*>(Data);

	for (int32 i = 0; i < HeightData.Num(); ++i)
	{
		uint8 Val = static_cast<uint8>(FMath::Clamp(HeightData[i] * 255.0f, 0.0f, 255.0f));
		int32 PixelIdx = i * 4;
		Pixels[PixelIdx + 0] = Val; // B
		Pixels[PixelIdx + 1] = Val; // G
		Pixels[PixelIdx + 2] = Val; // R
		Pixels[PixelIdx + 3] = 255; // A
	}

	Mip.BulkData.Unlock();
	Texture->UpdateResource();

	return Texture;
}
