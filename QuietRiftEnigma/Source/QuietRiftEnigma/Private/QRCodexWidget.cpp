#include "QRCodexWidget.h"
#include "QRCodexSubsystem.h"
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
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"


// ─── Category button ──────────────────────────────────────────────

UQRCodexCategoryButton::UQRCodexCategoryButton()
{
	OnClicked.AddDynamic(this, &UQRCodexCategoryButton::HandleClicked);
}

void UQRCodexCategoryButton::HandleClicked()
{
	QRUISound::PlayClick(this);
	if (Owner.IsValid()) Owner->SetCategory(Category);
}


// ─── Main widget ──────────────────────────────────────────────────

UQRCodexWidget::UQRCodexWidget(const FObjectInitializer& OI) : Super(OI)
{
	bIsFocusable = true;
}


TSharedRef<SWidget> UQRCodexWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		WidgetTree->RootWidget = Canvas;

		// Full-screen dim background.
		UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Dim->SetBrushColor(FLinearColor(0, 0, 0, 0.85f));
		UCanvasPanelSlot* DimSlot = Canvas->AddChildToCanvas(Dim);
		if (DimSlot)
		{
			DimSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			DimSlot->SetOffsets(FMargin(0));
		}

		// Inner panel.
		UHorizontalBox* Panel = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
		if (PanelSlot)
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(960.0f, 640.0f));
		}

		// Left — category column.
		CategoryColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBoxSlot* LeftSlot = Panel->AddChildToHorizontalBox(CategoryColumn);
		if (LeftSlot) LeftSlot->SetPadding(FMargin(24));

		// Right — entry list + header.
		UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBoxSlot* RightSlot = Panel->AddChildToHorizontalBox(Right);
		if (RightSlot)
		{
			RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			RightSlot->SetPadding(FMargin(24));
		}

		HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		HeaderText->SetText(FText::FromString(TEXT("Codex")));
		HeaderText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo F = HeaderText->GetFont(); F.Size = 28; HeaderText->SetFont(F);
		}
		Right->AddChildToVerticalBox(HeaderText);

		ProgressText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ProgressText->SetText(FText::FromString(TEXT("0 / 0 entries")));
		ProgressText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
		{
			FSlateFontInfo F = ProgressText->GetFont(); F.Size = 14; ProgressText->SetFont(F);
		}
		UVerticalBoxSlot* ProgSlot = Right->AddChildToVerticalBox(ProgressText);
		if (ProgSlot) ProgSlot->SetPadding(FMargin(0, 6, 0, 16));

		EntryList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBoxSlot* ListSlot = Right->AddChildToVerticalBox(EntryList);
		if (ListSlot) ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Close button at the bottom of the right column.
		UButton* CloseBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		UTextBlock* CT = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		CT->SetText(FText::FromString(TEXT("Close")));
		CT->SetJustification(ETextJustify::Center);
		CT->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CloseBtn->AddChild(CT);
		UVerticalBoxSlot* CloseSlot = Right->AddChildToVerticalBox(CloseBtn);
		if (CloseSlot) CloseSlot->SetPadding(FMargin(0, 16, 0, 0));
		CloseBtn->OnClicked.AddDynamic(this, &UQRCodexWidget::HandleClose);
	}
	return Super::RebuildWidget();
}


void UQRCodexWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* W = GetWorld())
	{
		Codex = W->GetSubsystem<UQRCodexSubsystem>();
	}
	RefreshCategories();
	RefreshEntryList();
}


void UQRCodexWidget::NativeDestruct()
{
	Codex = nullptr;
	Super::NativeDestruct();
}


void UQRCodexWidget::SetCategory(FName Category)
{
	CurrentCategory = Category;
	RefreshEntryList();
}


void UQRCodexWidget::RefreshCategories()
{
	if (!CategoryColumn || !Codex || !WidgetTree) return;
	CategoryColumn->ClearChildren();

	static const TArray<FName> CanonicalOrder = {
		TEXT("Fauna"), TEXT("Flora"), TEXT("Item"),
		TEXT("POI"),   TEXT("Biome"), TEXT("Remnant"),
	};

	TArray<FName> Known = Codex->GetKnownCategories();
	// Merge — show canonical order first, append unknown extras.
	TArray<FName> All;
	for (const FName& C : CanonicalOrder) All.Add(C);
	for (const FName& C : Known) if (!All.Contains(C)) All.Add(C);

	for (const FName& Cat : All)
	{
		UQRCodexCategoryButton* Btn = WidgetTree->ConstructWidget<UQRCodexCategoryButton>(UQRCodexCategoryButton::StaticClass());
		Btn->Category = Cat;
		Btn->Owner    = this;

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const int32 KnownCount = Codex->GetEntriesByCategory(Cat).Num();
		const FString Body = FString::Printf(TEXT("%s  (%d)"), *Cat.ToString(), KnownCount);
		Label->SetText(FText::FromString(Body));
		Label->SetJustification(ETextJustify::Left);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Btn->AddChild(Label);

		UVerticalBoxSlot* Slot = CategoryColumn->AddChildToVerticalBox(Btn);
		if (Slot) Slot->SetPadding(FMargin(0, 4));
	}
}


void UQRCodexWidget::RefreshEntryList()
{
	if (!EntryList || !Codex || !WidgetTree) return;
	EntryList->ClearChildren();

	if (HeaderText) HeaderText->SetText(FText::FromString(
		FString::Printf(TEXT("Codex — %s"), *CurrentCategory.ToString())));

	const TArray<FQRCodexEntry> Entries = Codex->GetEntriesByCategory(CurrentCategory);
	if (ProgressText)
	{
		const int32 Researched = Codex->CountByCategoryAndState(CurrentCategory, EQRCodexDiscoveryState::Researched);
		ProgressText->SetText(FText::FromString(FString::Printf(
			TEXT("%d entries · %d fully researched"), Entries.Num(), Researched)));
	}

	for (const FQRCodexEntry& E : Entries)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(E.DisplayName.IsEmpty() ? FText::FromName(E.Id) : E.DisplayName);
		Name->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name);
		if (NameSlot) NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// State badge.
		FString StateStr;
		FLinearColor StateColor = FLinearColor::Gray;
		switch (E.State)
		{
		case EQRCodexDiscoveryState::Unknown:    StateStr = TEXT("UNKNOWN");    StateColor = FLinearColor(0.5f, 0.5f, 0.5f); break;
		case EQRCodexDiscoveryState::Observed:   StateStr = TEXT("OBSERVED");   StateColor = FLinearColor(0.85f, 0.85f, 0.30f); break;
		case EQRCodexDiscoveryState::Sampled:    StateStr = TEXT("SAMPLED");    StateColor = FLinearColor(0.30f, 0.75f, 0.85f); break;
		case EQRCodexDiscoveryState::Researched: StateStr = TEXT("RESEARCHED"); StateColor = FLinearColor(0.30f, 0.85f, 0.40f); break;
		}
		UTextBlock* Badge = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Badge->SetText(FText::FromString(StateStr));
		Badge->SetColorAndOpacity(FSlateColor(StateColor));
		Row->AddChildToHorizontalBox(Badge);

		EntryList->AddChild(Row);
	}
}


void UQRCodexWidget::HandleClose()
{
	QRUISound::PlayClick(this);
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
	RemoveFromParent();
}
