#include "QRVitalsHUDWidget.h"
#include "QRSurvivalComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"

namespace
{
	// Row ordering — used as the index into Bars / Labels arrays.
	enum EVitalIndex : int32
	{
		VI_Health   = 0,
		VI_Stamina  = 1,
		VI_Hunger   = 2,
		VI_Thirst   = 3,
		VI_Oxygen   = 4,
		VI_Count    = 5,
	};
}

UQRVitalsHUDWidget::UQRVitalsHUDWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRVitalsHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Dark translucent backing panel groups the vitals into one block.
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.06f, 0.72f));
		Panel->SetPadding(FMargin(12.0f, 10.0f));

		Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Root);
		UCanvasPanelSlot* RootSlot = Canvas->AddChildToCanvas(Panel);
		if (RootSlot)
		{
			// Anchor bottom-left. Position the box ~24px from the screen edges.
			RootSlot->SetAnchors(FAnchors(0.0f, 1.0f));  // bottom-left
			RootSlot->SetAlignment(FVector2D(0.0f, 1.0f));
			RootSlot->SetPosition(FVector2D(24.0f, -24.0f));
			RootSlot->SetAutoSize(true);
		}

		Bars.SetNum(VI_Count);
		Labels.SetNum(VI_Count);

		MakeRow(VI_Health,  FText::FromString(TEXT("HP")),  FLinearColor(0.85f, 0.20f, 0.20f, 1.0f));
		MakeRow(VI_Stamina, FText::FromString(TEXT("STA")), FLinearColor(0.85f, 0.85f, 0.20f, 1.0f));
		MakeRow(VI_Hunger,  FText::FromString(TEXT("FOOD")),FLinearColor(0.85f, 0.55f, 0.20f, 1.0f));
		MakeRow(VI_Thirst,  FText::FromString(TEXT("H2O")), FLinearColor(0.30f, 0.65f, 0.95f, 1.0f));
		MakeRow(VI_Oxygen,  FText::FromString(TEXT("OX")),  FLinearColor(0.65f, 0.90f, 0.95f, 1.0f));
	}

	return Super::RebuildWidget();
}

UHorizontalBox* UQRVitalsHUDWidget::MakeRow(int32 Index, const FText& LabelText, FLinearColor FillColor)
{
	if (!Root || !WidgetTree) return nullptr;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	// Label cell — fixed-width box keeps every bar's left edge aligned.
	UTextBlock* LabelText2 = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText2->SetText(LabelText);
	LabelText2->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.85f, 0.90f, 1.0f)));
	{
		FSlateFontInfo Font = LabelText2->GetFont();
		Font.Size = 12;
		LabelText2->SetFont(Font);
	}
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(44.0f);
	LabelBox->SetContent(LabelText2);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelBox);
	if (LabelSlot)
	{
		LabelSlot->SetPadding(FMargin(0, 0, 8, 0));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Progress bar in a fixed-size box so every vital reads the same.
	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	Bar->SetFillColorAndOpacity(FillColor);
	Bar->SetPercent(1.0f);

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(190.0f);
	BarBox->SetHeightOverride(16.0f);
	BarBox->SetContent(Bar);

	UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarBox);
	if (BarSlot)
	{
		BarSlot->SetPadding(FMargin(0, 2, 8, 2));
		BarSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Numeric readout cell.
	UTextBlock* Numeric = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Numeric->SetText(FText::FromString(TEXT("100")));
	Numeric->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = Numeric->GetFont();
		Font.Size = 12;
		Numeric->SetFont(Font);
	}
	UHorizontalBoxSlot* NumericSlot = Row->AddChildToHorizontalBox(Numeric);
	if (NumericSlot)
	{
		NumericSlot->SetVerticalAlignment(VAlign_Center);
	}

	UVerticalBoxSlot* RowSlot = Root->AddChildToVerticalBox(Row);
	if (RowSlot) RowSlot->SetPadding(FMargin(0, 2));

	Bars[Index]   = Bar;
	Labels[Index] = Numeric;
	return Row;
}

void UQRVitalsHUDWidget::Bind(UQRSurvivalComponent* InSurvival)
{
	if (Survival == InSurvival) return;

	if (Survival)
	{
		Survival->OnHealthChanged.RemoveDynamic(this, &UQRVitalsHUDWidget::HandleHealthChanged);
	}
	Survival = InSurvival;
	if (Survival)
	{
		Survival->OnHealthChanged.AddDynamic(this, &UQRVitalsHUDWidget::HandleHealthChanged);
	}
	RefreshAll();
}

void UQRVitalsHUDWidget::NativeDestruct()
{
	if (Survival)
	{
		Survival->OnHealthChanged.RemoveDynamic(this, &UQRVitalsHUDWidget::HandleHealthChanged);
		Survival = nullptr;
	}
	Super::NativeDestruct();
}

void UQRVitalsHUDWidget::NativeTick(const FGeometry& InGeometry, float DeltaTime)
{
	Super::NativeTick(InGeometry, DeltaTime);

	// Refresh twice per second — hunger / thirst / fatigue / oxygen
	// drain slowly so we don't need every-frame updates.
	RefreshAccum += DeltaTime;
	if (RefreshAccum >= 0.5f)
	{
		RefreshAccum = 0.0f;
		RefreshAll();
	}
}

void UQRVitalsHUDWidget::HandleHealthChanged(float /*NewHealth*/)
{
	RefreshAll();
}

void UQRVitalsHUDWidget::RefreshAll()
{
	if (!Survival || Bars.Num() < VI_Count || Labels.Num() < VI_Count) return;

	auto SetRow = [&](int32 Idx, float Cur, float Max)
	{
		const float Pct = (Max > 0.0f) ? FMath::Clamp(Cur / Max, 0.0f, 1.0f) : 0.0f;
		if (Bars[Idx])   Bars[Idx]->SetPercent(Pct);
		if (Labels[Idx]) Labels[Idx]->SetText(FText::AsNumber(FMath::RoundToInt(Cur)));
	};

	SetRow(VI_Health,  Survival->Health,   Survival->MaxHealth);
	SetRow(VI_Stamina, Survival->Fatigue,  Survival->MaxFatigue);
	SetRow(VI_Hunger,  Survival->Hunger,   Survival->MaxHunger);
	SetRow(VI_Thirst,  Survival->Thirst,   Survival->MaxThirst);
	SetRow(VI_Oxygen,  Survival->Oxygen,   Survival->MaxOxygen);
}
