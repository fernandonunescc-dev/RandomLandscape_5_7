// MapGeneratorFactory.h
// Factory for creating map generators based on map type

#pragma once

#include "CoreMinimal.h"
#include "MapTypes.h"
#include "MapGeneratorBase.h"

/**
 * Factory class for creating the appropriate map generator based on map type.
 */
class RANDOMLANDSCAPE_5_7_API FMapGeneratorFactory
{
public:
	/**
	 * Creates a map generator for the specified map type
	 * @param Outer The outer object for the created generator
	 * @param MapType The type of map to create a generator for
	 * @return The created map generator, or nullptr if invalid type
	 */
	static UMapGeneratorBase* CreateGenerator(UObject* Outer, EMapType MapType);
};
