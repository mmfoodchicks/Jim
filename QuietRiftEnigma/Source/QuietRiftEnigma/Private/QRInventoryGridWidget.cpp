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
	OnHovered.AddDynamic(this, &UQRInventoryItemButton::HandleHovered);
	OnUnhovered.AddDynamic(this, &UQRInventoryItemButton::HandleUnhovered);
}

void UQRInventoryItemButton::HandleClicked()
{
	if (OwnerWidget.IsValid() && Item)
	{
		OwnerWidget->HandleItemClicked(Item);
	}
}

void UQRInventoryItemButton::HandleHovered()
{
	if (OwnerWidget.IsValid()) OwnerWidget->HandleItemHovered(Item);
}

void UQRInventoryItemButton::HandleUnhovered()
{
	if (OwnerWidget.IsValid()) OwnerWidget->HandleItemUnhovered(Item);
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
				== EQRInventoryResult::Success);
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

// ─── Right-click + context menu ───────────────────────────────────

void UQRInventoryGridWidget::HandleItemHovered(UQRItemInstance* Item)
{
	HoveredItem = Item;
}

void UQRInventoryGridWidget::HandleItemUnhovered(UQRItemInstance* Item)
{
	if (HoveredItem == Item) HoveredItem = nullptr;
}

bool UQRInventoryGridWidget::CanEquipItem(UQRItemInstance* Item) const
{
	if (!Item || !Item->Definition) return false;
	const FString Id = Item->Definition->ItemId.ToString().ToUpper();
	switch (Item->Definition->Category)
	{
	case EQRItemCategory::Weapon:    return true;   // hand slot
	case EQRItemCategory::Clothing:
		return Id.Contains(TEXT("HELM"))  ||
		       Id.Contains(TEXT("CHEST")) ||
		       Id.Contains(TEXT("LEGS"));
	case EQRItemCategory::ChestRig:  return true;
	case EQRItemCategory::Backpack:  return true;
	default:                         return false;
	}
}

bool UQRInventoryGridWidget::IsItemEquipped(UQRItemInstance* Item) const
{
	if (!Item || !Inventory) return false;
	if (Inventory->HandSlot                       == Item) return true;
	if (Inventory->EquippedHelm                   == Item) return true;
	if (Inventory->EquippedChestArmour            == Item) return true;
	if (Inventory->EquippedLegsArmour             == Item) return true;
	if (Inventory->EquippedChestRig               == Item) return true;
	if (Inventory->EquippedBackpack               == Item) return true;
	return false;
}

FReply UQRInventoryGridWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (HoveredItem)
		{
			OpenContextMenu(HoveredItem, InMouseEvent.GetScreenSpacePosition());
		}
		else
		{
			CloseContextMenu();
		}
		return FReply::Handled();
	}
	// Left clicks on dead space close any open menu.
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		CloseContextMenu();
		HideInspectPopup();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

static UTextBlock* _MakeMenuLabel(UWidgetTree* Tree, const FString& Text)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Text));
	T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = T->GetFont();
	Font.Size = 12;
	T->SetFont(Font);
	return T;
}

