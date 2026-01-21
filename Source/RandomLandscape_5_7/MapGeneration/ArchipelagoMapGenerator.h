// ArchipelagoMapGenerator.h
// Generator for Archipelago-type maps

#pragma once

#include "CoreMinimal.h"
#include "MapGeneratorBase.h"
#include "ArchipelagoMapGenerator.generated.h"

/**
 * Map generator for Archipelago-type maps.
 * Creates multiple smaller islands scattered across water.
 */
UCLASS(Blueprintable)
class RANDOMLANDSCAPE_5_7_API UArchipelagoMapGenerator : public UMapGeneratorBase
{
	GENERATED_BODY()

public:
	UArchipelagoMapGenerator();
	virtual ~UArchipelagoMapGenerator() = default;

	//~ Begin UMapGeneratorBase Interface
	virtual void Initialize(const FMapGenerationSettings& InSettings) override;
	virtual bool Generate() override;
	//~ End UMapGeneratorBase Interface

protected:
	// Future: Add archipelago-specific generation parameters here
	// e.g., island count range, island size variation, spacing, etc.
};
