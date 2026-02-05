// RandomLandscape_5_7EditorModule.h
// Editor module header - registers Details panel customizations
// Version: 02.04.2026.23.37

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Editor module for RandomLandscape_5_7.
 * Registers custom Details panel layouts for editor-only UX improvements.
 */
class FRandomLandscape_5_7EditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
