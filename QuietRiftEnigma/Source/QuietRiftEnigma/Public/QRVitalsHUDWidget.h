#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRVitalsHUDWidget.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UQRSurvivalComponent;

/**
 * Programmatic vitals HUD. Bottom-left stack of progress bars driven by
 * UQRSurvivalComponent's replicated Health / Hunger / Thirst / Fatigue
 * / Oxygen values. Each row is "<label>  [▓▓▓░░░] <int>".
 *
 * Same pattern as UQRHotbarHUDWidget — no UMG asset required, the
 * widget tree is constructed in C++. Designer can swap a polished WBP
 * subclass via AQRCharacter::VitalsHUDClass later.
 *
 * The bound survival comp's OnHealthChanged delegate triggers refreshes
 * for Health; the other vitals refresh on NativeTick (twice per second)
 * since they don't have per-stat delegates. That's cheap — 5 rows, no
 * tweens.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRVitalsHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRVitalsHUDWidget(const FObjectInitializer& OI);

	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRSurvivalComponent* InSurvival);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& InGeometry, float DeltaTime) override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY()
	TObjectPtr<UQRSurvivalComponent> Survival = nullptr;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Root = nullptr;

	// Index order matches EVitalIndex below — Health/Stamina/Hunger/Thirst/Oxygen.
	UPROPERTY()
	TArray<TObjectPtr<UProgressBar>> Bars;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> Labels;

	float RefreshAccum = 0.0f;

	UFUNCTION()
	void HandleHealthChanged(float NewHealth);

	void RefreshAll();
	UHorizontalBox* MakeRow(int32 Index, const FText& LabelText, FLinearColor FillColor);
};
