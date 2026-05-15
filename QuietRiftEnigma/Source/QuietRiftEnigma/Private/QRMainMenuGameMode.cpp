#include "QRMainMenuGameMode.h"
#include "QRMainMenuWidget.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"

AQRMainMenuGameMode::AQRMainMenuGameMode()
{
	MainMenuClass = UQRMainMenuWidget::StaticClass();
}

void AQRMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!MainMenuClass) return;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	MainMenu = CreateWidget<UQRMainMenuWidget>(PC, MainMenuClass);
	if (!MainMenu) return;

	MainMenu->AddToViewport(/*ZOrder*/ 1);
	PC->bShowMouseCursor = true;
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(MainMenu->TakeWidget());
	PC->SetInputMode(Mode);
}
