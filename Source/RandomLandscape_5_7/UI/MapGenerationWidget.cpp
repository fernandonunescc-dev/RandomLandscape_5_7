// MapGenerationWidget.cpp
// In-game UI for configuring and previewing procedural world generation

#include "MapGenerationWidget.h"
#include "WorldGenerationActor.h"

#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Border.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

// ----------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------
void UMapGenerationWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Solid background
	if (BackgroundBorder)
	{
		BackgroundBorder->SetBrushColor(BackgroundColor);
	}

	// Wire button delegates
	if (GenerateButton)
	{
		GenerateButton->OnClicked.AddDynamic(this, &UMapGenerationWidget::OnGenerateClicked);
	}
	if (RandomizeButton)
	{
		RandomizeButton->OnClicked.AddDynamic(this, &UMapGenerationWidget::OnRandomizeClicked);
	}
	if (RandomizeSeedButton)
	{
		RandomizeSeedButton->OnClicked.AddDynamic(this, &UMapGenerationWidget::OnRandomizeSeedClicked);
	}

	// Wire slider delegates
	if (TerrainRoughnessSlider)
	{
		TerrainRoughnessSlider->OnValueChanged.AddDynamic(this, &UMapGenerationWidget::OnTerrainRoughnessChanged);
	}
	if (LandAreaSlider)
	{
		LandAreaSlider->OnValueChanged.AddDynamic(this, &UMapGenerationWidget::OnLandAreaChanged);
	}
	if (SnowPercentSlider)
	{
		SnowPercentSlider->OnValueChanged.AddDynamic(this, &UMapGenerationWidget::OnSnowPercentChanged);
	}
	if (DesertPercentSlider)
	{
		DesertPercentSlider->OnValueChanged.AddDynamic(this, &UMapGenerationWidget::OnDesertPercentChanged);
	}
	if (ForestPercentSlider)
	{
		ForestPercentSlider->OnValueChanged.AddDynamic(this, &UMapGenerationWidget::OnForestPercentChanged);
	}

	// Wire seed input
	if (SeedInput)
	{
		SeedInput->OnTextCommitted.AddDynamic(this, &UMapGenerationWidget::OnSeedCommitted);
	}

	// Wire landmass type combo
	if (LandmassTypeCombo)
	{
		LandmassTypeCombo->ClearOptions();
		LandmassTypeCombo->AddOption(TEXT("Islands / Archipelago"));
		LandmassTypeCombo->AddOption(TEXT("Continent"));
		LandmassTypeCombo->OnSelectionChanged.AddDynamic(this, &UMapGenerationWidget::OnLandmassTypeChanged);
	}

	// Wire checkbox delegates
	if (VolcanoCheckBox)
	{
		VolcanoCheckBox->OnCheckStateChanged.AddDynamic(this, &UMapGenerationWidget::OnVolcanoToggled);
	}
	if (MountainsCheckBox)
	{
		MountainsCheckBox->OnCheckStateChanged.AddDynamic(this, &UMapGenerationWidget::OnMountainsToggled);
	}
	if (HillsCheckBox)
	{
		HillsCheckBox->OnCheckStateChanged.AddDynamic(this, &UMapGenerationWidget::OnHillsToggled);
	}
	if (PlateausCheckBox)
	{
		PlateausCheckBox->OnCheckStateChanged.AddDynamic(this, &UMapGenerationWidget::OnPlateausToggled);
	}
	if (RiversCheckBox)
	{
		RiversCheckBox->OnCheckStateChanged.AddDynamic(this, &UMapGenerationWidget::OnRiversToggled);
	}
	if (CanyonsCheckBox)
	{
		CanyonsCheckBox->OnCheckStateChanged.AddDynamic(this, &UMapGenerationWidget::OnCanyonsToggled);
	}

	// Auto-initialize if actor is already in the world
	InitializeGenerator();
}

