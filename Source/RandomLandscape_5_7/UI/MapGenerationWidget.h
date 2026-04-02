// MapGenerationWidget.h
// In-game UI for configuring and previewing procedural world generation

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BiomeTypes.h"
#include "MapGenerationWidget.generated.h"

class AWorldGenerationActor;
class UImage;
class UButton;
class USlider;
class UTextBlock;
class UComboBoxString;
class UCheckBox;
class UEditableTextBox;
class UBorder;

/**
 * Runtime UMG widget that exposes world-generation settings to the player.
 *
 * Designed to sit on a full-screen solid-colour background so the player
 * never sees the world while tweaking settings.  A biome-map preview
 * image updates after every generation pass.
 *
 * To use: create a Widget Blueprint that derives from this class, bind the
 * meta-tagged sub-widgets listed below, then add it to the viewport from
 * a PlayerController or HUD class.
 */
UCLASS(Abstract, Blueprintable)
class RANDOMLANDSCAPE_5_7_API UMapGenerationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ==================== Blueprint Bindable Widgets ====================

	/** Solid-colour background that hides the world behind the UI. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BackgroundBorder;

	/** Preview image showing the latest biome map. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> PreviewImage;

	// --- Seed ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> SeedInput;

	// --- Landmass Type ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UComboBoxString> LandmassTypeCombo;

	// --- Terrain Roughness ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> TerrainRoughnessSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TerrainRoughnessLabel;

	// --- Target Land Area ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> LandAreaSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LandAreaLabel;

	// --- Biome Percentages ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> SnowPercentSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SnowPercentLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> DesertPercentSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DesertPercentLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USlider> ForestPercentSlider;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ForestPercentLabel;

	// --- Terrain Feature Toggles ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> VolcanoCheckBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> MountainsCheckBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> HillsCheckBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> PlateausCheckBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> RiversCheckBox;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCheckBox> CanyonsCheckBox;

	// --- Action Buttons ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> GenerateButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> RandomizeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> RandomizeSeedButton;

	// --- Status ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	// ==================== Configuration ====================

	/** Texture resolution used for preview generation (lower = faster). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation UI", meta = (ClampMin = "64", ClampMax = "1024"))
	int32 PreviewResolution = 256;

	/** Background colour shown behind the UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation UI")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.08f, 1.0f);

	// ==================== Public API ====================

	/** Find or spawn a WorldGenerationActor and wire up all delegates. Call from BP after adding to viewport if needed. */
	UFUNCTION(BlueprintCallable, Category = "Map Generation UI")
	void InitializeGenerator();

	/** Push current UI values to the actor and run the full pipeline + mesh. */
	UFUNCTION(BlueprintCallable, Category = "Map Generation UI")
	void Generate();

	/** Randomize everything (seed + settings) and regenerate. */
	UFUNCTION(BlueprintCallable, Category = "Map Generation UI")
	void RandomizeAll();

	/** Randomize seed only and regenerate. */
	UFUNCTION(BlueprintCallable, Category = "Map Generation UI")
	void RandomizeSeedOnly();

protected:
	virtual void NativeConstruct() override;

private:
	// ==================== Internal Helpers ====================

	/** Push all widget values → actor properties. */
	void SyncUIToActor();

	/** Pull actor properties → widget values (after randomize, etc.). */
	void SyncActorToUI();

	/** Grab Debug_BiomeMap from the actor and display it in PreviewImage. */
	void UpdatePreviewImage();

	/** Set a status message. */
	void SetStatus(const FString& Message);

	// ==================== Delegates ====================

	UFUNCTION() void OnGenerateClicked();
	UFUNCTION() void OnRandomizeClicked();
	UFUNCTION() void OnRandomizeSeedClicked();

	UFUNCTION() void OnTerrainRoughnessChanged(float Value);
	UFUNCTION() void OnLandAreaChanged(float Value);
	UFUNCTION() void OnSnowPercentChanged(float Value);
	UFUNCTION() void OnDesertPercentChanged(float Value);
	UFUNCTION() void OnForestPercentChanged(float Value);

	UFUNCTION() void OnSeedCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void OnLandmassTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION() void OnVolcanoToggled(bool bIsChecked);
	UFUNCTION() void OnMountainsToggled(bool bIsChecked);
	UFUNCTION() void OnHillsToggled(bool bIsChecked);
	UFUNCTION() void OnPlateausToggled(bool bIsChecked);
	UFUNCTION() void OnRiversToggled(bool bIsChecked);
	UFUNCTION() void OnCanyonsToggled(bool bIsChecked);

	// ==================== State ====================

	UPROPERTY()
	TObjectPtr<AWorldGenerationActor> GeneratorActor;
};
