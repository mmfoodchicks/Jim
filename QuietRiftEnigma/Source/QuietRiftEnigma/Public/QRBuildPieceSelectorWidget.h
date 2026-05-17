#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "QRBuildPieceSelectorWidget.generated.h"

class UQRBuildModeComponent;
class UScrollBox;
class UTextBlock;

/**
 * Build piece selector. Shown while UQRBuildModeComponent::bBuildModeActive.
 * Lists every row in the component's PieceCatalog DataTable; clicking a
 * row calls SelectPiece(rowId) and dismisses the picker (mode stays
 * active, ghost updates to the chosen piece).
 *
 * Mounted by AQRCharacter when the player enters build mode via the
 * G→building dispatch. Esc / Right-click closes it without changing
 * the current piece.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRBuildPieceSelectorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRBuildPieceSelectorWidget(const FObjectInitializer& OI);

	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRBuildModeComponent* InBuild);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY()
	TObjectPtr<UQRBuildModeComponent> Build = nullptr;

	UPROPERTY()
	TObjectPtr<UScrollBox> List = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton = nullptr;

	UFUNCTION() void HandleClose();

	void RebuildList();
};

/**
 * One row in the selector. Carries the piece id + a weak reference to
 * the BuildMode component so the click handler can dispatch directly.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRBuildPieceButton : public UButton
{
	GENERATED_BODY()

public:
	UQRBuildPieceButton();

	UPROPERTY()
	FName PieceId;

	UPROPERTY()
	TWeakObjectPtr<UQRBuildModeComponent> TargetBuild;

	UPROPERTY()
	TWeakObjectPtr<UUserWidget> OwningWidget;

	UFUNCTION()
	void HandleClicked();
};