// ----------------------------------------------------------------
// Public API
// ----------------------------------------------------------------
void UMapGenerationWidget::InitializeGenerator()
{
	if (GeneratorActor)
	{
		SyncActorToUI();
		return;
	}

	// Try to find an existing WorldGenerationActor
	UWorld* World = GetWorld();
	if (!World)
	{
		SetStatus(TEXT("No world context"));
		return;
	}

	AActor* Found = UGameplayStatics::GetActorOfClass(World, AWorldGenerationActor::StaticClass());
	if (Found)
	{
		GeneratorActor = Cast<AWorldGenerationActor>(Found);
	}
	else
	{
		// Spawn one
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GeneratorActor = World->SpawnActor<AWorldGenerationActor>(AWorldGenerationActor::StaticClass(), FTransform::Identity, SpawnParams);
	}

	if (!GeneratorActor)
	{
		SetStatus(TEXT("Failed to create generator"));
		return;
	}

	// Use lower resolution for preview speed
	GeneratorActor->TextureResolution = PreviewResolution;

	SyncActorToUI();
	SetStatus(TEXT("Ready — press Generate or Randomize"));
}

void UMapGenerationWidget::Generate()
{
	if (!GeneratorActor)
	{
		SetStatus(TEXT("No generator — call InitializeGenerator first"));
		return;
	}

	SetStatus(TEXT("Generating..."));

	SyncUIToActor();
	GeneratorActor->GenerateAll();
	GeneratorActor->GenerateMesh();

	SyncActorToUI();
	UpdatePreviewImage();

	SetStatus(FString::Printf(TEXT("Done — Seed %d, Map %.0f m"),
		GeneratorActor->ActualSeedUsed, GeneratorActor->FinalMapSizeMeters));
}

void UMapGenerationWidget::RandomizeAll()
{
	if (!GeneratorActor)
	{
		SetStatus(TEXT("No generator — call InitializeGenerator first"));
		return;
	}

	SetStatus(TEXT("Randomizing..."));

	// Randomize changes seed + biome targets + land area, then generates
	GeneratorActor->TextureResolution = PreviewResolution;
	GeneratorActor->Randomize();

	SyncActorToUI();
	UpdatePreviewImage();

	SetStatus(FString::Printf(TEXT("Randomized — Seed %d"), GeneratorActor->ActualSeedUsed));
}

void UMapGenerationWidget::RandomizeSeedOnly()
{
	if (!GeneratorActor)
	{
		SetStatus(TEXT("No generator — call InitializeGenerator first"));
		return;
	}

	SetStatus(TEXT("Randomizing seed..."));

	SyncUIToActor();
	GeneratorActor->RandomizeSeed();

	SyncActorToUI();
	UpdatePreviewImage();

	SetStatus(FString::Printf(TEXT("New seed %d"), GeneratorActor->ActualSeedUsed));
}

