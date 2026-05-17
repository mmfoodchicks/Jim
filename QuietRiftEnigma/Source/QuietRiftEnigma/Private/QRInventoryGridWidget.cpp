#include "QRInventoryGridWidget.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRUISound.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

// ─── Click handlers for sub-buttons ──────────────────────────────────

UQRInventoryItemButton::UQRInventoryItemButton()
{
	OnClicked.AddDynamic(this, &UQRInventoryItemButton::HandleClicked);
}

void UQRInventoryItemButton::HandleClicked()
{
	if (OwnerWidget.IsValid() && Item)
	{
		OwnerWidget->HandleItemClicked(Item);
	}
}

UQRInventoryCellButton::UQRInventoryCellButton()
{
	OnClicked.AddDynamic(this, &UQRInventoryCellButton::HandleClicked);
}

void UQRInventoryCellButton::HandleClicked()
{
	if (OwnerWidget.IsValid())
	{
		OwnerWidget->HandleCellClicked(PackedKey());
	}
}

// ─── Main widget ─────────────────────────────────────────────────────

UQRInventoryGridWidget::UQRInventoryGridWidget(const FObjectInitializer& OI)
	: Super(OI)
{
	bIsFocusable = true;
}

TSharedRef<SWidget> UQRInventoryGridWidget::RebuildWidget()
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

		// Center column holds: header + three labeled grids stacked.
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UCanvasPanelSlot* ColSlot = Canvas->AddChildToCanvas(Column);
		if (ColSlot)
		{
			ColSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ColSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ColSlot->SetAutoSize(true);
		}

		// Header line.
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Title->SetText(FText::FromString(TEXT("Inventory")));
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = Title->GetFont();
			Font.Size = 24;
			Title->SetFont(Font);
		}
		UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
		if (TitleSlot) TitleSlot->SetPadding(FMargin(0, 0, 0, 6));

		WeightText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		WeightText->SetText(FText::FromString(TEXT("--")));
		WeightText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
		Column->AddChildToVerticalBox(WeightText);

		auto AddLabeledGrid = [&](const FString& Label, TObjectPtr<UCanvasPanel>& OutGrid)
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			L->SetText(FText::FromString(Label));
			L->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			{
				FSlateFontInfo Font = L->GetFont();
				Font.Size = 14;
				L->SetFont(Font);
			}
			UVerticalBoxSlot* LS = Column->AddChildToVerticalBox(L);
			if (LS) LS->SetPadding(FMargin(0, 10, 0, 4));

			OutGrid = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
			UVerticalBoxSlot* GS = Column->AddChildToVerticalBox(OutGrid);
			if (GS) GS->SetPadding(FMargin(0, 0, 0, 6));
		};
		AddLabeledGrid(TEXT("Body"),     BodyGrid);
		AddLabeledGrid(TEXT("Chest Rig"),ChestGrid);
		AddLabeledGrid(TEXT("Backpack"), BackpackGrid);

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		StatusText->SetText(FText::FromString(TEXT("Left-click to grab, R rotates, click cell to place, Esc to close")));
		StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)));
		{
			FSlateFontInfo Font = StatusText->GetFont();
			Font.Size = 11;
			StatusText->SetFont(Font);
		}
		UVerticalBoxSlot* SS = Column->AddChildToVerticalBox(StatusText);
		if (SS) SS->SetPadding(FMargin(0, 12, 0, 0));
	}
	return Super::RebuildWidget();
}

void UQRInventoryGridWidget::Bind(UQRInventoryComponent* InInventory)
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UQRInventoryGridWidget::HandleInventoryChanged);
	}
	Inventory = InInventory;
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UQRInventoryGridWidget::HandleInventoryChanged);
	}
	GrabbedItem = nullptr;
	bGrabbedRotation = false;
	Rebuild();
}

void UQRInventoryGridWidget::NativeDestruct()
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UQRInventoryGridWidget::HandleInventoryChanged);
		Inventory = nullptr;
	}
	Super::NativeDestruct();
}

void UQRInventoryGridWidget::HandleInventoryChanged()
{
	Rebuild();
}

void UQRInventoryGridWidget::Rebuild()
{
	if (!Inventory) return;
	RebuildKind(BodyGrid,     EQRContainerKind::Body);
	RebuildKind(ChestGrid,    EQRContainerKind::ChestRig);
	RebuildKind(BackpackGrid, EQRContainerKind::Backpack);
	RefreshHeader();
}

void UQRInventoryGridWidget::RebuildKind(UCanvasPanel* Panel, EQRContainerKind Kind)
{
	if (!Panel || !Inventory) return;
	Panel->ClearChildren();

	int32 W = 0, H = 0;
	if (!Inventory->GetGridSize(Kind, W, H) || W <= 0 || H <= 0) return;

	// Lock the panel to the grid pixel size. ResizeToContent() doesn't
	// exist on UCanvasPanel — the parent VerticalBoxSlot wraps it tight.
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
	{
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetSize(FVector2D(W * CellSize, H * CellSize));
	}

	AddCellGrid(Panel, Kind);

	// Layer placed items on top.
	for (UQRItemInstance* Item : Inventory->Items)
	{
		if (!Item || !Item->IsValid()) continue;
		if (Item->ContainerKind != Kind) continue;
		AddItem(Panel, Kind, Item);
	}
}

