// MapGeneratorBase.h
// Base class for all map generation strategies

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MapTypes.h"
#include "MapGeneratorBase.generated.h"

/**
 * Abstract base class for map generation strategies.
 * Subclasses implement specific generation algorithms for different map types.
 */
UCLASS(Abstract, Blueprintable)
class RANDOMLANDSCAPE_5_7_API UMapGeneratorBase : public UObject
{
	GENERATED_BODY()

public:
	UMapGeneratorBase();
	virtual ~UMapGeneratorBase() = default;

	/**
	 * Initialize the generator with settings
	 * @param InSettings The map generation settings to use
	 */
	virtual void Initialize(const FMapGenerationSettings& InSettings);

	/**
	 * Generate the map data. Override in subclasses.
	 * @return True if generation was successful
	 */
	UFUNCTION(BlueprintCallable, Category = "Map Generation")
	virtual bool Generate();

	/**
	 * Get the current generation settings
	 */
	UFUNCTION(BlueprintPure, Category = "Map Generation")
	const FMapGenerationSettings& GetSettings() const { return Settings; }

protected:
	/** Current generation settings */
	UPROPERTY()
	FMapGenerationSettings Settings;

	/** Whether the generator has been initialized */
	bool bIsInitialized = false;
};
