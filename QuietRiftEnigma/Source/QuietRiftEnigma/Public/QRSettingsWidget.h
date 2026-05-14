#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRSettingsWidget.generated.h"

class USlider;
class UTextBlock;
class UButton;
class UVerticalBox;

/**
 * Minimal settings panel — three sliders (Mouse Sensitivity / FOV /
 * Master Volume) and a Close button. Pure C++ UMG, no asset required.
 *
 * Values are written to /Script/Engine.GameUserSettings via GConfig so
 * they survive across sessions without us shipping our own config
 * subsystem. The character's FP view component reads FOV on tick and
 * the look input multiplies sensitivity locally.
 *
 * Opened by ConsoleCommand("QR_OpenSettings") from the pause / main
 * menus. AQRCharacter exposes that exec for in-game opening.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRSettingsWidget(const FObjectInitializer& OI);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<USlider> SensitivitySlider = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> SensitivityValue = nullptr;

	UPROPERTY()
	TObjectPtr<USlider> FOVSlider = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> FOVValue = nullptr;

	UPROPERTY()
	TObjectPtr<USlider> VolumeSlider = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> VolumeValue = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton = nullptr;

	UFUNCTION() void HandleSensitivity(float NewValue);
	UFUNCTION() void HandleFOV(float NewValue);
	UFUNCTION() void HandleVolume(float NewValue);
	UFUNCTION() void HandleClose();

	void MakeSliderRow(UVerticalBox* Parent, const FString& Label,
		float Min, float Max, float Current,
		USlider*& OutSlider, UTextBlock*& OutValue);
};
