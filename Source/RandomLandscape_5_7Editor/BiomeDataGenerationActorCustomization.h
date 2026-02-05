// BiomeDataGenerationActorCustomization.h
// IDetailCustomization for ABiomeDataGenerationActor - organized generation UI
// Version: 02.04.2026.23.55

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IDetailCategoryBuilder;
class ABiomeDataGenerationActor;

/**
 * Custom Details panel layout for ABiomeDataGenerationActor.
 * 
 * Organizes the actor's properties into a clean UX with:
 * - "Landmass" group: settings + Generate button + preview
 * - "Biomes" group: settings + Generate button + preview  
 * - "Batch Actions" group: Generate All + Clear buttons
 * 
 * All properties and buttons are placed under a single "Generation" category.
 * 
 * Safety: If property handles fail, falls back to default layout with buttons only.
 */
class FBiomeDataGenerationActorCustomization : public IDetailCustomization
{
public:
	/**
	 * Factory method called by the property editor to create an instance.
	 * Required signature for RegisterCustomClassLayout.
	 */
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ Begin IDetailCustomization Interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	//~ End IDetailCustomization Interface

private:
	// ==================== Helper Methods ====================
	
	/**
	 * Build a button widget that calls a method on all selected actors.
	 * @param ButtonText - Display text for the button
	 * @param OnClickedLambda - Lambda to execute on each selected actor
	 * @param WeakActors - Weak pointers to selected actors (captured by the button)
	 */
	TSharedRef<class SWidget> MakeButtonWidget(
		const FText& ButtonText,
		TFunction<void(ABiomeDataGenerationActor*)> OnClickedLambda,
		const TArray<TWeakObjectPtr<ABiomeDataGenerationActor>>& WeakActors
	);
	
	/**
	 * Add a button row directly to a category (fallback mode).
	 * @param Category - The category builder to add to
	 * @param ButtonLabel - Display label for the button
	 * @param OnClickedLambda - Lambda to execute on each selected actor
	 */
	void AddButtonRow(
		IDetailCategoryBuilder& Category,
		const FString& ButtonLabel,
		TFunction<void(ABiomeDataGenerationActor*)> OnClickedLambda
	);
	
	/**
	 * Build a preview widget showing a texture thumbnail.
	 * Shows "No preview generated" if texture is null.
	 * @param TexturePtr - Pointer to the UTexture2D to display
	 * @param Label - Label text (e.g., "Landmass Preview")
	 */
	TSharedRef<class SWidget> MakeTexturePreviewWidget(
		TObjectPtr<UTexture2D>* TexturePtr,
		const FText& Label
	);
	
	/**
	 * Get the first selected actor (for preview texture access).
	 * Returns nullptr if no valid actors selected.
	 */
	ABiomeDataGenerationActor* GetFirstSelectedActor() const;
	
	// ==================== Cached Data ====================
	
	/** Weak pointers to all selected actors (for multi-select support) */
	TArray<TWeakObjectPtr<ABiomeDataGenerationActor>> SelectedActors;
};
