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

UQRInventoryEquipButton::UQRInventoryEquipButton()
{
	OnClicked.AddDynamic(this, &UQRInventoryEquipButton::HandleClicked);
}

void UQRInventoryEquipButton::HandleClicked()
{
	if (OwnerWidget.IsValid())
	{
		OwnerWidget->HandleEquipSlotClicked(KindIndex);
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

		// Equip strip: 5 square slots laid out left-to-right above the grids.
		EquipStrip = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBoxSlot* StripSlot = Column->AddChildToVerticalBox(EquipStrip);
		if (StripSlot) StripSlot->SetPadding(FMargin(0, 8, 0, 8));

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
	RebuildEquipStrip();
	RebuildKind(BodyGrid,     EQRContainerKind::Body);
	RebuildKind(ChestGrid,    EQRContainerKind::ChestRig);
	RebuildKind(BackpackGrid, EQRContainerKind::Backpack);
	// Hide container grids until the user double-clicks the slot.
	if (ChestGrid)    ChestGrid->SetVisibility(
		bShowChestGrid ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (BackpackGrid) BackpackGrid->SetVisibility(
		bShowBackpackGrid ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RefreshHeader();
}

void UQRInventoryGridWidget::RebuildEquipStrip()
{
	if (!EquipStrip) return;
	EquipStrip->ClearChildren();

	// Five slots. KindIndex: 1=Helm 2=ChestArm 3=LegsArm 4=Rig 5=Backpack.
	struct FSlotInfo { const TCHAR* Label; int32 Kind; UQRItemInstance* Item; };
	const FSlotInfo Slots[5] = {
		{ TEXT("Helm"),     1, Inventory ? Inventory->GetEquippedArmour(EQRArmourSlot::Helm)  : nullptr },
		{ TEXT("Chest"),    2, Inventory ? Inventory->GetEquippedArmour(EQRArmourSlot::Chest) : nullptr },
		{ TEXT("Legs"),     3, Inventory ? Inventory->GetEquippedArmour(EQRArmourSlot::Legs)  : nullptr },
		{ TEXT("Rig"),      4, Inventory ? Inventory->EquippedChestRig.Get()                  : nullptr },
		{ TEXT("Backpack"), 5, Inventory ? Inventory->EquippedBackpack.Get()                  : nullptr },
	};

	for (const FSlotInfo& S : Slots)
	{
		UQRInventoryEquipButton* Btn = WidgetTree->ConstructWidget<UQRInventoryEquipButton>(UQRInventoryEquipButton::StaticClass());
		Btn->OwnerWidget = this;
		Btn->KindIndex = S.Kind;
		FButtonStyle Style = Btn->GetStyle();
		const FLinearColor Fill = S.Item
			? FLinearColor(0.20f, 0.45f, 0.35f, 1.0f)   // occupied
			: FLinearColor(0.12f, 0.12f, 0.14f, 1.0f);   // empty
		Style.Normal.TintColor   = FSlateColor(Fill);
		Style.Hovered.TintColor  = FSlateColor(Fill * 1.25f);
		Style.Pressed.TintColor  = FSlateColor(Fill * 0.85f);
		Btn->SetStyle(Style);

		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		const FString Display = S.Item && S.Item->Definition
			? FString::Printf(TEXT("%s\n%s"), S.Label, *S.Item->Definition->ItemId.ToString())
			: FString(S.Label);
		Lbl->SetText(FText::FromString(Display));
		Lbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		{
			FSlateFontInfo Font = Lbl->GetFont();
			Font.Size = 10;
			Lbl->SetFont(Font);
		}
		Btn->SetContent(Lbl);

		UHorizontalBoxSlot* BoxSlot = EquipStrip->AddChildToHorizontalBox(Btn);
		if (BoxSlot)
		{
			BoxSlot->SetPadding(FMargin(4));
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
}

void UQRInventoryGridWidget::HandleEquipSlotClicked(int32 KindIndex)
{
	if (!Inventory) return;

	// Detect a double-click for container slots (Rig=4, Backpack=5) to
	// toggle the corresponding grid visibility. Single-click on any slot
	// equips-from-grab or unequips back to the body grid.
	const double Now = FPlatformTime::Seconds();
	const EEquipKind Kind = static_cast<EEquipKind>(KindIndex);
	const bool bDouble = (LastClickedEquip == Kind) &&
	                     (Now - LastClickedTime < DoubleClickWindowSec);
	LastClickedEquip = Kind;
	LastClickedTime  = Now;

	if (bDouble && (Kind == EEquipKind::Rig || Kind == EEquipKind::Backpack))
	{
		if (Kind == EEquipKind::Rig)      bShowChestGrid    = !bShowChestGrid;
		else                              bShowBackpackGrid = !bShowBackpackGrid;
		Rebuild();
		return;
	}

	// Single-click: equip the grabbed item (if it matches this slot), or
	// unequip whatever is in the slot back to the body grid.
	if (GrabbedItem && GrabbedItem->Definition)
	{
		bool bEquipped = false;
		switch (Kind)
		{
		case EEquipKind::Helm:
		case EEquipKind::Chest:
		case EEquipKind::Legs:
			bEquipped = Inventory->TryEquipArmour(GrabbedItem);
			break;
		case EEquipKind::Rig:
		case EEquipKind::Backpack:
			bEquipped = (Inventory->TryEquipContainer(GrabbedItem)
				== EQRInventoryResult::Ok);
			break;
		default: break;
		}
		if (bEquipped)
		{
			GrabbedItem = nullptr;
			Rebuild();
			return;
		}
	}

	// Nothing grabbed -- unequip and dump back into the body grid.
	UQRItemInstance* Removed = nullptr;
	switch (Kind)
	{
	case EEquipKind::Helm:     Inventory->TryUnequipArmour(EQRArmourSlot::Helm, Removed); break;
	case EEquipKind::Chest:    Inventory->TryUnequipArmour(EQRArmourSlot::Chest, Removed); break;
	case EEquipKind::Legs:     Inventory->TryUnequipArmour(EQRArmourSlot::Legs, Removed); break;
	case EEquipKind::Rig:      Inventory->TryUnequipContainer(EQRContainerSlotType::ChestRig, Removed); break;
	case EEquipKind::Backpack: Inventory->TryUnequipContainer(EQRContainerSlotType::Backpack, Removed); break;
	default: break;
	}
	Rebuild();
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
