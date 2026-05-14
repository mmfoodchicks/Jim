#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRPauseMenuWidget.generated.h"

class UButton;
class UCanvasPanel;
class UBorder;
class UVerticalBox;
class UTextBlock;

/**
 * Pause overlay shown when the player hits Esc. Programmatic C++ UMG —
 * Resume / Save / Settings / Quit-to-Menu / Quit-to-Desktop buttons in
 * a center column over a half-opaque background.
 *
 * AQRCharacter binds Esc to TogglePause which spawns this on first call
 * and pauses the world. Resume / Save / Quit close the overlay and
 * unpause; Settings opens the settings widget on top.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRPauseMenuWidget(const FObjectInitializer& OI);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UButton> ResumeButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> SaveButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> SettingsButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> QuitToMenuButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> QuitToDesktopButton = nullptr;

	UFUNCTION() void HandleResume();
	UFUNCTION() void HandleSave();
	UFUNCTION() void HandleSettings();
	UFUNCTION() void HandleQuitToMenu();
	UFUNCTION() void HandleQuitToDesktop();

	UButton* MakeMenuButton(UVerticalBox* Parent, const FString& Label);
};
