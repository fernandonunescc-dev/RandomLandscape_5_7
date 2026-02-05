// BiomeDataGenerationActorCustomization.cpp
// IDetailCustomization implementation for ABiomeDataGenerationActor
// Version: 02.04.2026.23.55

#include "BiomeDataGenerationActorCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Engine/Texture2D.h"
#include "MapGeneration/BiomeDataGenerationActor.h"

// ==================== Factory Method ====================

TSharedRef<IDetailCustomization> FBiomeDataGenerationActorCustomization::MakeInstance()
{
	return MakeShareable(new FBiomeDataGenerationActorCustomization());
}

// ==================== Main Customization Method ====================

void FBiomeDataGenerationActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// -------------------- Gather Selected Objects --------------------
	// Cache weak pointers to all selected actors for button callbacks
	
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	
	SelectedActors.Empty();
	for (const TWeakObjectPtr<UObject>& Obj : SelectedObjects)
	{
		if (ABiomeDataGenerationActor* Actor = Cast<ABiomeDataGenerationActor>(Obj.Get()))
		{
			SelectedActors.Add(Actor);
		}
	}
	
	// If no valid actors selected, bail out and let default layout show
	if (SelectedActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("BiomeDataGenerationActorCustomization: No valid actors selected"));
		return;
	}
	
	// -------------------- Get Property Handles --------------------
	// Validate that we can get the property handles before hiding default categories
	
	TSharedRef<IPropertyHandle> LandmassSettingsHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ABiomeDataGenerationActor, LandmassSettings)
	);
	
	TSharedRef<IPropertyHandle> BiomeSettingsHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ABiomeDataGenerationActor, BiomeLayoutSettings)
	);
	
	TSharedRef<IPropertyHandle> LandmassPreviewHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ABiomeDataGenerationActor, LandmassPreviewTexture)
	);
	
	TSharedRef<IPropertyHandle> BiomePreviewHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ABiomeDataGenerationActor, BiomePreviewTexture)
	);
	
	TSharedRef<IPropertyHandle> LandmassSeedHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ABiomeDataGenerationActor, ActualLandmassSeedUsed)
	);
	
	TSharedRef<IPropertyHandle> BiomeSeedHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ABiomeDataGenerationActor, ActualBiomeSeedUsed)
	);
	
	TSharedRef<IPropertyHandle> ResolutionHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(ABiomeDataGenerationActor, TextureResolutionUsed)
	);
	
	// -------------------- Safety Check --------------------
	// If critical property handles are invalid, log error and use fallback layout
	
	const bool bLandmassValid = LandmassSettingsHandle->IsValidHandle();
	const bool bBiomeValid = BiomeSettingsHandle->IsValidHandle();
	
	if (!bLandmassValid || !bBiomeValid)
	{
		UE_LOG(LogTemp, Error, TEXT("BiomeDataGenerationActorCustomization: Property handles invalid! LandmassSettings=%d, BiomeLayoutSettings=%d"),
			bLandmassValid ? 1 : 0, bBiomeValid ? 1 : 0);
		
		// FALLBACK: Don't hide anything, just add buttons to a Generation category
		IDetailCategoryBuilder& FallbackCategory = DetailBuilder.EditCategory(
			"Generation",
			FText::FromString("Generation"),
			ECategoryPriority::Important
		);
		
		// Add buttons directly without groups
		AddButtonRow(FallbackCategory, "Generate Landmass", 
			[](ABiomeDataGenerationActor* Actor) { Actor->GenerateLandmass(); });
		AddButtonRow(FallbackCategory, "Generate Biomes",
			[](ABiomeDataGenerationActor* Actor) { Actor->GenerateBiomes(); });
		AddButtonRow(FallbackCategory, "Generate All",
			[](ABiomeDataGenerationActor* Actor) { Actor->GenerateAll(); });
		AddButtonRow(FallbackCategory, "Clear Generated Data",
			[](ABiomeDataGenerationActor* Actor) { Actor->ClearGeneratedData(); });
		
		return;
	}
	
	// -------------------- Hide Default Categories --------------------
	// Only hide after validating we can provide replacement UI
	
	DetailBuilder.HideCategory("Generation|Landmass");
	DetailBuilder.HideCategory("Generation|Biomes");
	DetailBuilder.HideCategory("Generation|Actions");
	
	// -------------------- Create Main "Generation" Category --------------------
	
	IDetailCategoryBuilder& GenerationCategory = DetailBuilder.EditCategory(
		"Generation",
		FText::FromString("Generation"),
		ECategoryPriority::Important
	);
	
	// ==================== LANDMASS GROUP ====================
	
	IDetailGroup& LandmassGroup = GenerationCategory.AddGroup(
		"LandmassGroup",
		FText::FromString("Landmass"),
		true,  // bStartExpanded
		true   // bShowHeader
	);
	
	// Add LandmassSettings property
	LandmassGroup.AddPropertyRow(LandmassSettingsHandle)
		.DisplayName(FText::FromString("Settings"));
	
	// Add Generate Landmass button
	LandmassGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Action"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Generate Landmass"),
				[](ABiomeDataGenerationActor* Actor) { Actor->GenerateLandmass(); },
				SelectedActors
			)
		];
	
	// Add output properties
	if (LandmassPreviewHandle->IsValidHandle())
	{
		LandmassGroup.AddPropertyRow(LandmassPreviewHandle)
			.DisplayName(FText::FromString("Preview Texture"));
	}
	
	if (LandmassSeedHandle->IsValidHandle())
	{
		LandmassGroup.AddPropertyRow(LandmassSeedHandle)
			.DisplayName(FText::FromString("Actual Seed Used"));
	}
	
	if (ResolutionHandle->IsValidHandle())
	{
		LandmassGroup.AddPropertyRow(ResolutionHandle)
			.DisplayName(FText::FromString("Resolution Used"));
	}
	
	// ==================== BIOMES GROUP ====================
	
	IDetailGroup& BiomesGroup = GenerationCategory.AddGroup(
		"BiomesGroup",
		FText::FromString("Biomes"),
		true,  // bStartExpanded
		true   // bShowHeader
	);
	
	// Add BiomeLayoutSettings property
	BiomesGroup.AddPropertyRow(BiomeSettingsHandle)
		.DisplayName(FText::FromString("Settings"));
	
	// Add Generate Biomes button
	BiomesGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Action"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Generate Biomes"),
				[](ABiomeDataGenerationActor* Actor) { Actor->GenerateBiomes(); },
				SelectedActors
			)
		];
	
	// Add output properties
	if (BiomePreviewHandle->IsValidHandle())
	{
		BiomesGroup.AddPropertyRow(BiomePreviewHandle)
			.DisplayName(FText::FromString("Preview Texture"));
	}
	
	if (BiomeSeedHandle->IsValidHandle())
	{
		BiomesGroup.AddPropertyRow(BiomeSeedHandle)
			.DisplayName(FText::FromString("Actual Seed Used"));
	}
	
	// ==================== BATCH ACTIONS GROUP ====================
	
	IDetailGroup& ActionsGroup = GenerationCategory.AddGroup(
		"ActionsGroup",
		FText::FromString("Batch Actions"),
		true,  // bStartExpanded
		true   // bShowHeader
	);
	
	// Add Generate All button
	ActionsGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Generate All"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Generate Landmass + Biomes"),
				[](ABiomeDataGenerationActor* Actor) { Actor->GenerateAll(); },
				SelectedActors
			)
		];
	
	// Add Clear button
	ActionsGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Clear"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Clear Generated Data"),
				[](ABiomeDataGenerationActor* Actor) { Actor->ClearGeneratedData(); },
				SelectedActors
			)
		];
}