void UQRInventoryGridWidget::OpenContextMenu(UQRItemInstance* Item, FVector2D ScreenPos)
{
	CloseContextMenu();
	if (!Item) return;

	ContextTarget = Item;

	// Lazy-create the menu panel parented to the root canvas.
	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root) return;

	ContextMenu = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	UCanvasPanelSlot* RootSlot = Root->AddChildToCanvas(ContextMenu);
	if (RootSlot)
	{
		// Convert screen pos to local. With the root canvas anchored to
		// fullscreen and 0 offset, screen px == local px (close enough).
		RootSlot->SetAnchors(FAnchors(0, 0));
		RootSlot->SetAutoSize(true);
		RootSlot->SetPosition(ScreenPos);
	}

	// Solid background border.
	UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bg->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.10f, 0.95f));
	UCanvasPanelSlot* BgSlot = ContextMenu->AddChildToCanvas(Bg);
	if (BgSlot)
	{
		BgSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		BgSlot->SetOffsets(FMargin(0));
	}

	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Bg->SetContent(List);

	// Header: item id, dim.
	{
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Header->SetText(FText::FromString(Item->Definition ? Item->Definition->ItemId.ToString() : TEXT("?")));
		Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
		FSlateFontInfo Font = Header->GetFont();
		Font.Size = 11;
		Header->SetFont(Font);
		UVerticalBoxSlot* HS = List->AddChildToVerticalBox(Header);
		if (HS) HS->SetPadding(FMargin(8, 6, 8, 8));
	}

	// Helper to add one menu row.
	auto AddRow = [&](const FString& Label, void (UQRInventoryGridWidget::*Fn)())
	{
		UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		// Bind via a non-dynamic lambda is messy here; use a static map
		// to UFUNCTIONs by binding directly:
		if (Label == TEXT("Equip"))
		{
			B->OnClicked.AddDynamic(this, &UQRInventoryGridWidget::ContextActionEquip);
		}
		else if (Label == TEXT("Remove"))
		{
			B->OnClicked.AddDynamic(this, &UQRInventoryGridWidget::ContextActionUnequip);
		}
		else if (Label == TEXT("Destroy"))
		{
			B->OnClicked.AddDynamic(this, &UQRInventoryGridWidget::ContextActionDestroy);
		}
		else if (Label == TEXT("Inspect"))
		{
			B->OnClicked.AddDynamic(this, &UQRInventoryGridWidget::ContextActionInspect);
		}
		else
		{
			B->OnClicked.AddDynamic(this, &UQRInventoryGridWidget::ContextActionClose);
		}
		B->SetContent(_MakeMenuLabel(WidgetTree, Label));
		UVerticalBoxSlot* S = List->AddChildToVerticalBox(B);
		if (S) S->SetPadding(FMargin(2));
	};

	// Conditional rows.
	if (CanEquipItem(Item) && !IsItemEquipped(Item)) AddRow(TEXT("Equip"),   nullptr);
	if (IsItemEquipped(Item))                         AddRow(TEXT("Remove"),  nullptr);
	AddRow(TEXT("Inspect"), nullptr);
	AddRow(TEXT("Destroy"), nullptr);
	AddRow(TEXT("Cancel"),  nullptr);
}

void UQRInventoryGridWidget::CloseContextMenu()
{
	if (ContextMenu)
	{
		ContextMenu->RemoveFromParent();
		ContextMenu = nullptr;
	}
	ContextTarget = nullptr;
}

void UQRInventoryGridWidget::ContextActionEquip()
{
	if (!Inventory || !ContextTarget || !ContextTarget->Definition) { CloseContextMenu(); return; }
	UQRItemInstance* Item = ContextTarget;
	const EQRItemCategory Cat = Item->Definition->Category;
	bool bOk = false;
	if      (Cat == EQRItemCategory::Clothing)  bOk = Inventory->TryEquipArmour(Item);
	else if (Cat == EQRItemCategory::ChestRig
	      || Cat == EQRItemCategory::Backpack)  bOk = (Inventory->TryEquipContainer(Item) == EQRInventoryResult::Success);
	else if (Cat == EQRItemCategory::Weapon)    bOk = Inventory->TryEquipToHandSlot(Item);
	CloseContextMenu();
	Rebuild();
	(void)bOk;
}

void UQRInventoryGridWidget::ContextActionUnequip()
{
	if (!Inventory || !ContextTarget) { CloseContextMenu(); return; }
	UQRItemInstance* Item = ContextTarget;
	UQRItemInstance* Removed = nullptr;
	if      (Inventory->EquippedHelm        == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Helm,  Removed);
	else if (Inventory->EquippedChestArmour == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Chest, Removed);
	else if (Inventory->EquippedLegsArmour  == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Legs,  Removed);
	else if (Inventory->EquippedChestRig    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::ChestRig, Removed);
	else if (Inventory->EquippedBackpack    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::Backpack, Removed);
	else if (Inventory->HandSlot            == Item) Inventory->ClearHandSlot();
	CloseContextMenu();
	Rebuild();
}

