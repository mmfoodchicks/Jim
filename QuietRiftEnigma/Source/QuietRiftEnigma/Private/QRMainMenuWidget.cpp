#include "QRMainMenuWidget.h"
#include "QRSaveGameSystem.h"
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

UQRMainMenuWidget::UQRMainMenuWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRMainMenuWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Full-screen dark gradient background placeholder.
		UBorder* BG = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		BG->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.06f, 1.0f));
		UCanvasPanelSlot* BGSlot = Canvas->AddChildToCanvas(BG);
		if (BGSlot)
		{
			BGSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BGSlot->SetOffsets(FMargin(0));
		}

		// Centered title + button column.
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UCanvasPanelSlot* ColSlot = Canvas->AddChildToCanvas(Column);
		if (ColSlot)
		{
			ColSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ColSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ColSlot->SetAutoSize(true);
		}

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Title->SetText(FText::FromString(TEXT("QUIET RIFT")));
		Title->SetJustification(ETextJustify::Center);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.90f, 0.95f, 1.0f)));
		{
			FSlateFontInfo Font = Title->GetFont();
			Font.Size = 60;
			Title->SetFont(Font);
		}
		UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
		if (TitleSlot) TitleSlot->SetHorizontalAlignment(HAlign_Center);

		UTextBlock* Subtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Subtitle->SetText(FText::FromString(TEXT("ENIGMA")));
		Subtitle->SetJustification(ETextJustify::Center);
		Subtitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.65f, 0.75f, 1.0f)));
		{
			FSlateFontInfo Font = Subtitle->GetFont();
			Font.Size = 22;
			Subtitle->SetFont(Font);
		}
		UVerticalBoxSlot* SubSlot = Column->AddChildToVerticalBox(Subtitle);
		if (SubSlot)
		{
			SubSlot->SetHorizontalAlignment(HAlign_Center);
			SubSlot->SetPadding(FMargin(0, 0, 0, 60));
		}

		NewGameButton  = MakeMenuButton(Column, TEXT("New Game"));
		ContinueButton = MakeMenuButton(Column, TEXT("Continue"));
		SettingsButton = MakeMenuButton(Column, TEXT("Settings"));
		QuitButton     = MakeMenuButton(Column, TEXT("Quit"));

		if (NewGameButton)  NewGameButton->OnClicked.AddDynamic(this, &UQRMainMenuWidget::HandleNewGame);
		if (ContinueButton) ContinueButton->OnClicked.AddDynamic(this, &UQRMainMenuWidget::HandleContinue);
		if (SettingsButton) SettingsButton->OnClicked.AddDynamic(this, &UQRMainMenuWidget::HandleSettings);
		if (QuitButton)     QuitButton->OnClicked.AddDynamic(this, &UQRMainMenuWidget::HandleQuit);

		// Disable Continue if no save exists. We check the default UE
		// save layer directly via UGameplayStatics to avoid spinning up
		// the QR save system actor just to ask.
		const bool bHasSave = UGameplayStatics::DoesSaveGameExist(AutosaveSlotName, 0);
		if (ContinueButton)
		{
			ContinueButton->SetIsEnabled(bHasSave);
		}
	}
	return Super::RebuildWidget();
}

UButton* UQRMainMenuWidget::MakeMenuButton(UVerticalBox* Parent, const FString& Label)
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

	UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Btn);
	if (Slot)
	{
		Slot->SetHorizontalAlignment(HAlign_Fill);
		Slot->SetPadding(FMargin(0, 6));
	}
	return Btn;
}

void UQRMainMenuWidget::HandleNewGame()
{
	QRUISound::PlayConfirm(this);
	// Wipe any previous quick-save so the gameplay map starts fresh.
	UGameplayStatics::DeleteGameInSlot(AutosaveSlotName, 0);
	UGameplayStatics::OpenLevel(this, GameplayLevelName);
}

void UQRMainMenuWidget::HandleContinue()
{
	QRUISound::PlayConfirm(this);
	UGameplayStatics::OpenLevel(this, GameplayLevelName);
}

void UQRMainMenuWidget::HandleSettings()
{
	QRUISound::PlayClick(this);
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("QR_OpenSettings"));
	}
}

void UQRMainMenuWidget::HandleQuit()
{
	QRUISound::PlayDeny(this);
	if (APlayerController* PC = GetOwningPlayer())
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}
