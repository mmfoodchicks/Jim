#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRCodexWidget.generated.h"

class UVerticalBox;
class UScrollBox;
class UHorizontalBox;
class UButton;
class UTextBlock;
class UQRCodexSubsystem;

/**
 * Codex screen. Left column = category buttons (Flora / Fauna /
 * Items / POIs / Biomes / Remnants). Right column = scroll list of
 * entries for the selected category, each rendered as a row with
 * its DisplayName and state badge (Unknown silhouette / Observed /
 * Sampled / Researched).
 *
 * Pure C++ UMG — designer subclasses to swap in a polished WBP_
 * version. Opens via the K key from AQRCharacter.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRCodexWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRCodexWidget(const FObjectInitializer& OI);

	// Called by UQRCodexCategoryButton when clicked.
	UFUNCTION(BlueprintCallable, Category = "QR|Codex")
	void SetCategory(FName Category);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY() TObjectPtr<UQRCodexSubsystem> Codex = nullptr;

	UPROPERTY() TObjectPtr<UVerticalBox> CategoryColumn = nullptr;
	UPROPERTY() TObjectPtr<UScrollBox>   EntryList      = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock>   HeaderText     = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock>   ProgressText   = nullptr;

	FName CurrentCategory = TEXT("Fauna");

	UFUNCTION() void HandleClose();
	void RefreshCategories();
	void RefreshEntryList();
};


/**
 * Internal helper button — carries a category FName so click routes
 * back to the owning widget without per-instance OnClicked plumbing.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRCodexCategoryButton : public UButton
{
	GENERATED_BODY()

public:
	UQRCodexCategoryButton();

	UPROPERTY() FName Category;
	UPROPERTY() TWeakObjectPtr<UQRCodexWidget> Owner;

	UFUNCTION() void HandleClicked();
};
