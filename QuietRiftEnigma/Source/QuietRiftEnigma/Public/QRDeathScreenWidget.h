#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRDeathScreenWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;

/**
 * Full-screen death overlay: black fade-in + "YOU DIED" text + a
 * countdown to respawn. Constructed programmatically so no UMG asset
 * is required.
 *
 * Lifetime: AQRGameMode::HandlePlayerDied creates one, calls
 * Initialize(RespawnSeconds) to start the countdown, then removes it
 * after the timer fires by calling RestartPlayer.
 *
 * Designer can subclass to swap in a polished WBP layout.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRDeathScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRDeathScreenWidget(const FObjectInitializer& OI);

	// Begin the countdown. RespawnSeconds drives the ticking number;
	// the widget self-updates and stops at 0 (game mode handles the
	// actual respawn, this is purely cosmetic).
	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Initialize(float RespawnSeconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& InGeometry, float DeltaTime) override;

private:
	UPROPERTY()
	TObjectPtr<UBorder> FadeBorder = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> MainText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> CountdownText = nullptr;

	float Remaining = 0.0f;
	float Elapsed   = 0.0f;
};
