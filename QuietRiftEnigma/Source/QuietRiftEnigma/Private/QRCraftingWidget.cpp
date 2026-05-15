#include "QRCraftingWidget.h"
#include "QRCraftingBench.h"
#include "QRCraftingComponent.h"
#include "QRRecipeDefinition.h"
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
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"

// ─── Inline recipe button helper ─────────────────────────────────────

UQRRecipeQueueButton::UQRRecipeQueueButton()
{
	OnClicked.AddDynamic(this, &UQRRecipeQueueButton::HandleClicked);
}

void UQRRecipeQueueButton::HandleClicked()
{
	QRUISound::PlayConfirm(this);
	if (TargetComp.IsValid() && !RecipeId.IsNone())
	{
		TargetComp->QueueRecipe(RecipeId);
	}
}

// ─── Main widget ─────────────────────────────────────────────────────

UQRCraftingWidget::UQRCraftingWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRCraftingWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Panel->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.06f, 0.92f));
		Panel->SetPadding(FMargin(20.0f));
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		if (PanelSlot)
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(680.0f, 540.0f));
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Column);

		// Title.
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TitleText->SetText(FText::FromString(TEXT("Crafting Bench")));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = TitleText->GetFont();
			Font.Size = 26;
			TitleText->SetFont(Font);
		}
		UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText);
		if (TitleSlot) TitleSlot->SetPadding(FMargin(0, 0, 0, 12));

		// Current-task header line.
		CurrentTaskText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CurrentTaskText->SetText(FText::FromString(TEXT("Idle")));
		CurrentTaskText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
		Column->AddChildToVerticalBox(CurrentTaskText);

		CurrentTaskBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		CurrentTaskBar->SetFillColorAndOpacity(FLinearColor(0.30f, 0.85f, 0.45f, 1.0f));
		CurrentTaskBar->SetPercent(0.0f);
		CurrentTaskBar->SetMinimumDesiredSize(FVector2D(640.0f, 14.0f));
		UVerticalBoxSlot* BarSlot = Column->AddChildToVerticalBox(CurrentTaskBar);
		if (BarSlot) BarSlot->SetPadding(FMargin(0, 4, 0, 18));

		// Recipe-list section title.
		UTextBlock* ListTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ListTitle->SetText(FText::FromString(TEXT("Available Recipes")));
		ListTitle->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = ListTitle->GetFont();
			Font.Size = 16;
			ListTitle->SetFont(Font);
		}
		Column->AddChildToVerticalBox(ListTitle);

		RecipeList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(RecipeList);
		if (ListSlot)
		{
			ListSlot->SetPadding(FMargin(0, 6, 0, 12));
			ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}

		// Queue status + action buttons.
		QueueCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		QueueCountText->SetText(FText::FromString(TEXT("Queued: 0")));
		QueueCountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
		Column->AddChildToVerticalBox(QueueCountText);

		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		auto MakeBtn = [&](const FString& Label) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			UTextBlock* TB = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			TB->SetText(FText::FromString(Label));
			TB->SetJustification(ETextJustify::Center);
			TB->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Btn->AddChild(TB);
			UHorizontalBoxSlot* SS = ButtonRow->AddChildToHorizontalBox(Btn);
			if (SS)
			{
				SS->SetPadding(FMargin(4));
				SS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			return Btn;
		};
		CancelButton = MakeBtn(TEXT("Cancel Current"));
		ClearButton  = MakeBtn(TEXT("Clear Queue"));
		CloseButton  = MakeBtn(TEXT("Close"));

		if (CancelButton) CancelButton->OnClicked.AddDynamic(this, &UQRCraftingWidget::HandleCancel);
		if (ClearButton)  ClearButton->OnClicked.AddDynamic(this,  &UQRCraftingWidget::HandleClear);
		if (CloseButton)  CloseButton->OnClicked.AddDynamic(this,  &UQRCraftingWidget::HandleClose);

		UVerticalBoxSlot* BtnRowSlot = Column->AddChildToVerticalBox(ButtonRow);
		if (BtnRowSlot) BtnRowSlot->SetPadding(FMargin(0, 12, 0, 0));
	}
	return Super::RebuildWidget();
}

