#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRTypes.h"
#include "QRDevOverlay.generated.h"

// In-PIE debug overlay widget.
// Create via Blueprint (WBP_DevOverlay) and add to viewport in a development-only HUD.
// Call RefreshFromPawn() every frame (or bind to a timer) to keep display current.
// All displayed values are public properties so Blueprint text blocks can bind directly.
UCLASS(Abstract)
class QRUI_API UQRDevOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	// ── Displayed Data (populated by RefreshFromPawn) ──────────
	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float HealthPct = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float HungerPct = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float ThirstPct = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float FatiguePct = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float CoreTemperature = 37.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float EncumbranceRatio = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float ScentIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float WeaponFouling = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	bool bWeaponJammed = false;

	// Colony / leader stats
	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float MoraleIndex = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float LeaderBuff = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	int32 LeaderLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float IssueEscalationScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	EQRWeatherEvent ActiveWeather = EQRWeatherEvent::None;

	UPROPERTY(BlueprintReadOnly, Category = "DevOverlay")
	float WeatherIntensity = 0.0f;

	// ── Config ────────────────────────────────────────────
	// Auto-refresh interval in seconds (0 = refresh every frame)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DevOverlay")
	float RefreshIntervalSeconds = 0.25f;

	// ── Interface ─────────────────────────────────────────
	// Pull data from the pawn and its components; safe to call with a null pawn
	UFUNCTION(BlueprintCallable, Category = "DevOverlay")
	void RefreshFromPawn(APawn* TargetPawn);

	// Show / hide the overlay
	UFUNCTION(BlueprintCallable, Category = "DevOverlay")
	void ToggleOverlayVisibility();

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float TimeSinceRefresh = 0.0f;
	bool bVisible = true;
};
