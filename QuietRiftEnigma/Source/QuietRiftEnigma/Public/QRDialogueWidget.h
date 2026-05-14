#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRDialogueWidget.generated.h"

class UQRDialogueComponent;
class UTextBlock;
class UButton;

/**
 * Bottom-screen dialogue box. Binds to a UQRDialogueComponent's
 * OnLineSpoken / OnDialogueEnded events; shows the current speaker
 * and line text, gives the player a Continue button that calls
 * Advance() on the component.
 *
 * Auto-removes itself when the conversation ends.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRDialogueWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRDialogueWidget(const FObjectInitializer& OI);

	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRDialogueComponent* InDialogue);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UQRDialogueComponent> Dialogue = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> SpeakerText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> LineText = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> ContinueButton = nullptr;

	UFUNCTION()
	void HandleLineSpoken(FText Speaker, FText SubstitutedText);

	UFUNCTION()
	void HandleEnded();

	UFUNCTION()
	void HandleContinue();
};
