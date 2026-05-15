#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRCraftingWidget.generated.h"

class UVerticalBox;
class UScrollBox;
class UProgressBar;
class UTextBlock;
class UButton;
class UQRCraftingComponent;
class AQRCraftingBench;

/**
 * Crafting bench overlay. Pure C++ UMG.
 *
 *  ┌──────────────────────────────┐
 *  │  Workbench                   │
 *  │  [progress bar — current     │
 *  │   task] Crafting "X"…  37%   │
 *  │                              │
 *  │  Available Recipes           │
 *  │   • Recipe A      [Queue]    │
 *  │   • Recipe B      [Queue]    │
 *  │   …                          │
 *  │                              │
 *  │  Queued: 3                   │
 *  │  [Cancel Current] [Clear]    │
 *  │  [Close]                     │
 *  └──────────────────────────────┘
 *
 * Bind(bench) snapshots the recipes from the component's RecipeTable
 * and rebuilds the row list. Progress + queue count update each
 * NativeTick (4 Hz).
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRCraftingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRCraftingWidget(const FObjectInitializer& OI);

	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(AQRCraftingBench* InBench);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& InGeometry, float DeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<AQRCraftingBench> Bench = nullptr;

	UPROPERTY()
	TObjectPtr<UQRCraftingComponent> Crafting = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> CurrentTaskText = nullptr;

	UPROPERTY()
	TObjectPtr<UProgressBar> CurrentTaskBar = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> QueueCountText = nullptr;

	UPROPERTY()
	TObjectPtr<UScrollBox> RecipeList = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> CancelButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> ClearButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton = nullptr;

	float RefreshAccum = 0.0f;

	void RebuildRecipeList();
	void RefreshDynamic();

	UFUNCTION() void HandleCancel();
	UFUNCTION() void HandleClear();
	UFUNCTION() void HandleClose();

	UButton* MakeQueueRow(const FName& RecipeId, const FString& DisplayName);
};

/**
 * Internal helper button that remembers which recipe it represents,
 * so the OnClicked handler knows what to queue.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRRecipeQueueButton : public UButton
{
	GENERATED_BODY()

public:
	UQRRecipeQueueButton();

	UPROPERTY()
	FName RecipeId;

	UPROPERTY()
	TWeakObjectPtr<UQRCraftingComponent> TargetComp;

	UFUNCTION()
	void HandleClicked();
};
