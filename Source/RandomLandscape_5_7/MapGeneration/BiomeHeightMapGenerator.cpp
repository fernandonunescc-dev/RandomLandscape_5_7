// BiomeHeightMapGenerator.cpp
// Delegates heightmap generation to individual biome terrain generators

#include "BiomeHeightMapGenerator.h"
#include "BiomeTerrainGenerators/BiomeTerrainGeneratorFactory.h"
#include "BiomeTerrainGenerators/BiomeTerrainGeneratorBase.h"

TArray<float> FBiomeHeightMapGenerator::GenerateBiomeHeightMap(
	EBiomeType BiomeType,
	const FBiomeMeshSettings& MeshSettings,
	int32 Resolution,
	int32 Seed,
	float MapSizeInMeters,
	bool bTileable)
{
	// Delegate to the biome-specific terrain generator
	const FBiomeTerrainGeneratorBase* Generator = FBiomeTerrainGeneratorFactory::Get().GetGenerator(BiomeType);
	if (Generator)
	{
		TArray<float> HeightMap = Generator->GenerateHeightMap(MeshSettings, Resolution, Seed, MapSizeInMeters, bTileable);
		UE_LOG(LogTemp, Log, TEXT("Generated %s heightmap: %dx%d via terrain generator"),
			*UEnum::GetValueAsString(BiomeType), Resolution, Resolution);
		return HeightMap;
	}

	// Fallback: return empty if no generator found
	UE_LOG(LogTemp, Warning, TEXT("No terrain generator found for %s, returning empty heightmap"),
		*UEnum::GetValueAsString(BiomeType));
	return TArray<float>();
}

