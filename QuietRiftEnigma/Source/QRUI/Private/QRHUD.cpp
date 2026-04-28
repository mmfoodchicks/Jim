#include "QRHUD.h"
#include "Blueprint/UserWidget.h"

AQRHUD::AQRHUD()
{
}

void AQRHUD::BeginPlay()
{
	Super::BeginPlay();

	// Create persistent HUD widgets
	SurvivalWidget     = CreateAndAddWidget(SurvivalWidgetClass, 10);
	CrosshairWidget    = CreateAndAddWidget(CrosshairWidgetClass, 5);
	RaidAlertWidget    = CreateAndAddWidget(RaidAlertWidgetClass, 20);

	// Create but don't add inventory/research/codex until toggled
	if (InventoryWidgetClass)
		InventoryWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), InventoryWidgetClass);
	if (ResearchWidgetClass)
		ResearchWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), ResearchWidgetClass);
	if (CodexWidgetClass)
		CodexWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), CodexWidgetClass);
	if (ColonyReportWidgetClass)
		ColonyReportWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), ColonyReportWidgetClass);

	if (RaidAlertWidget) RaidAlertWidget->SetVisibility(ESlateVisibility::Collapsed);
}

UUserWidget* AQRHUD::CreateAndAddWidget(TSubclassOf<UUserWidget> WidgetClass, int32 ZOrder)
{
	if (!WidgetClass || !GetOwningPlayerController()) return nullptr;
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetOwningPlayerController(), WidgetClass);
	if (Widget) Widget->AddToViewport(ZOrder);
	return Widget;
}

void AQRHUD::ToggleInventory()
{
	if (!InventoryWidget) return;
	bInventoryOpen = !bInventoryOpen;

	if (bInventoryOpen)
	{
		InventoryWidget->AddToViewport(15);
		// Release mouse for inventory navigation
		if (APlayerController* PC = GetOwningPlayerController())
		{
			FInputModeGameAndUI Mode;
			PC->SetInputMode(Mode);
			PC->SetShowMouseCursor(true);
		}
	}
	else
	{
		InventoryWidget->RemoveFromParent();
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}
}

void AQRHUD::ToggleResearch()
{
	if (!ResearchWidget) return;
	bResearchOpen = !bResearchOpen;

	if (bResearchOpen)
	{
		ResearchWidget->AddToViewport(16);
		if (APlayerController* PC = GetOwningPlayerController())
		{
			FInputModeGameAndUI Mode;
			PC->SetInputMode(Mode);
			PC->SetShowMouseCursor(true);
		}
	}
	else
	{
		ResearchWidget->RemoveFromParent();
		if (APlayerController* PC = GetOwningPlayerController())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	}
}

void AQRHUD::ToggleCodex()
{
	if (!CodexWidget) return;
	bCodexOpen = !bCodexOpen;
	if (bCodexOpen) CodexWidget->AddToViewport(17);
	else CodexWidget->RemoveFromParent();
}

void AQRHUD::ShowRaidAlert(float ThreatScore)
{
	if (RaidAlertWidget) RaidAlertWidget->SetVisibility(ESlateVisibility::Visible);
}

void AQRHUD::HideRaidAlert()
{
	if (RaidAlertWidget) RaidAlertWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void AQRHUD::ShowDebugOverlay(bool bShow)
{
	if (!DebugOverlayWidgetClass) return;
	// Debug overlay created on demand
	static TObjectPtr<UUserWidget> DebugWidget = nullptr;
	if (bShow && !DebugWidget)
	{
		DebugWidget = CreateAndAddWidget(DebugOverlayWidgetClass, 100);
	}
	else if (!bShow && DebugWidget)
	{
		DebugWidget->RemoveFromParent();
		DebugWidget = nullptr;
	}
}

void AQRHUD::ShowInteractionPrompt(FText ActionText, FText TargetName)
{
	// Blueprint implements the visual; C++ just triggers the event via BPImplementableEvent
}

void AQRHUD::HideInteractionPrompt()
{
	// Companion to ShowInteractionPrompt
}

void AQRHUD::PushNotification(FText Message, float Duration, bool bIsWarning)
{
	// Delegate to Blueprint implementation for animated notification display
}
