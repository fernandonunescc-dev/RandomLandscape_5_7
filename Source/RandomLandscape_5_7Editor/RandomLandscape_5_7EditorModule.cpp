// RandomLandscape_5_7EditorModule.cpp
// Editor module implementation - registers Details panel customizations

#include "RandomLandscape_5_7EditorModule.h"
#include "PropertyEditorModule.h"
#include "WorldGenerationActorCustomization.h"
#include "MapGeneration/WorldGenerationActor.h"

// ==================== Module Implementation ====================

void FRandomLandscape_5_7EditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout(
		AWorldGenerationActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FWorldGenerationActorCustomization::MakeInstance)
	);

	PropertyModule.NotifyCustomizationModuleChanged();

	UE_LOG(LogTemp, Log, TEXT("RandomLandscape_5_7Editor: Registered Details customization for WorldGenerationActor"));
}

void FRandomLandscape_5_7EditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(AWorldGenerationActor::StaticClass()->GetFName());
	}

	UE_LOG(LogTemp, Log, TEXT("RandomLandscape_5_7Editor: Unregistered Details customization"));
}

IMPLEMENT_MODULE(FRandomLandscape_5_7EditorModule, RandomLandscape_5_7Editor)
