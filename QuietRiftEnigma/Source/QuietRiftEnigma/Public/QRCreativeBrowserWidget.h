#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "QRCreativeBrowserWidget.generated.h"

class UEditableTextBox;
class UScrollBox;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UQRItemDefinition;
class UQRHotbarComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResultRowClicked, UQRItemDefinition*, Definition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotButtonClicked, int32, SlotIndex);

/**
 * UButton subclass that carries an item definition payload, so the
 * browser can identify which result row was clicked without scanning
 * IsHovered/IsPressed state.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRResultButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UQRItemDefinition> CarriedDefinition = nullptr;

	UPROPERTY(BlueprintAssignable)
	FOnResultRowClicked OnResultRowClicked;

	UQRResultButton();

private:
	UFUNCTION()
	void HandleClicked();
};

/** UButton subclass carrying a slot index for the hotbar assign row. */
UCLASS()
class QUIETRIFTENIGMA_API UQRSlotAssignButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintAssignable)
	FOnSlotButtonClicked OnSlotButtonClicked;

	UQRSlotAssignButton();

private:
	UFUNCTION()
	void HandleClicked();
};

/**
 * Programmatic creative-mode browser.
 *
 * Layout (top to bottom, all built in C++):
 *   Search box      — filters the result list as the user types
 *   Result scroll   — every UQRItemDefinition; click selects
 *   Status line     — "Selected: <name> — click a slot below to assign"
 *   Slot row 1..9   — click to assign selected def into that hotbar slot
 *
 * Toggled by the character's OnCreativeBrowserToggled event. When open,
 * sets input mode to GameAndUI and shows the mouse cursor; when closed,
 * back to Game-only.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRCreativeBrowserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRCreativeBrowserWidget(const FObjectInitializer& OI);

	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRHotbarComponent* InHotbar);

	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Toggle();

	UFUNCTION(BlueprintPure, Category = "QR|UI")
	bool IsOpen() const { return bOpen; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UQRHotbarComponent> Hotbar = nullptr;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> SearchBox = nullptr;

	UPROPERTY()
	TObjectPtr<UScrollBox> ResultsScroll = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusLabel = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UQRItemDefinition>> AllDefs;

	UPROPERTY()
	TObjectPtr<UQRItemDefinition> Selected = nullptr;

	bool bOpen = false;

	void Open();
	void Close();
	void ApplyInputMode();
	void RebuildResultList();
	void RefreshStatus();

	UFUNCTION()
	void OnSearchChanged(const FText& Text);

	UFUNCTION()
	void HandleResultRowClicked(UQRItemDefinition* Definition);

	UFUNCTION()
	void HandleSlotButtonClicked(int32 SlotIndex);
};
