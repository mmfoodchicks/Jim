#include "QRDeathScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"

UQRDeathScreenWidget::UQRDeathScreenWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRDeathScreenWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Full-screen black border — alpha drives the fade-in.
		FadeBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		FadeBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
		UCanvasPanelSlot* BorderSlot = Canvas->AddChildToCanvas(FadeBorder);
		if (BorderSlot)
		{
			BorderSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BorderSlot->SetOffsets(FMargin(0.0f));
		}

		// Stacked text: "YOU DIED" + small countdown line below.
		UVerticalBox* TextStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		FadeBorder->SetContent(TextStack);

		MainText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		MainText->SetText(FText::FromString(TEXT("YOU DIED")));
		MainText->SetJustification(ETextJustify::Center);
		MainText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.10f, 0.10f, 0.0f)));
		{
			FSlateFontInfo Font = MainText->GetFont();
			Font.Size = 84;
			MainText->SetFont(Font);
		}
		UVerticalBoxSlot* MainSlot = TextStack->AddChildToVerticalBox(MainText);
		if (MainSlot)
		{
			MainSlot->SetHorizontalAlignment(HAlign_Center);
			MainSlot->SetVerticalAlignment(VAlign_Center);
		}

		CountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CountdownText->SetText(FText::FromString(TEXT("Respawning…")));
		CountdownText->SetJustification(ETextJustify::Center);
		CountdownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 0.0f)));
		{
			FSlateFontInfo Font = CountdownText->GetFont();
			Font.Size = 22;
			CountdownText->SetFont(Font);
		}
		UVerticalBoxSlot* CountdownSlot = TextStack->AddChildToVerticalBox(CountdownText);
		if (CountdownSlot)
		{
			CountdownSlot->SetHorizontalAlignment(HAlign_Center);
			CountdownSlot->SetVerticalAlignment(VAlign_Center);
			CountdownSlot->SetPadding(FMargin(0, 20, 0, 0));
		}
	}
	return Super::RebuildWidget();
}

void UQRDeathScreenWidget::Initialize(float RespawnSeconds)
{
	Remaining = FMath::Max(0.0f, RespawnSeconds);
	Elapsed   = 0.0f;
}

void UQRDeathScreenWidget::NativeTick(const FGeometry& InGeometry, float DeltaTime)
{
	Super::NativeTick(InGeometry, DeltaTime);

	Elapsed   += DeltaTime;
	Remaining = FMath::Max(0.0f, Remaining - DeltaTime);

	// Fade in over the first second.
	const float FadeAlpha = FMath::Clamp(Elapsed / 1.0f, 0.0f, 1.0f);
	if (FadeBorder)
	{
		FadeBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, FadeAlpha * 0.85f));
	}
	if (MainText)
	{
		MainText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.10f, 0.10f, FadeAlpha)));
	}
	if (CountdownText)
	{
		const FString Body = FString::Printf(TEXT("Respawning in %d…"), FMath::CeilToInt(Remaining));
		CountdownText->SetText(FText::FromString(Body));
		CountdownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, FadeAlpha)));
	}
}