void UQRInventoryGridWidget::AddCellGrid(UCanvasPanel* Panel, EQRContainerKind Kind)
{
	int32 W = 0, H = 0;
	if (!Inventory || !Inventory->GetGridSize(Kind, W, H)) return;

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			UQRInventoryCellButton* CellBtn = WidgetTree->ConstructWidget<UQRInventoryCellButton>(UQRInventoryCellButton::StaticClass());
			CellBtn->OwnerWidget = this;
			CellBtn->Kind = Kind;
			CellBtn->X = X;
			CellBtn->Y = Y;

			// Light tint so cells are visible while empty.
			CellBtn->SetBackgroundColor(FLinearColor(0.10f, 0.11f, 0.13f, 0.85f));

			UCanvasPanelSlot* S = Panel->AddChildToCanvas(CellBtn);
			if (S)
			{
				S->SetPosition(FVector2D(X * CellSize, Y * CellSize));
				S->SetSize(FVector2D(CellSize - 1.0f, CellSize - 1.0f));
				S->SetZOrder(0);
			}
		}
	}
}

void UQRInventoryGridWidget::AddItem(UCanvasPanel* Panel, EQRContainerKind Kind, UQRItemInstance* Item)
{
	int32 W = 0, H = 0;
	Inventory->GetItemFootprint(Item, W, H);
	if (W <= 0 || H <= 0) return;

	UQRInventoryItemButton* ItemBtn = WidgetTree->ConstructWidget<UQRInventoryItemButton>(UQRInventoryItemButton::StaticClass());
	ItemBtn->OwnerWidget = this;
	ItemBtn->Item        = Item;

	// Tint based on whether this is the currently-grabbed item.
	const bool bIsGrabbed = (GrabbedItem == Item);
	const FLinearColor Base = bIsGrabbed
		? FLinearColor(0.85f, 0.70f, 0.20f, 0.95f)
		: FLinearColor(0.30f, 0.45f, 0.55f, 0.95f);
	ItemBtn->SetBackgroundColor(Base);

	// Inner label = item display name. Use Definition->ItemId as a
	// readable fallback when DisplayName isn't authored.
	if (Item->Definition)
	{
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const FString Name = Item->Definition->DisplayName.IsEmpty()
			? Item->Definition->ItemId.ToString()
			: Item->Definition->DisplayName.ToString();
		Label->SetText(FText::FromString(Name));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = Label->GetFont();
			Font.Size = 10;
			Label->SetFont(Font);
		}
		ItemBtn->AddChild(Label);
	}

	UCanvasPanelSlot* S = Panel->AddChildToCanvas(ItemBtn);
	if (S)
	{
		S->SetPosition(FVector2D(Item->GridX * CellSize, Item->GridY * CellSize));
		S->SetSize(FVector2D(W * CellSize - 2.0f, H * CellSize - 2.0f));
		S->SetZOrder(10);  // above cells
	}
}

void UQRInventoryGridWidget::RefreshHeader()
{
	if (!WeightText || !Inventory) return;
	const float Cur = Inventory->GetCurrentWeightKg();
	const float Max = Inventory->MaxCarryWeightKg;
	WeightText->SetText(FText::FromString(FString::Printf(
		TEXT("Weight: %.1f / %.1f kg"), Cur, Max)));
}

void UQRInventoryGridWidget::HandleItemClicked(UQRItemInstance* Item)
{
	if (!Item) return;
	QRUISound::PlayClick(this);

	if (GrabbedItem == Item)
	{
		// Click the grabbed item again to cancel.
		GrabbedItem = nullptr;
		bGrabbedRotation = false;
	}
	else
	{
		GrabbedItem = Item;
		bGrabbedRotation = Item->bRotated;
	}
	Rebuild();
}

void UQRInventoryGridWidget::HandleCellClicked(int32 PackedKey)
{
	if (!Inventory || !GrabbedItem)
	{
		// Click on cell with nothing grabbed = no-op.
		return;
	}

	EQRContainerKind Kind;
	int32 X = 0, Y = 0;
	UQRInventoryCellButton::Unpack(PackedKey, Kind, X, Y);

	const bool bOk = Inventory->TryMoveItem(GrabbedItem, Kind, X, Y, bGrabbedRotation);
	if (bOk)
	{
		QRUISound::PlayConfirm(this);
	}
	else
	{
		QRUISound::PlayDeny(this);
	}
	GrabbedItem = nullptr;
	bGrabbedRotation = false;
	Rebuild();
}

FReply UQRInventoryGridWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::R && GrabbedItem)
	{
		bGrabbedRotation = !bGrabbedRotation;
		QRUISound::PlayClick(this);
		// Re-render to update visual hint (no actual server change yet —
		// rotation is applied when the player commits the move).
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		if (GrabbedItem)
		{
			GrabbedItem = nullptr;
			bGrabbedRotation = false;
			Rebuild();
			return FReply::Handled();
		}
		RemoveFromParent();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
