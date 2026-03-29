#include "BiomeAssignmentGenerator.h"

//------------------------------------------------------------------------------
// Initialize: store settings, record resolution, allocate output buffer
//------------------------------------------------------------------------------
void UBiomeAssignmentGenerator::Initialize(const FBiomeAssignmentSettings& InSettings, int32 TextureResolution)
{
	Settings = InSettings;
	Resolution = TextureResolution;

	const int32 TotalPixels = Resolution * Resolution;
	BiomeMap.SetNumZeroed(TotalPixels);

	UE_LOG(LogTemp, Log,
		TEXT("BiomeAssignmentGenerator initialized – Resolution: %d, MountainThreshold: %.2f, VolcanicRadius: %d px"),
		Resolution, Settings.MountainElevationThreshold, Settings.VolcanicRadiusPixels);
}

//------------------------------------------------------------------------------
// Generate: classify each pixel into a biome using priority rules
//------------------------------------------------------------------------------
bool UBiomeAssignmentGenerator::Generate(const TArray<float>& Elevation, const TArray<float>& Temperature,
	const TArray<float>& Moisture, const TArray<uint8>& LandMask,
	const TArray<FVector2D>& VolcanicCenters)
{
	if (Resolution <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("BiomeAssignmentGenerator::Generate – Resolution not set. Call Initialize first."));
		return false;
	}

	const int32 TotalPixels = Resolution * Resolution;

	if (Elevation.Num() != TotalPixels || Temperature.Num() != TotalPixels
		|| Moisture.Num() != TotalPixels || LandMask.Num() != TotalPixels)
	{
		UE_LOG(LogTemp, Error,
			TEXT("BiomeAssignmentGenerator::Generate – Input array size mismatch (expected %d)"), TotalPixels);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator::Generate – starting (%d pixels, %d volcanic centers)"),
		TotalPixels, VolcanicCenters.Num());

	// Pre-compute volcanic center pixel coordinates
	TArray<FVector2D> VolcanicPixelPositions;
	VolcanicPixelPositions.Reserve(VolcanicCenters.Num());
	const float Res = static_cast<float>(Resolution);
	for (const FVector2D& Center : VolcanicCenters)
	{
		VolcanicPixelPositions.Add(FVector2D(Center.X * Res, Center.Y * Res));
	}

	const float VolcanicRadiusSq = static_cast<float>(Settings.VolcanicRadiusPixels)
		* static_cast<float>(Settings.VolcanicRadiusPixels);

	// Biome distribution counters
	TMap<EBiomeType, int32> BiomeCounts;

	for (int32 i = 0; i < TotalPixels; ++i)
	{
		EBiomeType Biome = EBiomeType::Land;

		// 1. Ocean
		if (LandMask[i] == 0)
		{
			Biome = EBiomeType::Ocean;
		}
		else
		{
			// 2. Volcanic — near a volcanic center AND elevation > 0.3
			bool bIsVolcanic = false;
			if (VolcanicPixelPositions.Num() > 0 && Elevation[i] > 0.3f)
			{
				const float PixelX = static_cast<float>(i % Resolution);
				const float PixelY = static_cast<float>(i / Resolution);

				for (const FVector2D& VPos : VolcanicPixelPositions)
				{
					const float DX = PixelX - VPos.X;
					const float DY = PixelY - VPos.Y;
					if (DX * DX + DY * DY < VolcanicRadiusSq)
					{
						bIsVolcanic = true;
						break;
					}
				}
			}

			if (bIsVolcanic)
			{
				Biome = EBiomeType::Volcanic;
			}
			// 3. Mountain
			else if (Elevation[i] > Settings.MountainElevationThreshold)
			{
				Biome = EBiomeType::Mountain;
			}
			// 4. Ice
			else if (Temperature[i] < Settings.IceTemperatureThreshold && Moisture[i] > Settings.IceMoistureThreshold)
			{
				Biome = EBiomeType::Ice;
			}
			// 5. Snow
			else if (Temperature[i] < Settings.SnowTemperatureThreshold)
			{
				Biome = EBiomeType::Snow;
			}
			// 6. Desert
			else if (Temperature[i] > Settings.DesertTemperatureThreshold && Moisture[i] < Settings.DesertMoistureThreshold)
			{
				Biome = EBiomeType::Desert;
			}
			// 7. Forest
			else if (Moisture[i] > Settings.ForestMoistureThreshold && Temperature[i] > Settings.ForestTemperatureThreshold)
			{
				Biome = EBiomeType::Forest;
			}
			// 8. Default — Land (grassland) already set
		}

		BiomeMap[i] = static_cast<int32>(Biome);
		BiomeCounts.FindOrAdd(Biome)++;
	}

	// Log biome distribution
	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator::Generate – Biome distribution:"));
	UE_LOG(LogTemp, Log, TEXT("  Ocean:    %d"), BiomeCounts.FindRef(EBiomeType::Ocean));
	UE_LOG(LogTemp, Log, TEXT("  Volcanic: %d"), BiomeCounts.FindRef(EBiomeType::Volcanic));
	UE_LOG(LogTemp, Log, TEXT("  Mountain: %d"), BiomeCounts.FindRef(EBiomeType::Mountain));
	UE_LOG(LogTemp, Log, TEXT("  Ice:      %d"), BiomeCounts.FindRef(EBiomeType::Ice));
	UE_LOG(LogTemp, Log, TEXT("  Snow:     %d"), BiomeCounts.FindRef(EBiomeType::Snow));
	UE_LOG(LogTemp, Log, TEXT("  Desert:   %d"), BiomeCounts.FindRef(EBiomeType::Desert));
	UE_LOG(LogTemp, Log, TEXT("  Forest:   %d"), BiomeCounts.FindRef(EBiomeType::Forest));
	UE_LOG(LogTemp, Log, TEXT("  Land:     %d"), BiomeCounts.FindRef(EBiomeType::Land));

	UE_LOG(LogTemp, Log, TEXT("BiomeAssignmentGenerator::Generate – complete"));
	return true;
}
