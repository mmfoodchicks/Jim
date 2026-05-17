#include "QRPauseMenuWidget.h"
#include "QRGameMode.h"
#include "QRUISound.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

UQRPauseMenuWidget::UQRPauseMenuWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRPauseMenuWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Full-screen dim background.
		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Dim->SetBrushColor(FLinearColor(0, 0, 0, 0.55f));
		UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Dim);
		if (DimSlot)
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0));
		}

		// Center column with title + buttons.
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UCanvasPanelSlot* ColSlot = Canvas->AddChildToCanvas(Column);
		if (ColSlot)
		{
			ColSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ColSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ColSlot->SetAutoSize(true);
		}

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Title->SetText(FText::FromString(TEXT("PAUSED")));
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = Title->GetFont();
			Font.Size = 42;
			Title->SetFont(Font);
		}
		UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
		if (TitleSlot)
		{
			TitleSlot->SetHorizontalAlignment(HAlign_Center);
			TitleSlot->SetPadding(FMargin(0, 0, 0, 24));
		}

		ResumeButton        = MakeMenuButton(Column, TEXT("Resume"));
		SaveButton          = MakeMenuButton(Column, TEXT("Save Game"));
		SettingsButton      = MakeMenuButton(Column, TEXT("Settings"));
		QuitToMenuButton    = MakeMenuButton(Column, TEXT("Quit to Main Menu"));
		QuitToDesktopButton = MakeMenuButton(Column, TEXT("Quit to Desktop"));

		if (ResumeButton)        ResumeButton->OnClicked.AddDynamic(this, &UQRPauseMenuWidget::HandleResume);
		if (SaveButton)          SaveButton->OnClicked.AddDynamic(this, &UQRPauseMenuWidget::HandleSave);
		if (SettingsButton)      SettingsButton->OnClicked.AddDynamic(this, &UQRPauseMenuWidget::HandleSettings);
		if (QuitToMenuButton)    QuitToMenuButton->OnClicked.AddDynamic(this, &UQRPauseMenuWidget::HandleQuitToMenu);
		if (QuitToDesktopButton) QuitToDesktopButton->OnClicked.AddDynamic(this, &UQRPauseMenuWidget::HandleQuitToDesktop);
	}
	return Super::RebuildWidget();
}

UButton* UQRPauseMenuWidget::MakeMenuButton(UVerticalBox* Parent, const FString& Label)
{
	if (!Parent || !WidgetTree) return nullptr;

	UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

	UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TB->SetText(FText::FromString(Label));
	TB->SetJustification(ETextJustify::Center);
	TB->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = TB->GetFont();
		Font.Size = 18;
		TB->SetFont(Font);
	}
	Btn->AddChild(TB);

	UVerticalBoxSlot* BtnSlot = Parent->AddChildToVerticalBox(Btn);
	if (BtnSlot)
	{
		BtnSlot->SetHorizontalAlignment(HAlign_Fill);
		BtnSlot->SetPadding(FMargin(0, 4));
	}
	return Btn;
}

void UQRPauseMenuWidget::HandleResume()
{
	QRUISound::PlayClick(this);
	if (APlayerController* PC = GetOwningPlayer())
	{
		UGameplayStatics::SetGamePaused(this, false);
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
	RemoveFromParent();
}

void UQRPauseMenuWidget::HandleSave()
{
	QRUISound::PlayConfirm(this);
	if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
	{
		GM->QuickSave();
	}
}

void UQRPauseMenuWidget::HandleSettings()
{
	QRUISound::PlayClick(this);
	// Settings widget is wired through the player character so we
	// don't take a hard dep on its class here. AQRCharacter watches
	// for this notify-style call via a console hook for now —
	// designer can wire a real button in BP.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("QR_OpenSettings"));
	}
}

void UQRPauseMenuWidget::HandleQuitToMenu()
{
	QRUISound::PlayClick(this);
	if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
	{
		GM->QuickSave();  // autosave on leave
	}
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(TEXT("L_MainMenu")));
}

void UQRPauseMenuWidget::HandleQuitToDesktop()
{
	QRUISound::PlayDeny(this);
	if (AQRGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AQRGameMode>() : nullptr)
	{
		GM->QuickSave();
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
	}
}
