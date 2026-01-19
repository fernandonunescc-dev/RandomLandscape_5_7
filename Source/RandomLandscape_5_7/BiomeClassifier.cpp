// Copyright Epic Games, Inc. All Rights Reserved.

#include "BiomeClassifier.h"

FBiomeClassifier::FBiomeClassifier()
{
	InitializeDefaultBiomes();
}

void FBiomeClassifier::InitializeDefaultBiomes()
{
	BiomeDefinitions.Empty();

	// Ocean: Height < -500cm
	FBiomeDefinition OceanBiome;
	OceanBiome.BiomeType = EBiomeType::Ocean;
	OceanBiome.MinHeight = -FLT_MAX;
	OceanBiome.MaxHeight = -500.0f;
	OceanBiome.BiomeName = TEXT("Ocean");
	OceanBiome.DebugColor = FColor::Blue;
	BiomeDefinitions.Add(EBiomeType::Ocean, OceanBiome);

	// Lake: Height -500 to 0cm
	FBiomeDefinition LakeBiome;
	LakeBiome.BiomeType = EBiomeType::Lake;
	LakeBiome.MinHeight = -500.0f;
	LakeBiome.MaxHeight = 0.0f;
	LakeBiome.BiomeName = TEXT("Lake");
	LakeBiome.DebugColor = FColor::Cyan;
	BiomeDefinitions.Add(EBiomeType::Lake, LakeBiome);

	// Plains: Height 0 to 3000cm
	FBiomeDefinition PlainsBiome;
	PlainsBiome.BiomeType = EBiomeType::Plains;
	PlainsBiome.MinHeight = 0.0f;
	PlainsBiome.MaxHeight = 3000.0f;
	PlainsBiome.BiomeName = TEXT("Plains");
	PlainsBiome.DebugColor = FColor::Green;
	BiomeDefinitions.Add(EBiomeType::Plains, PlainsBiome);

	// Cliffs: Height 3000 to 5000cm
	FBiomeDefinition CliffsBiome;
	CliffsBiome.BiomeType = EBiomeType::Cliffs;
	CliffsBiome.MinHeight = 3000.0f;
	CliffsBiome.MaxHeight = 5000.0f;
	CliffsBiome.BiomeName = TEXT("Cliffs");
	CliffsBiome.DebugColor = FColor::Yellow;
	BiomeDefinitions.Add(EBiomeType::Cliffs, CliffsBiome);

	// Mountains: Height 5000 to 10000cm
	FBiomeDefinition MountainBiome;
	MountainBiome.BiomeType = EBiomeType::Mountains;
	MountainBiome.MinHeight = 5000.0f;
	MountainBiome.MaxHeight = 10000.0f;
	MountainBiome.BiomeName = TEXT("Mountains");
	MountainBiome.DebugColor = FColor::Magenta;
	BiomeDefinitions.Add(EBiomeType::Mountains, MountainBiome);

	// Peaks: Height > 10000cm
	FBiomeDefinition PeakBiome;
	PeakBiome.BiomeType = EBiomeType::Peaks;
	PeakBiome.MinHeight = 10000.0f;
	PeakBiome.MaxHeight = FLT_MAX;
	PeakBiome.BiomeName = TEXT("Peaks");
	PeakBiome.DebugColor = FColor::White;
	BiomeDefinitions.Add(EBiomeType::Peaks, PeakBiome);
}

EBiomeType FBiomeClassifier::GetBiomeAtHeight(float InHeight) const
{
	for (const auto& Pair : BiomeDefinitions)
	{
		const FBiomeDefinition& Def = Pair.Value;
		if (InHeight >= Def.MinHeight && InHeight <= Def.MaxHeight)
		{
			return Def.BiomeType;
		}
	}

	return EBiomeType::None;
}

FBiomeDefinition FBiomeClassifier::GetBiomeDefinition(EBiomeType InBiome) const
{
	if (const FBiomeDefinition* Def = BiomeDefinitions.Find(InBiome))
	{
		return *Def;
	}

	// Return an empty definition if not found
	FBiomeDefinition EmptyDef;
	EmptyDef.BiomeType = EBiomeType::None;
	return EmptyDef;
}
