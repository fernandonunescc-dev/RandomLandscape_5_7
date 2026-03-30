// WorldGenerationActorCustomization.h
// IDetailCustomization for AWorldGenerationActor - organized pipeline UI

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IDetailCategoryBuilder;
class AWorldGenerationActor;

/**
 * Custom Details panel layout for AWorldGenerationActor.
 *
 * Organizes the actor's properties into a clean UX with:
 * - "Global Settings" group
 * - Per-stage groups (Landmass, Uplift, Hydrology, Erosion, Climate, Biomes, Refinement)
 *   each with Settings + Generate button + debug texture outputs
 * - "Batch Actions" group: Generate All, Generate Mesh, Validate, Clear
 *
 * Safety: If property handles fail, falls back to default layout with buttons only.
 */
class FWorldGenerationActorCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface

private:
	TSharedRef<class SWidget> MakeButtonWidget(
		const FText& ButtonText,
		TFunction<void(AWorldGenerationActor*)> OnClickedLambda,
		const TArray<TWeakObjectPtr<AWorldGenerationActor>>& WeakActors
	);

	void AddButtonRow(
		IDetailCategoryBuilder& Category,
		const FString& ButtonLabel,
		TFunction<void(AWorldGenerationActor*)> OnClickedLambda
	);

	TArray<TWeakObjectPtr<AWorldGenerationActor>> SelectedActors;
};
