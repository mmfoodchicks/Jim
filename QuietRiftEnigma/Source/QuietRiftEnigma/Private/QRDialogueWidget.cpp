#include "QRDialogueWidget.h"
#include "QRDialogueComponent.h"
#include "QRUISound.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

UQRDialogueWidget::UQRDialogueWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRDialogueWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Bottom-center letterboxed box.
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.78f));
		Panel->SetPadding(FMargin(24.0f));
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		if (PanelSlot)
		{
			PanelSlot->SetAnchors(FAnchors(0.1f, 1.0f, 0.9f, 1.0f));
			PanelSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			PanelSlot->SetOffsets(FMargin(0.0f, -240.0f, 0.0f, 24.0f));
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);

		SpeakerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		SpeakerText->SetText(FText::FromString(TEXT("...")));
		SpeakerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.65f, 1.0f)));
		{
			FSlateFontInfo Font = SpeakerText->GetFont();
			Font.Size = 18;
			SpeakerText->SetFont(Font);
		}
		UVerticalBoxSlot* SS = Column->AddChildToVerticalBox(SpeakerText);
		if (SS) SS->SetPadding(FMargin(0, 0, 0, 8));

		LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LineText->SetText(FText::FromString(TEXT("")));
		LineText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		LineText->SetAutoWrapText(true);
		{
			FSlateFontInfo Font = LineText->GetFont();
			Font.Size = 16;
			LineText->SetFont(Font);
		}
		Column->AddChildToVerticalBox(LineText);

		ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* CT = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CT->SetText(FText::FromString(TEXT("Continue")));
		CT->SetJustification(ETextJustify::Center);
		CT->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ContinueButton->AddChild(CT);
		UVerticalBoxSlot* CBS = Column->AddChildToVerticalBox(ContinueButton);
		if (CBS) CBS->SetPadding(FMargin(0, 16, 0, 0));
		ContinueButton->OnClicked.AddDynamic(this, &UQRDialogueWidget::HandleContinue);
	}
	return Super::RebuildWidget();
}

void UQRDialogueWidget::Bind(UQRDialogueComponent* InDialogue)
{
	if (Dialogue)
	{
		Dialogue->OnLineSpoken.RemoveDynamic(this, &UQRDialogueWidget::HandleLineSpoken);
		Dialogue->OnDialogueEnded.RemoveDynamic(this, &UQRDialogueWidget::HandleEnded);
	}
	Dialogue = InDialogue;
	if (Dialogue)
	{
		Dialogue->OnLineSpoken.AddDynamic(this, &UQRDialogueWidget::HandleLineSpoken);
		Dialogue->OnDialogueEnded.AddDynamic(this, &UQRDialogueWidget::HandleEnded);

		// Catch the case where Bind fires after the first line already
		// emitted (we missed the broadcast) — pull current line manually.
		if (Dialogue->IsTalking())
		{
			if (SpeakerText) SpeakerText->SetText(Dialogue->GetCurrentSpeaker());
			if (LineText)    LineText->SetText(Dialogue->GetCurrentSubstitutedText());
		}
	}
}

void UQRDialogueWidget::HandleLineSpoken(FText Speaker, FText SubstitutedText)
{
	if (SpeakerText) SpeakerText->SetText(Speaker);
	if (LineText)    LineText->SetText(SubstitutedText);
}

void UQRDialogueWidget::HandleEnded()
{
	RemoveFromParent();
}

void UQRDialogueWidget::HandleContinue()
{
	QRUISound::PlayClick(this);
	if (Dialogue) Dialogue->Advance();
}

void UQRDialogueWidget::NativeDestruct()
{
	if (Dialogue)
	{
		Dialogue->OnLineSpoken.RemoveDynamic(this, &UQRDialogueWidget::HandleLineSpoken);
		Dialogue->OnDialogueEnded.RemoveDynamic(this, &UQRDialogueWidget::HandleEnded);
		Dialogue = nullptr;
	}
	Super::NativeDestruct();
}