// ----------------------------------------------------------------
// Sync UI ↔ Actor
// ----------------------------------------------------------------
void UMapGenerationWidget::SyncUIToActor()
{
	if (!GeneratorActor) return;

	GeneratorActor->TextureResolution = PreviewResolution;

	// Seed
	if (SeedInput)
	{
		GeneratorActor->GlobalSeed = FCString::Atoi(*SeedInput->GetText().ToString());
	}

	// Landmass type
	if (LandmassTypeCombo)
	{
		const int32 Index = LandmassTypeCombo->GetSelectedIndex();
		GeneratorActor->LandmassType = (Index == 1) ? ELandmassType::Continent : ELandmassType::Islands;
	}

	// Terrain roughness: slider 0-1 maps directly
	if (TerrainRoughnessSlider)
	{
		GeneratorActor->TerrainRoughness = TerrainRoughnessSlider->GetValue();
	}

	// Land area: slider 0-1 → 0.01 - 16.0 sq km (logarithmic feel via power curve)
	if (LandAreaSlider)
	{
		const float T = LandAreaSlider->GetValue();
		GeneratorActor->TargetLandAreaSqKm = FMath::Lerp(0.01f, 16.0f, T * T);
	}

	// Biome percentages: slider 0-1 → their respective ranges
	if (SnowPercentSlider)
	{
		GeneratorActor->TargetSnowPercent = SnowPercentSlider->GetValue() * 30.0f;
	}
	if (DesertPercentSlider)
	{
		GeneratorActor->TargetDesertPercent = DesertPercentSlider->GetValue() * 40.0f;
	}
	if (ForestPercentSlider)
	{
		GeneratorActor->TargetForestPercent = ForestPercentSlider->GetValue() * 60.0f;
	}

	// Feature toggles
	if (VolcanoCheckBox)    GeneratorActor->bIncludeVolcano    = VolcanoCheckBox->IsChecked();
	if (MountainsCheckBox)  GeneratorActor->bIncludeMountains  = MountainsCheckBox->IsChecked();
	if (HillsCheckBox)      GeneratorActor->bIncludeHills      = HillsCheckBox->IsChecked();
	if (PlateausCheckBox)   GeneratorActor->bIncludePlateaus   = PlateausCheckBox->IsChecked();
	if (RiversCheckBox)     GeneratorActor->bIncludeRivers     = RiversCheckBox->IsChecked();
	if (CanyonsCheckBox)    GeneratorActor->bIncludeCanyons    = CanyonsCheckBox->IsChecked();
}

void UMapGenerationWidget::SyncActorToUI()
{
	if (!GeneratorActor) return;

	// Seed
	if (SeedInput)
	{
		SeedInput->SetText(FText::AsNumber(GeneratorActor->GlobalSeed));
	}

	// Landmass type
	if (LandmassTypeCombo)
	{
		LandmassTypeCombo->SetSelectedIndex(GeneratorActor->LandmassType == ELandmassType::Continent ? 1 : 0);
	}

	// Terrain roughness
	if (TerrainRoughnessSlider)
	{
		TerrainRoughnessSlider->SetValue(GeneratorActor->TerrainRoughness);
	}
	if (TerrainRoughnessLabel)
	{
		TerrainRoughnessLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Roughness: %.0f%%"), GeneratorActor->TerrainRoughness * 100.0f)));
	}

	// Land area — inverse of the power curve used in SyncUIToActor
	if (LandAreaSlider)
	{
		const float Normalized = FMath::Sqrt(FMath::GetRangePct(0.01f, 16.0f, GeneratorActor->TargetLandAreaSqKm));
		LandAreaSlider->SetValue(FMath::Clamp(Normalized, 0.0f, 1.0f));
	}
	if (LandAreaLabel)
	{
		LandAreaLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Land Area: %.2f km²"), GeneratorActor->TargetLandAreaSqKm)));
	}

	// Snow
	if (SnowPercentSlider)
	{
		SnowPercentSlider->SetValue(GeneratorActor->TargetSnowPercent / 30.0f);
	}
	if (SnowPercentLabel)
	{
		SnowPercentLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Snow: %.0f%%"), GeneratorActor->TargetSnowPercent)));
	}

	// Desert
	if (DesertPercentSlider)
	{
		DesertPercentSlider->SetValue(GeneratorActor->TargetDesertPercent / 40.0f);
	}
	if (DesertPercentLabel)
	{
		DesertPercentLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Desert: %.0f%%"), GeneratorActor->TargetDesertPercent)));
	}

	// Forest
	if (ForestPercentSlider)
	{
		ForestPercentSlider->SetValue(GeneratorActor->TargetForestPercent / 60.0f);
	}
	if (ForestPercentLabel)
	{
		ForestPercentLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Forest: %.0f%%"), GeneratorActor->TargetForestPercent)));
	}

	// Feature toggles
	if (VolcanoCheckBox)    VolcanoCheckBox->SetIsChecked(GeneratorActor->bIncludeVolcano);
	if (MountainsCheckBox)  MountainsCheckBox->SetIsChecked(GeneratorActor->bIncludeMountains);
	if (HillsCheckBox)      HillsCheckBox->SetIsChecked(GeneratorActor->bIncludeHills);
	if (PlateausCheckBox)   PlateausCheckBox->SetIsChecked(GeneratorActor->bIncludePlateaus);
	if (RiversCheckBox)     RiversCheckBox->SetIsChecked(GeneratorActor->bIncludeRivers);
	if (CanyonsCheckBox)    CanyonsCheckBox->SetIsChecked(GeneratorActor->bIncludeCanyons);
}

