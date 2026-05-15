#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRScopeOverlayWidget.generated.h"

class UQRFPViewComponent;
class UBorder;
class UImage;

/**
 * Scope reticle overlay. Visible only while the FP view component
 * reports bScopeAvailable && IsADS(). Lightweight version: a black
 * full-screen border with a circular cutout simulated via 4 black
 * Borders (top/bottom/left/right) framing a central reticle Image.
 *
 * A real render-to-texture scope (separate camera, scope mesh, RT
 * material) is a substantial follow-up; this widget covers the
 * "you can see you're zoomed" gameplay read without that complexity.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRScopeOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRScopeOverlayWidget(const FObjectInitializer& OI);

	UFUNCTION(BlueprintCallable, Category = "QR|Scope")
	void Bind(UQRFPViewComponent* InView);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& InGeometry, float DeltaTime) override;

private:
	UPROPERTY() TObjectPtr<UQRFPViewComponent> View = nullptr;

	UPROPERTY() TObjectPtr<UBorder> LetterboxTop = nullptr;
	UPROPERTY() TObjectPtr<UBorder> LetterboxBottom = nullptr;
	UPROPERTY() TObjectPtr<UBorder> LetterboxLeft = nullptr;
	UPROPERTY() TObjectPtr<UBorder> LetterboxRight = nullptr;
	UPROPERTY() TObjectPtr<UImage>  Reticle = nullptr;

	void SetActiveVisuals(bool bActive);
	bool bWasActive = false;
};
