#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRMainMenuWidget.generated.h"

class UButton;
class UVerticalBox;

/**
 * Main menu shown by AQRMainMenuGameMode on L_MainMenu. Programmatic
 * C++ UMG with title + 4 buttons: New Game / Continue / Settings /
 * Quit. Continue is enabled only if a save exists.
 *
 * New Game / Continue open the gameplay map. Settings opens the
 * settings widget. Quit closes the application.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRMainMenuWidget(const FObjectInitializer& OI);

	// Gameplay level to open on New Game / Continue. Defaults to
	// "L_DevTest"; override per project.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Menu")
	FName GameplayLevelName = TEXT("L_DevTest");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|Menu")
	FString AutosaveSlotName = TEXT("QuickSave");

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UButton> NewGameButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> SettingsButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> QuitButton = nullptr;

	UFUNCTION() void HandleNewGame();
	UFUNCTION() void HandleContinue();
	UFUNCTION() void HandleSettings();
	UFUNCTION() void HandleQuit();

	UButton* MakeMenuButton(UVerticalBox* Parent, const FString& Label);
};