void UQRInventoryGridWidget::ContextActionDestroy()
{
	if (!Inventory || !ContextTarget) { CloseContextMenu(); return; }
	UQRItemInstance* Item = ContextTarget;
	// Unequip from any slot first so the dangling ptr can be cleared.
	UQRItemInstance* Dummy = nullptr;
	if      (Inventory->EquippedHelm        == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Helm,  Dummy);
	else if (Inventory->EquippedChestArmour == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Chest, Dummy);
	else if (Inventory->EquippedLegsArmour  == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Legs,  Dummy);
	else if (Inventory->EquippedChestRig    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::ChestRig, Dummy);
	else if (Inventory->EquippedBackpack    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::Backpack, Dummy);
	else if (Inventory->HandSlot            == Item) Inventory->ClearHandSlot();
	Inventory->Items.Remove(Item);
	if (HoveredItem == Item) HoveredItem = nullptr;
	CloseContextMenu();
	Rebuild();
}

void UQRInventoryGridWidget::ContextActionInspect()
{
	if (ContextTarget) ShowInspectPopup(ContextTarget);
	CloseContextMenu();
}

void UQRInventoryGridWidget::ContextActionClose()
{
	CloseContextMenu();
}

void UQRInventoryGridWidget::ShowInspectPopup(UQRItemInstance* Item)
{
	HideInspectPopup();
	if (!Item || !Item->Definition) return;

	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root) return;
	InspectPopup = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	UCanvasPanelSlot* RS = Root->AddChildToCanvas(InspectPopup);
	if (RS) { RS->SetAnchors(FAnchors(0.5f, 0.5f)); RS->SetAlignment(FVector2D(0.5f, 0.5f)); RS->SetAutoSize(true); }

	UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Bg->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.98f));
	UCanvasPanelSlot* BgS = InspectPopup->AddChildToCanvas(Bg);
	if (BgS) { BgS->SetAnchors(FAnchors(0, 0, 1, 1)); BgS->SetOffsets(FMargin(0)); }

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Bg->SetContent(Col);

	const UQRItemDefinition* Def = Item->Definition;
	auto Field = [&](const FString& Label, const FString& Value)
	{
		UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"), *Label, *Value)));
		T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo F = T->GetFont(); F.Size = 12; T->SetFont(F);
		UVerticalBoxSlot* S = Col->AddChildToVerticalBox(T);
		if (S) S->SetPadding(FMargin(14, 4));
	};

	Field(TEXT("Id"),       Def->ItemId.ToString());
	Field(TEXT("Name"),     Def->DisplayName.ToString());
	Field(TEXT("Category"), FString::FromInt((int32)Def->Category));
	Field(TEXT("Mass"),     FString::Printf(TEXT("%.2f kg"), Def->MassKg));
	Field(TEXT("Volume"),   FString::Printf(TEXT("%.2f L"),  Def->VolumeLiters));
	Field(TEXT("Stack"),    FString::FromInt(Def->MaxStackSize));
	if (Def->MaxDurability > 0.0f)
		Field(TEXT("Durability"), FString::Printf(TEXT("%.0f"), Def->MaxDurability));
	if (!Def->Description.IsEmpty())
		Field(TEXT("Description"), Def->Description.ToString());

	UButton* Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Close->OnClicked.AddDynamic(this, &UQRInventoryGridWidget::InspectActionClose);
	Close->SetContent(_MakeMenuLabel(WidgetTree, TEXT("Close")));
	UVerticalBoxSlot* CS = Col->AddChildToVerticalBox(Close);
	if (CS) CS->SetPadding(FMargin(14, 10, 14, 14));
}

void UQRInventoryGridWidget::HideInspectPopup()
{
	if (InspectPopup)
	{
		InspectPopup->RemoveFromParent();
		InspectPopup = nullptr;
	}
}

void UQRInventoryGridWidget::InspectActionClose()
{
	HideInspectPopup();
}
