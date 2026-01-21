// MapGeneratorBase.cpp
// Implementation of the base map generator

#include "MapGeneratorBase.h"

UMapGeneratorBase::UMapGeneratorBase()
{
}

void UMapGeneratorBase::Initialize(const FMapGenerationSettings& InSettings)
{
	Settings = InSettings;
	bIsInitialized = true;
}

bool UMapGeneratorBase::Generate()
{
	// Base implementation does nothing - subclasses override this
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("MapGeneratorBase::Generate called before Initialize"));
		return false;
	}
	return true;
}