TMap<EBiomeType, TArray<float>> FBiomeHeightMapGenerator::GenerateAllBiomeHeightMaps(
	const TArray<FBiomeMeshSettings>& BiomeSettings,
	int32 Resolution,
	int32 Seed,
	float MapSizeInMeters,
	bool bTileable)
{
	TMap<EBiomeType, TArray<float>> AllHeightMaps;

	for (const FBiomeMeshSettings& Settings : BiomeSettings)
	{
		// Use biome's own seed if specified (non-zero), otherwise derive from global seed
		int32 BiomeSeed = (Settings.Seed != 0) 
			? Settings.Seed 
			: Seed + static_cast<int32>(Settings.BiomeType) * 12345;
		
		TArray<float> HeightMap = GenerateBiomeHeightMap(
			Settings.BiomeType,
			Settings,
			Resolution,
			BiomeSeed,
			MapSizeInMeters,
			bTileable);

		if (HeightMap.Num() > 0)
		{
			AllHeightMaps.Add(Settings.BiomeType, MoveTemp(HeightMap));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Generated %d biome heightmaps at %dx%d resolution"), 
		AllHeightMaps.Num(), Resolution, Resolution);

	return AllHeightMaps;
}

float FBiomeHeightMapGenerator::SampleBlendedHeight(
	float NormX, float NormY,
	int32 HeightMapResolution,
	int32 BiomeTextureResolution,
	const TMap<EBiomeType, TArray<float>>& BiomeHeightMaps,
	const TArray<int32>& BiomeMap,
	const TArray<bool>& LandMask,
	const TArray<FBiomeConfig>& BiomeConfigs,
	float BlendRadius)
{
	// Clamp to valid range
	NormX = FMath::Clamp(NormX, 0.0f, 1.0f);
	NormY = FMath::Clamp(NormY, 0.0f, 1.0f);

	// For biome lookup, use the BiomeTextureResolution
	float BiomeGridX = NormX * (BiomeTextureResolution - 1);
	float BiomeGridY = NormY * (BiomeTextureResolution - 1);

	// Get the four corner indices for biome interpolation
	int32 BX0 = FMath::FloorToInt(BiomeGridX);
	int32 BY0 = FMath::FloorToInt(BiomeGridY);
	int32 BX1 = FMath::Min(BX0 + 1, BiomeTextureResolution - 1);
	int32 BY1 = FMath::Min(BY0 + 1, BiomeTextureResolution - 1);

	float BiomeFracX = BiomeGridX - BX0;
	float BiomeFracY = BiomeGridY - BY0;

	// Smoothstep for smoother biome transitions
	BiomeFracX = BiomeFracX * BiomeFracX * (3.0f - 2.0f * BiomeFracX);
	BiomeFracY = BiomeFracY * BiomeFracY * (3.0f - 2.0f * BiomeFracY);

	// For heightmap sampling, use the HeightMapResolution (high-res noise)
	float HeightGridX = NormX * (HeightMapResolution - 1);
	float HeightGridY = NormY * (HeightMapResolution - 1);

	int32 HX0 = FMath::FloorToInt(HeightGridX);
	int32 HY0 = FMath::FloorToInt(HeightGridY);
	int32 HX1 = FMath::Min(HX0 + 1, HeightMapResolution - 1);
	int32 HY1 = FMath::Min(HY0 + 1, HeightMapResolution - 1);

	float HeightFracX = HeightGridX - HX0;
	float HeightFracY = HeightGridY - HY0;

	// Smoothstep for smoother height transitions
	HeightFracX = HeightFracX * HeightFracX * (3.0f - 2.0f * HeightFracX);
	HeightFracY = HeightFracY * HeightFracY * (3.0f - 2.0f * HeightFracY);

	// Lambda to get biome type at a biome grid position
	auto GetBiomeAtPos = [&](int32 BX, int32 BY) -> EBiomeType
	{
		int32 Index = BY * BiomeTextureResolution + BX;
		if (Index < 0 || Index >= LandMask.Num())
		{
			return EBiomeType::Ocean;
		}

		if (LandMask[Index] && Index < BiomeMap.Num())
		{
			int32 BiomeIndex = BiomeMap[Index];
			if (BiomeIndex >= 0 && BiomeIndex < BiomeConfigs.Num())
			{
				return BiomeConfigs[BiomeIndex].BiomeType;
			}
		}

		return EBiomeType::Ocean;
	};

	// Lambda to sample height from a specific biome's heightmap using high-res coordinates
	auto SampleHeightFromBiome = [&](EBiomeType BiomeType) -> float
	{
		const TArray<float>* HeightMapPtr = BiomeHeightMaps.Find(BiomeType);
		if (!HeightMapPtr || HeightMapPtr->Num() == 0)
		{
			return 0.0f;
		}

		// Bilinear interpolation from the high-res heightmap
		int32 Idx00 = HY0 * HeightMapResolution + HX0;
		int32 Idx10 = HY0 * HeightMapResolution + HX1;
		int32 Idx01 = HY1 * HeightMapResolution + HX0;
		int32 Idx11 = HY1 * HeightMapResolution + HX1;

		// Safety bounds check
		int32 MaxIdx = HeightMapPtr->Num() - 1;
		Idx00 = FMath::Clamp(Idx00, 0, MaxIdx);
		Idx10 = FMath::Clamp(Idx10, 0, MaxIdx);
		Idx01 = FMath::Clamp(Idx01, 0, MaxIdx);
		Idx11 = FMath::Clamp(Idx11, 0, MaxIdx);

		float H00 = (*HeightMapPtr)[Idx00];
		float H10 = (*HeightMapPtr)[Idx10];
		float H01 = (*HeightMapPtr)[Idx01];
		float H11 = (*HeightMapPtr)[Idx11];

		float H0 = FMath::Lerp(H00, H10, HeightFracX);
		float H1 = FMath::Lerp(H01, H11, HeightFracX);

		return FMath::Lerp(H0, H1, HeightFracY);
	};

	// Get biome at 4 corners of the biome grid
	EBiomeType Biome00 = GetBiomeAtPos(BX0, BY0);
	EBiomeType Biome10 = GetBiomeAtPos(BX1, BY0);
	EBiomeType Biome01 = GetBiomeAtPos(BX0, BY1);
	EBiomeType Biome11 = GetBiomeAtPos(BX1, BY1);

	// Optimization: If all 4 corners are the same biome, just sample directly from
	// the high-res heightmap without biome blending overhead
	if (Biome00 == Biome10 && Biome00 == Biome01 && Biome00 == Biome11)
	{
		return SampleHeightFromBiome(Biome00);
	}

	// Multiple biomes present - need to blend between them
	// Sample heights from each biome's heightmap (using high-res sampling)
	float H00 = SampleHeightFromBiome(Biome00);
	float H10 = SampleHeightFromBiome(Biome10);
	float H01 = SampleHeightFromBiome(Biome01);
	float H11 = SampleHeightFromBiome(Biome11);

	// Bilinear interpolation of heights based on biome boundaries
	float H0 = FMath::Lerp(H00, H10, BiomeFracX);
	float H1 = FMath::Lerp(H01, H11, BiomeFracX);

	return FMath::Lerp(H0, H1, BiomeFracY);
}

