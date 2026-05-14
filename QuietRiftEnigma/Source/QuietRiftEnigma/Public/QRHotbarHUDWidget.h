#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRHotbarHUDWidget.generated.h"

class UHorizontalBox;
class UBorder;
class UTextBlock;
class UQRHotbarComponent;
class UQRItemInstance;

/**
 * Programmatic hotbar HUD. Builds 9 slot tiles in a horizontal row at the
 * bottom-center of the screen. Each tile shows the slot index, item name
 * (or "—" for empty), and stack quantity. The currently-active slot is
 * tinted.
 *
 * No UMG asset is required — the widget tree is constructed in C++ via
 * UWidgetTree::ConstructWidget so the harness can show something even
 * before the user authors a polished WBP version in the editor.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRHotbarHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRHotbarHUDWidget(const FObjectInitializer& OI);

	// Bind to a character's UQRHotbarComponent so the HUD updates when
	// slots / active index change.
	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRHotbarComponent* InHotbar);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UQRHotbarComponent> Hotbar = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UBorder>> SlotBorders;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> SlotLabels;

	void RefreshAll();
	void RefreshSlot(int32 SlotIndex);

	UFUNCTION()
	void HandleSlotChanged(int32 SlotIndex, UQRItemInstance* NewItem);

	UFUNCTION()
	void HandleActiveSlotChanged(int32 NewActiveSlot);
};
