// BiomeTerrainGeneratorFactory.cpp
// Factory implementation for biome terrain generators

#include "BiomeTerrainGeneratorFactory.h"
#include "BiomeTerrainGeneratorBase.h"
#include "OceanTerrainGenerator.h"
#include "ForestTerrainGenerator.h"
#include "MountainTerrainGenerator.h"
#include "DesertTerrainGenerator.h"
#include "SnowTerrainGenerator.h"
#include "VolcanicTerrainGenerator.h"

FBiomeTerrainGeneratorFactory& FBiomeTerrainGeneratorFactory::Get()
{
	static FBiomeTerrainGeneratorFactory Instance;
	return Instance;
}

FBiomeTerrainGeneratorFactory::FBiomeTerrainGeneratorFactory()
{
	// Register all biome terrain generators
	Generators.Add(EBiomeType::Ocean, MakeUnique<FOceanTerrainGenerator>());
	Generators.Add(EBiomeType::Forest, MakeUnique<FForestTerrainGenerator>());
	Generators.Add(EBiomeType::Mountain, MakeUnique<FMountainTerrainGenerator>());
	Generators.Add(EBiomeType::Desert, MakeUnique<FDesertTerrainGenerator>());
	Generators.Add(EBiomeType::Snow, MakeUnique<FSnowTerrainGenerator>());
	Generators.Add(EBiomeType::Volcanic, MakeUnique<FVolcanicTerrainGenerator>());
}

FBiomeTerrainGeneratorFactory::~FBiomeTerrainGeneratorFactory()
{
	Generators.Empty();
}

const FBiomeTerrainGeneratorBase* FBiomeTerrainGeneratorFactory::GetGenerator(EBiomeType BiomeType) const
{
	const TUniquePtr<FBiomeTerrainGeneratorBase>* FoundGenerator = Generators.Find(BiomeType);
	if (FoundGenerator && FoundGenerator->IsValid())
	{
		return FoundGenerator->Get();
	}
	return nullptr;
}

float FBiomeTerrainGeneratorFactory::CalculateHeightForBiome(EBiomeType BiomeType, float NormX, float NormY,
	const FBiomeMeshSettings& MeshSettings, int32 Seed, float MapSizeInMeters) const
{
	const FBiomeTerrainGeneratorBase* Generator = GetGenerator(BiomeType);
	if (Generator)
	{
		return Generator->CalculateHeight(NormX, NormY, MeshSettings, Seed, MapSizeInMeters);
	}
	
	// Fallback: return 0 (midpoint of -1 to 1 noise range)
	return 0.0f;
}