// ----------------------------------------------------------------
// Preview
// ----------------------------------------------------------------
void UMapGenerationWidget::UpdatePreviewImage()
{
	if (!PreviewImage || !GeneratorActor) return;

	UTexture2D* BiomeTexture = GeneratorActor->Debug_BiomeMap;
	if (!BiomeTexture)
	{
		// Fall back to landmass preview if biomes haven't been generated yet
		BiomeTexture = GeneratorActor->Debug_Landmass;
	}

	if (BiomeTexture)
	{
		PreviewImage->SetBrushFromTexture(BiomeTexture, true);
	}
}

// ----------------------------------------------------------------
// Status
// ----------------------------------------------------------------
void UMapGenerationWidget::SetStatus(const FString& Message)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

// ----------------------------------------------------------------
// Button Delegates
// ----------------------------------------------------------------
void UMapGenerationWidget::OnGenerateClicked()
{
	Generate();
}

void UMapGenerationWidget::OnRandomizeClicked()
{
	RandomizeAll();
}

void UMapGenerationWidget::OnRandomizeSeedClicked()
{
	RandomizeSeedOnly();
}

// ----------------------------------------------------------------
// Slider Delegates
// ----------------------------------------------------------------
void UMapGenerationWidget::OnTerrainRoughnessChanged(float Value)
{
	if (TerrainRoughnessLabel)
	{
		TerrainRoughnessLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Roughness: %.0f%%"), Value * 100.0f)));
	}
}

void UMapGenerationWidget::OnLandAreaChanged(float Value)
{
	if (LandAreaLabel)
	{
		const float Area = FMath::Lerp(0.01f, 16.0f, Value * Value);
		LandAreaLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Land Area: %.2f km²"), Area)));
	}
}

void UMapGenerationWidget::OnSnowPercentChanged(float Value)
{
	if (SnowPercentLabel)
	{
		SnowPercentLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Snow: %.0f%%"), Value * 30.0f)));
	}
}

void UMapGenerationWidget::OnDesertPercentChanged(float Value)
{
	if (DesertPercentLabel)
	{
		DesertPercentLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Desert: %.0f%%"), Value * 40.0f)));
	}
}

void UMapGenerationWidget::OnForestPercentChanged(float Value)
{
	if (ForestPercentLabel)
	{
		ForestPercentLabel->SetText(FText::FromString(
			FString::Printf(TEXT("Forest: %.0f%%"), Value * 60.0f)));
	}
}

// ----------------------------------------------------------------
// Text / Combo Delegates
// ----------------------------------------------------------------
void UMapGenerationWidget::OnSeedCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Value is pushed on next Generate
}

void UMapGenerationWidget::OnLandmassTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// Value is pushed on next Generate
}

// ----------------------------------------------------------------
// Checkbox Delegates
// ----------------------------------------------------------------
void UMapGenerationWidget::OnVolcanoToggled(bool bIsChecked)
{
	// Value is pushed on next Generate
}

void UMapGenerationWidget::OnMountainsToggled(bool bIsChecked)
{
	// Value is pushed on next Generate
}

void UMapGenerationWidget::OnHillsToggled(bool bIsChecked)
{
	// Value is pushed on next Generate
}

void UMapGenerationWidget::OnPlateausToggled(bool bIsChecked)
{
	// Value is pushed on next Generate
}

void UMapGenerationWidget::OnRiversToggled(bool bIsChecked)
{
	// Value is pushed on next Generate
}

void UMapGenerationWidget::OnCanyonsToggled(bool bIsChecked)
{
	// Value is pushed on next Generate
}