void UQRCraftingWidget::Bind(AQRCraftingBench* InBench)
{
	Bench    = InBench;
	Crafting = Bench ? Bench->Crafting : nullptr;

	if (TitleText && Bench)
	{
		TitleText->SetText(Bench->DisplayName);
	}
	RebuildRecipeList();
	RefreshDynamic();
}

void UQRCraftingWidget::RebuildRecipeList()
{
	if (!RecipeList) return;
	RecipeList->ClearChildren();

	if (!Crafting || !Crafting->RecipeTable) return;

	for (const TPair<FName, uint8*>& Pair : Crafting->RecipeTable->GetRowMap())
	{
		const FQRRecipeTableRow* Row = reinterpret_cast<const FQRRecipeTableRow*>(Pair.Value);
		if (!Row) continue;
		const FString Display = Row->DisplayName.IsEmpty()
			? Pair.Key.ToString()
			: Row->DisplayName.ToString();
		MakeQueueRow(Row->RecipeId.IsNone() ? Pair.Key : Row->RecipeId, Display);
	}
}

UButton* UQRCraftingWidget::MakeQueueRow(const FName& RecipeId, const FString& DisplayName)
{
	if (!RecipeList || !WidgetTree) return nullptr;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Name->SetText(FText::FromString(DisplayName));
	Name->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name);
	if (NameSlot)
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(4));
	}

	UQRRecipeQueueButton* Btn = WidgetTree->ConstructWidget<UQRRecipeQueueButton>(UQRRecipeQueueButton::StaticClass());
	Btn->RecipeId   = RecipeId;
	Btn->TargetComp = Crafting;
	UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BtnLabel->SetText(FText::FromString(TEXT("Queue")));
	BtnLabel->SetJustification(ETextJustify::Center);
	BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Btn->AddChild(BtnLabel);
	UHorizontalBoxSlot* BtnSlot = Row->AddChildToHorizontalBox(Btn);
	if (BtnSlot)
	{
		BtnSlot->SetVerticalAlignment(VAlign_Center);
		BtnSlot->SetPadding(FMargin(4));
	}

	RecipeList->AddChild(Row);
	return Btn;
}

void UQRCraftingWidget::NativeTick(const FGeometry& InGeometry, float DeltaTime)
{
	Super::NativeTick(InGeometry, DeltaTime);
	RefreshAccum += DeltaTime;
	if (RefreshAccum >= 0.25f)
	{
		RefreshAccum = 0.0f;
		RefreshDynamic();
	}
}

void UQRCraftingWidget::RefreshDynamic()
{
	if (!Crafting) return;

	const float Progress = Crafting->GetCurrentProgress01();
	if (CurrentTaskBar) CurrentTaskBar->SetPercent(Progress);
	if (CurrentTaskText)
	{
		const FString Body = Crafting->IsCrafting()
			? FString::Printf(TEXT("Crafting %s — %d%%"),
				*Crafting->CurrentRecipeId.ToString(),
				FMath::RoundToInt(Progress * 100.0f))
			: TEXT("Idle");
		CurrentTaskText->SetText(FText::FromString(Body));
	}
	if (QueueCountText)
	{
		const FString Body = FString::Printf(TEXT("Queued: %d"), Crafting->RecipeQueue.Num());
		QueueCountText->SetText(FText::FromString(Body));
	}
}

void UQRCraftingWidget::HandleCancel()
{
	QRUISound::PlayDeny(this);
	if (Crafting) Crafting->CancelCurrentTask();
}

void UQRCraftingWidget::HandleClear()
{
	QRUISound::PlayDeny(this);
	if (Crafting) Crafting->ClearQueue();
}

void UQRCraftingWidget::HandleClose()
{
	QRUISound::PlayClick(this);
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
	RemoveFromParent();
}
