// MapGeneratorFactory.cpp
// Implementation of the map generator factory

#include "MapGeneratorFactory.h"
#include "ContinentMapGenerator.h"
#include "ArchipelagoMapGenerator.h"

UMapGeneratorBase* FMapGeneratorFactory::CreateGenerator(UObject* Outer, EMapType MapType)
{
	switch (MapType)
	{
	case EMapType::Continent:
		return NewObject<UContinentMapGenerator>(Outer);
		
	case EMapType::Archipelago:
		return NewObject<UArchipelagoMapGenerator>(Outer);
		
	default:
		UE_LOG(LogTemp, Error, TEXT("FMapGeneratorFactory::CreateGenerator - Unknown map type"));
		return nullptr;
	}
}
