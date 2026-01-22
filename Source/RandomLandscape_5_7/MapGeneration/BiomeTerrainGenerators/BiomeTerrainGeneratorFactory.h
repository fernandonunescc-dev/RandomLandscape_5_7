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

	/**
	 * Calculate terrain height for a biome at a given position.
	 * Convenience method that looks up the generator and calls CalculateHeight.
	 * @param BiomeType - The type of biome
	 * @param NormX - Normalized X position (0-1)
	 * @param NormY - Normalized Y position (0-1)
	 * @param BiomeConfig - The biome configuration
	 * @param Seed - Random seed
	 * @param MapSizeInMeters - The total map size in meters (used for frequency scaling)
	 * @return Height in Unreal Units, or 0 if generator not found
	 */
	float CalculateHeightForBiome(EBiomeType BiomeType, float NormX, float NormY, 
		const FBiomeConfig& BiomeConfig, int32 Seed, float MapSizeInMeters) const;

private:
	FBiomeTerrainGeneratorFactory();
	~FBiomeTerrainGeneratorFactory();

	// Prevent copying
	FBiomeTerrainGeneratorFactory(const FBiomeTerrainGeneratorFactory&) = delete;
	FBiomeTerrainGeneratorFactory& operator=(const FBiomeTerrainGeneratorFactory&) = delete;

	/** Map of biome type to generator instance */
	TMap<EBiomeType, TUniquePtr<FBiomeTerrainGeneratorBase>> Generators;
};
