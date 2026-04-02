// WorldGenerationActorCustomization.cpp
// IDetailCustomization implementation for AWorldGenerationActor

#include "WorldGenerationActorCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "MapGeneration/WorldGenerationActor.h"

// ==================== Factory Method ====================

TSharedRef<IDetailCustomization> FWorldGenerationActorCustomization::MakeInstance()
{
	return MakeShareable(new FWorldGenerationActorCustomization());
}

// ==================== Main Customization Method ====================

void FWorldGenerationActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// -------------------- Gather Selected Objects --------------------

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

	SelectedActors.Empty();
	for (const TWeakObjectPtr<UObject>& Obj : SelectedObjects)
	{
		if (AWorldGenerationActor* Actor = Cast<AWorldGenerationActor>(Obj.Get()))
		{
			SelectedActors.Add(Actor);
		}
	}

	if (SelectedActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldGenerationActorCustomization: No valid actors selected"));
		return;
	}

	// -------------------- Validate Key Property Handles --------------------

	TSharedRef<IPropertyHandle> LandmassHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, LandmassSettings));
	TSharedRef<IPropertyHandle> UpliftHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, UpliftSettings));

	if (!LandmassHandle->IsValidHandle() || !UpliftHandle->IsValidHandle())
	{
		UE_LOG(LogTemp, Error, TEXT("WorldGenerationActorCustomization: Property handles invalid, using fallback"));

		IDetailCategoryBuilder& FallbackCategory = DetailBuilder.EditCategory(
			"Pipeline", FText::FromString("Pipeline"), ECategoryPriority::Important);

		AddButtonRow(FallbackCategory, "Step 1: Generate Landmass",
			[](AWorldGenerationActor* A) { A->Step1_GenerateLandmass(); });
		AddButtonRow(FallbackCategory, "Step 2: Generate Uplift",
			[](AWorldGenerationActor* A) { A->Step2_GenerateUplift(); });
		AddButtonRow(FallbackCategory, "Step 3: Generate Hydrology",
			[](AWorldGenerationActor* A) { A->Step3_GenerateHydrology(); });
		AddButtonRow(FallbackCategory, "Step 4: Generate Erosion",
			[](AWorldGenerationActor* A) { A->Step4_GenerateErosion(); });
		AddButtonRow(FallbackCategory, "Step 5: Generate Climate",
			[](AWorldGenerationActor* A) { A->Step5_GenerateClimate(); });
		AddButtonRow(FallbackCategory, "Step 6: Generate Biomes",
			[](AWorldGenerationActor* A) { A->Step6_GenerateBiomes(); });
		AddButtonRow(FallbackCategory, "Step 7: Generate Refinement",
			[](AWorldGenerationActor* A) { A->Step7_GenerateRefinement(); });
		AddButtonRow(FallbackCategory, "Step 8: Generate Caves",
			[](AWorldGenerationActor* A) { A->Step8_GenerateCaves(); });
		AddButtonRow(FallbackCategory, "Randomize",
			[](AWorldGenerationActor* A) { A->Randomize(); });
		AddButtonRow(FallbackCategory, "Generate All",
			[](AWorldGenerationActor* A) { A->GenerateAll(); });
		AddButtonRow(FallbackCategory, "Generate Mesh",
			[](AWorldGenerationActor* A) { A->GenerateMesh(); });
		AddButtonRow(FallbackCategory, "Validate Terrain",
			[](AWorldGenerationActor* A) { A->ValidateTerrain(); });
		AddButtonRow(FallbackCategory, "Clear All",
			[](AWorldGenerationActor* A) { A->ClearAll(); });
		return;
	}

	// -------------------- Hide Default Categories --------------------

	DetailBuilder.HideCategory("Pipeline|Actions");

	// -------------------- Pipeline Category --------------------

	IDetailCategoryBuilder& PipelineCategory = DetailBuilder.EditCategory(
		"Pipeline", FText::FromString("Pipeline"), ECategoryPriority::Important);

	// ==================== QUICK PRESETS GROUP ====================

	IDetailGroup& PresetsGroup = PipelineCategory.AddGroup(
		"PresetsGroup", FText::FromString("Quick Presets"), true, true);

	auto AddPresetProp = [&](FName PropName, const FString& DisplayName)
	{
		TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(PropName);
		if (Handle->IsValidHandle())
		{
			PresetsGroup.AddPropertyRow(Handle).DisplayName(FText::FromString(DisplayName));
		}
	};

	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bAutoRegenerate), "Live Preview");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeVolcano), "Include Volcano");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeMountains), "Include Mountains");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeHills), "Include Hills");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludePlateaus), "Include Plateaus");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeRivers), "Include Rivers");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeCanyons), "Include Canyons");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, bIncludeCaves), "Include Caves");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TerrainRoughness), "Terrain Roughness");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TargetSnowPercent), "Snow %");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TargetDesertPercent), "Desert %");
	AddPresetProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TargetForestPercent), "Forest %");

	// ==================== GLOBAL SETTINGS GROUP ====================

	IDetailGroup& GlobalGroup = PipelineCategory.AddGroup(
		"GlobalGroup", FText::FromString("Global Settings"), true, true);

	auto AddGlobalProp = [&](FName PropName, const FString& DisplayName)
	{
		TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(PropName);
		if (Handle->IsValidHandle())
		{
			GlobalGroup.AddPropertyRow(Handle).DisplayName(FText::FromString(DisplayName));
		}
	};

	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, GlobalSeed), "Global Seed");
	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TextureResolution), "Texture Resolution");
	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, LandmassType), "Landmass Type");
	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, TargetLandAreaSqKm), "Target Land Area (sq km)");
	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, OceanPaddingMeters), "Ocean Padding (m)");
	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, MaxMapHeight), "Max Map Height (m)");
	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, OceanLevel), "Ocean Level (m)");
	AddGlobalProp(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, SeaLevel), "Sea Level (normalized)");

	// ==================== STAGE GROUPS ====================

	// Helper to add a stage group with settings, button, and debug textures
	struct FStageInfo
	{
		FString GroupName;
		FString DisplayName;
		FName SettingsProp;
		FString ButtonLabel;
		TFunction<void(AWorldGenerationActor*)> ButtonAction;
		TArray<FName> DebugTextureProps;
	};

	TArray<FStageInfo> Stages;

	// Stage 1: Landmass
	{
		FStageInfo S;
		S.GroupName = "LandmassGroup";
		S.DisplayName = "1 - Landmass";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, LandmassSettings);
		S.ButtonLabel = "Generate Landmass";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step1_GenerateLandmass(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_Landmass));
		Stages.Add(MoveTemp(S));
	}

	// Stage 2: Uplift
	{
		FStageInfo S;
		S.GroupName = "UpliftGroup";
		S.DisplayName = "2 - Uplift/Geology";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, UpliftSettings);
		S.ButtonLabel = "Generate Uplift";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step2_GenerateUplift(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_BaseElevation));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_UpliftMap));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_CombinedElevation));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_PlateauMap));
		Stages.Add(MoveTemp(S));
	}

	// Stage 3: Hydrology
	{
		FStageInfo S;
		S.GroupName = "HydrologyGroup";
		S.DisplayName = "3 - Hydrology";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, HydrologySettings);
		S.ButtonLabel = "Generate Hydrology";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step3_GenerateHydrology(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_Rivers));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_Lakes));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_Waterfalls));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_FlowAccumulation));
		Stages.Add(MoveTemp(S));
	}

	// Stage 4: Erosion
	{
		FStageInfo S;
		S.GroupName = "ErosionGroup";
		S.DisplayName = "4 - Erosion";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, ErosionSettings);
		S.ButtonLabel = "Generate Erosion";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step4_GenerateErosion(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_ErodedElevation));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_CanyonMask));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_ErosionDelta));
		Stages.Add(MoveTemp(S));
	}

	// Stage 5: Climate
	{
		FStageInfo S;
		S.GroupName = "ClimateGroup";
		S.DisplayName = "5 - Climate";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, ClimateSettings);
		S.ButtonLabel = "Generate Climate";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step5_GenerateClimate(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_Temperature));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_Moisture));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_Precipitation));
		Stages.Add(MoveTemp(S));
	}

	// Stage 6: Biomes
	{
		FStageInfo S;
		S.GroupName = "BiomesGroup";
		S.DisplayName = "6 - Biomes";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, BiomeAssignmentSettings);
		S.ButtonLabel = "Generate Biomes";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step6_GenerateBiomes(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_BiomeMap));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_SlopeMap));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_BiomeBlendWeights));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_TerrainArchetypeMap));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_SurfaceOverlayMap));
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_GeneratedFeatureMap));
		Stages.Add(MoveTemp(S));
	}

	// Stage 7: Refinement
	{
		FStageInfo S;
		S.GroupName = "RefinementGroup";
		S.DisplayName = "7 - Terrain Refinement";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, RefinementSettings);
		S.ButtonLabel = "Generate Refinement";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step7_GenerateRefinement(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_FinalElevation));
		Stages.Add(MoveTemp(S));
	}

	// Stage 8: Caves
	{
		FStageInfo S;
		S.GroupName = "CavesGroup";
		S.DisplayName = "8 - Caves";
		S.SettingsProp = GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, CaveSettings);
		S.ButtonLabel = "Generate Caves";
		S.ButtonAction = [](AWorldGenerationActor* A) { A->Step8_GenerateCaves(); };
		S.DebugTextureProps.Add(GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, Debug_CavePresence));
		Stages.Add(MoveTemp(S));
	}

	// Build each stage group
	for (const FStageInfo& Stage : Stages)
	{
		IDetailGroup& Group = PipelineCategory.AddGroup(
			*Stage.GroupName, FText::FromString(Stage.DisplayName), false, true);

		// Settings property
		TSharedRef<IPropertyHandle> SettingsHandle = DetailBuilder.GetProperty(Stage.SettingsProp);
		if (SettingsHandle->IsValidHandle())
		{
			Group.AddPropertyRow(SettingsHandle).DisplayName(FText::FromString("Settings"));
		}

		// Generate button
		Group.AddWidgetRow()
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
					FText::FromString(Stage.ButtonLabel),
					Stage.ButtonAction,
					SelectedActors
				)
			];

		// Debug texture outputs
		for (const FName& TexProp : Stage.DebugTextureProps)
		{
			TSharedRef<IPropertyHandle> TexHandle = DetailBuilder.GetProperty(TexProp);
			if (TexHandle->IsValidHandle())
			{
				Group.AddPropertyRow(TexHandle);
			}
		}
	}

	// ==================== BATCH ACTIONS GROUP ====================

	IDetailGroup& ActionsGroup = PipelineCategory.AddGroup(
		"ActionsGroup", FText::FromString("Batch Actions"), true, true);

	// Randomize
	ActionsGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Randomize"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Randomize"),
				[](AWorldGenerationActor* A) { A->Randomize(); },
				SelectedActors
			)
		];

	// Randomize Seed Only
	ActionsGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Randomize Seed"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Randomize Seed Only"),
				[](AWorldGenerationActor* A) { A->RandomizeSeed(); },
				SelectedActors
			)
		];

	// Generate All
	ActionsGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Full Pipeline"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Generate All (Steps 1-7)"),
				[](AWorldGenerationActor* A) { A->GenerateAll(); },
				SelectedActors
			)
		];

	// Generate Mesh
	ActionsGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Mesh"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Generate Mesh"),
				[](AWorldGenerationActor* A) { A->GenerateMesh(); },
				SelectedActors
			)
		];

	// Validate Terrain
	ActionsGroup.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Validation"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			MakeButtonWidget(
				FText::FromString("Validate Terrain"),
				[](AWorldGenerationActor* A) { A->ValidateTerrain(); },
				SelectedActors
			)
		];

	// Clear All
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
				FText::FromString("Clear All"),
				[](AWorldGenerationActor* A) { A->ClearAll(); },
				SelectedActors
			)
		];

	// ==================== DEBUG INFO GROUP ====================

	IDetailGroup& DebugInfoGroup = PipelineCategory.AddGroup(
		"DebugInfoGroup", FText::FromString("Debug Info"), false, true);

	TSharedRef<IPropertyHandle> SeedUsedHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, ActualSeedUsed));
	if (SeedUsedHandle->IsValidHandle())
	{
		DebugInfoGroup.AddPropertyRow(SeedUsedHandle).DisplayName(FText::FromString("Actual Seed Used"));
	}

	TSharedRef<IPropertyHandle> ResUsedHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, ResolutionUsed));
	if (ResUsedHandle->IsValidHandle())
	{
		DebugInfoGroup.AddPropertyRow(ResUsedHandle).DisplayName(FText::FromString("Resolution Used"));
	}

	TSharedRef<IPropertyHandle> MapSizeHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, FinalMapSizeMeters));
	if (MapSizeHandle->IsValidHandle())
	{
		DebugInfoGroup.AddPropertyRow(MapSizeHandle).DisplayName(FText::FromString("Final Map Size (m)"));
	}

	TSharedRef<IPropertyHandle> ValidationHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(AWorldGenerationActor, LastValidationResult));
	if (ValidationHandle->IsValidHandle())
	{
		DebugInfoGroup.AddPropertyRow(ValidationHandle).DisplayName(FText::FromString("Validation Result"));
	}
}

// ==================== Helper: Create Button Widget ====================

TSharedRef<SWidget> FWorldGenerationActorCustomization::MakeButtonWidget(
	const FText& ButtonText,
	TFunction<void(AWorldGenerationActor*)> OnClickedLambda,
	const TArray<TWeakObjectPtr<AWorldGenerationActor>>& WeakActors)
{
	return SNew(SButton)
		.Text(ButtonText)
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([WeakActors, OnClickedLambda]() -> FReply
		{
			for (const TWeakObjectPtr<AWorldGenerationActor>& WeakActor : WeakActors)
			{
				if (AWorldGenerationActor* Actor = WeakActor.Get())
				{
					OnClickedLambda(Actor);
				}
			}
			return FReply::Handled();
		});
}

// ==================== Helper: Add Button Row to Category ====================

void FWorldGenerationActorCustomization::AddButtonRow(
	IDetailCategoryBuilder& Category,
	const FString& ButtonLabel,
	TFunction<void(AWorldGenerationActor*)> OnClickedLambda)
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
