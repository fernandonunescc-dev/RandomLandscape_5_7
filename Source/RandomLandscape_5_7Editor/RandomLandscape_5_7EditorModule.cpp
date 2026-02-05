// RandomLandscape_5_7EditorModule.cpp
// Editor module implementation - registers Details panel customizations
// Version: 02.04.2026.23.37

#include "RandomLandscape_5_7EditorModule.h"
#include "PropertyEditorModule.h"
#include "BiomeDataGenerationActorCustomization.h"
#include "MapGeneration/BiomeDataGenerationActor.h"

// ==================== Module Implementation ====================

void FRandomLandscape_5_7EditorModule::StartupModule()
{
	// Register custom Details panel layout for ABiomeDataGenerationActor
	// This gives us full control over how the actor's properties appear in the editor
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	
	// Register the customization class for our actor
	// When any ABiomeDataGenerationActor is selected, our customization will handle the Details panel
	PropertyModule.RegisterCustomClassLayout(
		ABiomeDataGenerationActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FBiomeDataGenerationActorCustomization::MakeInstance)
	);
	
	// Refresh the property module to apply changes immediately
	PropertyModule.NotifyCustomizationModuleChanged();
	
	UE_LOG(LogTemp, Log, TEXT("RandomLandscape_5_7Editor: Registered Details customization for BiomeDataGenerationActor"));
}

void FRandomLandscape_5_7EditorModule::ShutdownModule()
{
	// Unregister our customization when the module shuts down
	// This prevents crashes if the editor tries to use our customization after unload
	
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(ABiomeDataGenerationActor::StaticClass()->GetFName());
	}
	
	UE_LOG(LogTemp, Log, TEXT("RandomLandscape_5_7Editor: Unregistered Details customization"));
}

// ==================== Module Registration Macro ====================
// This macro tells UE which class implements IModuleInterface for this module

IMPLEMENT_MODULE(FRandomLandscape_5_7EditorModule, RandomLandscape_5_7Editor)
