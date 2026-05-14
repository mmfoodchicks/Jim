#include "QRBuildPieceSelectorWidget.h"
#include "QRBuildModeComponent.h"
#include "QRBuildTypes.h"
#include "QRUISound.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/DataTable.h"

UQRBuildPieceButton::UQRBuildPieceButton()
{
	OnClicked.AddDynamic(this, &UQRBuildPieceButton::HandleClicked);
}

void UQRBuildPieceButton::HandleClicked()
{
	QRUISound::PlayConfirm(this);
	if (TargetBuild.IsValid() && !PieceId.IsNone())
	{
		TargetBuild->SelectPiece(PieceId);
	}
	// Dismiss the picker — designer can show it again by re-entering
	// the build menu key. Build mode stays active.
	if (OwningWidget.IsValid())
	{
		OwningWidget->RemoveFromParent();
	}
}


UQRBuildPieceSelectorWidget::UQRBuildPieceSelectorWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRBuildPieceSelectorWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.92f));
		Panel->SetPadding(FMargin(20.0f));
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		if (PanelSlot)
		{
			// Right-side column.
			PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
			PanelSlot->SetSize(FVector2D(360.0f, 560.0f));
			PanelSlot->SetPosition(FVector2D(-24.0f, 0.0f));
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TitleText->SetText(FText::FromString(TEXT("Build Pieces")));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = TitleText->GetFont();
			Font.Size = 22;
			TitleText->SetFont(Font);
		}
		UVerticalBoxSlot* TS = Column->AddChildToVerticalBox(TitleText);
		if (TS) TS->SetPadding(FMargin(0, 0, 0, 12));

		List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBoxSlot* LS = Column->AddChildToVerticalBox(List);
		if (LS) LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* CT = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CT->SetText(FText::FromString(TEXT("Close")));
		CT->SetJustification(ETextJustify::Center);
		CT->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CloseButton->AddChild(CT);
		UVerticalBoxSlot* CBS = Column->AddChildToVerticalBox(CloseButton);
		if (CBS) CBS->SetPadding(FMargin(0, 12, 0, 0));
		CloseButton->OnClicked.AddDynamic(this, &UQRBuildPieceSelectorWidget::HandleClose);
	}
	return Super::RebuildWidget();
}

void UQRBuildPieceSelectorWidget::Bind(UQRBuildModeComponent* InBuild)
{
	Build = InBuild;
	RebuildList();
}

void UQRBuildPieceSelectorWidget::RebuildList()
{
	if (!List) return;
	List->ClearChildren();
	if (!Build || !Build->PieceCatalog) return;

	for (const TPair<FName, uint8*>& Pair : Build->PieceCatalog->GetRowMap())
	{
		const FQRBuildPieceRow* Row = reinterpret_cast<const FQRBuildPieceRow*>(Pair.Value);
		if (!Row) continue;
		const FString Display = Row->DisplayName.IsEmpty()
			? Pair.Key.ToString()
			: Row->DisplayName.ToString();

		UQRBuildPieceButton* Btn = WidgetTree->ConstructWidget<UQRBuildPieceButton>(UQRBuildPieceButton::StaticClass());
		Btn->PieceId      = Pair.Key;
		Btn->TargetBuild  = Build;
		Btn->OwningWidget = this;

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(FText::FromString(Display));
		Label->SetJustification(ETextJustify::Left);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Btn->AddChild(Label);

		List->AddChild(Btn);
	}
}

void UQRBuildPieceSelectorWidget::HandleClose()
{
	QRUISound::PlayClick(this);
	RemoveFromParent();
}