// ==================== Helper: Create Button Widget ====================

TSharedRef<SWidget> FBiomeDataGenerationActorCustomization::MakeButtonWidget(
	const FText& ButtonText,
	TFunction<void(ABiomeDataGenerationActor*)> OnClickedLambda,
	const TArray<TWeakObjectPtr<ABiomeDataGenerationActor>>& WeakActors)
{
	return SNew(SButton)
		.Text(ButtonText)
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([WeakActors, OnClickedLambda]() -> FReply
		{
			for (const TWeakObjectPtr<ABiomeDataGenerationActor>& WeakActor : WeakActors)
			{
				if (ABiomeDataGenerationActor* Actor = WeakActor.Get())
				{
					OnClickedLambda(Actor);
				}
			}
			return FReply::Handled();
		});
}

// ==================== Helper: Add Button Row to Category ====================

void FBiomeDataGenerationActorCustomization::AddButtonRow(
	IDetailCategoryBuilder& Category,
	const FString& ButtonLabel,
	TFunction<void(ABiomeDataGenerationActor*)> OnClickedLambda)
{
	Category.AddCustomRow(FText::FromString(ButtonLabel))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(ButtonLabel))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(FText::FromString(ButtonLabel), OnClickedLambda, SelectedActors)
		];
}

// ==================== Helper: Create Texture Preview Widget ====================

TSharedRef<SWidget> FBiomeDataGenerationActorCustomization::MakeTexturePreviewWidget(
	TObjectPtr<UTexture2D>* TexturePtr,
	const FText& Label)
{
	return SNew(SBox)
		.Padding(FMargin(4.0f))
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

// ==================== Helper: Get First Selected Actor ====================

ABiomeDataGenerationActor* FBiomeDataGenerationActorCustomization::GetFirstSelectedActor() const
{
	for (const TWeakObjectPtr<ABiomeDataGenerationActor>& WeakActor : SelectedActors)
	{
		if (ABiomeDataGenerationActor* Actor = WeakActor.Get())
		{
			return Actor;
		}
	}
	return nullptr;
}
