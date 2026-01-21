// ProceduralMapActor.cpp
// Implementation of the procedural map actor

#include "ProceduralMapActor.h"
#include "MapGeneratorBase.h"
#include "MapGeneratorFactory.h"
#include "ContinentMapGenerator.h"
#include "Engine/Texture2D.h"

AProceduralMapActor::AProceduralMapActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create a default scene root
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AProceduralMapActor::BeginPlay()
{
	Super::BeginPlay();
}

void AProceduralMapActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AProceduralMapActor::GenerateMap()
{
	// Clear any existing map
	ClearMap();

	// Create the appropriate generator based on map type
	CurrentGenerator = FMapGeneratorFactory::CreateGenerator(this, MapType);
	
	if (!CurrentGenerator)
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateMap - Failed to create generator"));
		return;
	}

	// If this is a continent generator, pass the biome settings and seed
	if (MapType == EMapType::Continent)
	{
		UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
		if (ContinentGenerator)
		{
			ContinentGenerator->SetBiomeSettings(ContinentBiomeSettings);
			// Pass the seed - 0 means "generate random", non-zero means use that exact seed
			ContinentGenerator->SetSeed(Seed);
		}
	}

	// Initialize and generate
	FMapGenerationSettings Settings = CreateSettings();
	CurrentGenerator->Initialize(Settings);
	
	if (CurrentGenerator->Generate())
	{
		UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::GenerateMap - Map generation successful"));

		// Retrieve the preview texture from the generator
		if (MapType == EMapType::Continent)
		{
			UContinentMapGenerator* ContinentGenerator = Cast<UContinentMapGenerator>(CurrentGenerator);
			if (ContinentGenerator)
			{
				PreviewTexture = ContinentGenerator->GetPreviewTexture();
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ProceduralMapActor::GenerateMap - Map generation failed"));
	}
}

void AProceduralMapActor::ClearMap()
{
	if (CurrentGenerator)
	{
		CurrentGenerator = nullptr;
	}
	
	PreviewTexture = nullptr;
	
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::ClearMap - Map cleared"));
}

UTexture2D* AProceduralMapActor::GetPreviewTexture() const
{
	return PreviewTexture;
}

void AProceduralMapActor::NormalizeBiomePercentages()
{
	ContinentBiomeSettings.NormalizePercentages();
	UE_LOG(LogTemp, Log, TEXT("ProceduralMapActor::NormalizeBiomePercentages - Percentages normalized to 100%%"));
}

FMapGenerationSettings AProceduralMapActor::CreateSettings() const
{
	FMapGenerationSettings Settings;
	Settings.MapType = MapType;
	Settings.MapSizeInMeters = MapSizeInMeters;
	Settings.MapResolution = MapResolution;
	return Settings;
}
