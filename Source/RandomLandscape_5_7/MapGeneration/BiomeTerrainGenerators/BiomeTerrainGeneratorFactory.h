// BiomeTerrainGeneratorFactory.h
// Factory for creating and managing biome terrain generators

#pragma once

#include "CoreMinimal.h"
#include "../BiomeTypes.h"

class FBiomeTerrainGeneratorBase;

/**
 * Factory class for creating and retrieving biome terrain generators.
 * Uses singleton pattern to manage generator instances.
 */
class RANDOMLANDSCAPE_5_7_API FBiomeTerrainGeneratorFactory
{
public:
	/**
	 * Get the singleton instance.
	 */
	static FBiomeTerrainGeneratorFactory& Get();

	/**
	 * Get the terrain generator for a specific biome type.
	 * @param BiomeType - The type of biome
	 * @return Pointer to the generator, or nullptr if not found
	 */
	const FBiomeTerrainGeneratorBase* GetGenerator(EBiomeType BiomeType) const;

private:
	FBiomeTerrainGeneratorFactory();
	~FBiomeTerrainGeneratorFactory();

	// Prevent copying
	FBiomeTerrainGeneratorFactory(const FBiomeTerrainGeneratorFactory&) = delete;
	FBiomeTerrainGeneratorFactory& operator=(const FBiomeTerrainGeneratorFactory&) = delete;

	/** Map of biome type to generator instance */
	TMap<EBiomeType, TUniquePtr<FBiomeTerrainGeneratorBase>> Generators;
};
