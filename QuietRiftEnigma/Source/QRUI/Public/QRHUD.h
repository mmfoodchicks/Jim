#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "QRHUD.generated.h"

class UQRSurvivalWidget;
class UQRInventoryWidget;
class UQRResearchWidget;
class UQRColonyReportWidget;
class UQRRaidAlertWidget;
class UQRCodexWidget;
class UQRCrosshairWidget;
class UQRDebugOverlayWidget;

// Central HUD actor — owns and controls all UMG widget layers
UCLASS(BlueprintType, Blueprintable)
class QRUI_API AQRHUD : public AHUD
{
	GENERATED_BODY()

public:
	AQRHUD();

	// ── Widget Classes (assign in BP subclass) ─
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> SurvivalWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> InventoryWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> ResearchWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> ColonyReportWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> RaidAlertWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> CodexWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> CrosshairWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Widgets")
	TSubclassOf<UUserWidget> DebugOverlayWidgetClass;

	// ── Runtime Widget Refs ───────────────────
	UPROPERTY(BlueprintReadOnly, Category = "HUD|Widgets")
	TObjectPtr<UUserWidget> SurvivalWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Widgets")
	TObjectPtr<UUserWidget> InventoryWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Widgets")
	TObjectPtr<UUserWidget> ResearchWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Widgets")
	TObjectPtr<UUserWidget> ColonyReportWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Widgets")
	TObjectPtr<UUserWidget> RaidAlertWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Widgets")
	TObjectPtr<UUserWidget> CodexWidget;

	UPROPERTY(BlueprintReadOnly, Category = "HUD|Widgets")
	TObjectPtr<UUserWidget> CrosshairWidget;

	// ── State ─────────────────────────────────
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	bool bInventoryOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	bool bResearchOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	bool bCodexOpen = false;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleInventory();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleResearch();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ToggleCodex();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowRaidAlert(float ThreatScore);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideRaidAlert();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowDebugOverlay(bool bShow);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowInteractionPrompt(FText ActionText, FText TargetName);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void HideInteractionPrompt();

	// Notification system for transient on-screen messages
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void PushNotification(FText Message, float Duration = 5.0f, bool bIsWarning = false);

	virtual void BeginPlay() override;

private:
	UUserWidget* CreateAndAddWidget(TSubclassOf<UUserWidget> WidgetClass, int32 ZOrder = 0);
};
