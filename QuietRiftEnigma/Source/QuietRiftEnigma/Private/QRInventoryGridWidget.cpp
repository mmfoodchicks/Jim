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
#include "Components/SizeBox.h"
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
	OnHovered.AddDynamic(this, &UQRInventoryEquipButton::HandleHovered);
	OnUnhovered.AddDynamic(this, &UQRInventoryEquipButton::HandleUnhovered);
}

void UQRInventoryEquipButton::HandleClicked()
{
	if (OwnerWidget.IsValid())
	{
		OwnerWidget->HandleEquipSlotClicked(KindIndex);
	}
}

void UQRInventoryEquipButton::HandleHovered()
{
	// Track the equipped item so right-clicking an occupied slot opens the
	// context menu against it.
	if (OwnerWidget.IsValid()) OwnerWidget->HandleItemHovered(SlotItem);
}

void UQRInventoryEquipButton::HandleUnhovered()
{
	if (OwnerWidget.IsValid()) OwnerWidget->HandleItemUnhovered(SlotItem);
}

UQRInventoryActionButton::UQRInventoryActionButton()
{
	OnClicked.AddDynamic(this, &UQRInventoryActionButton::HandleClicked);
}

void UQRInventoryActionButton::HandleClicked()
{
	if (OwnerWidget.IsValid())
	{
		OwnerWidget->HandleContextAction(ActionId);
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

		// Main row: paper-doll on the left, the three container grids stacked
		// on the right (Tarkov GEAR-tab layout).
		UHorizontalBox* MainRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(MainRow);
		if (RowSlot) RowSlot->SetPadding(FMargin(0, 8, 0, 8));

		// Left column: "Equipment" label + the paper-doll canvas.
		{
			UVerticalBox* DollCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBoxSlot* DollColSlot = MainRow->AddChildToHorizontalBox(DollCol);
			if (DollColSlot)
			{
				DollColSlot->SetPadding(FMargin(0, 0, 18, 0));
				DollColSlot->SetVerticalAlignment(VAlign_Top);
			}

			UTextBlock* DollLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			DollLabel->SetText(FText::FromString(TEXT("Equipment")));
			DollLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			{
				FSlateFontInfo Font = DollLabel->GetFont();
				Font.Size = 14;
				DollLabel->SetFont(Font);
			}
			UVerticalBoxSlot* DLS = DollCol->AddChildToVerticalBox(DollLabel);
			if (DLS) DLS->SetPadding(FMargin(0, 0, 0, 4));

			// Fixed-size canvas the silhouette + slots are positioned into. A
			// bare UCanvasPanel reports no desired size, so a SizeBox pins the
			// paper-doll footprint and the Border draws the backing panel.
			UBorder* DollFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			DollFrame->SetBrushColor(FLinearColor(0.06f, 0.07f, 0.09f, 0.85f));
			UVerticalBoxSlot* DFS = DollCol->AddChildToVerticalBox(DollFrame);
			if (DFS) DFS->SetPadding(FMargin(0));

			USizeBox* DollSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			DollSize->SetWidthOverride(PaperDollWidth);
			DollSize->SetHeightOverride(PaperDollHeight);
			DollFrame->SetContent(DollSize);

			PaperDoll = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
			DollSize->SetContent(PaperDoll);
		}

		// Right column: the three labeled grids stacked.
		{
			UVerticalBox* GridCol = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UHorizontalBoxSlot* GridColSlot = MainRow->AddChildToHorizontalBox(GridCol);
			if (GridColSlot) GridColSlot->SetVerticalAlignment(VAlign_Top);

			auto AddLabeledGrid = [&](const FString& Label, TObjectPtr<UCanvasPanel>& OutGrid,
				TObjectPtr<UVerticalBox>* OutWrapper)
			{
				// Wrap label + grid so a container's whole block collapses
				// together when it isn't equipped.
				UVerticalBox* Wrapper = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
				GridCol->AddChildToVerticalBox(Wrapper);
				if (OutWrapper) *OutWrapper = Wrapper;

				// Tarkov-style section header: dark strip, uppercase label,
				// slight letterspacing feel via padding.
				UBorder* HeaderStrip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
				HeaderStrip->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.085f, 0.95f));
				HeaderStrip->SetPadding(FMargin(8.0f, 3.0f));
				UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
				L->SetText(FText::FromString(Label.ToUpper()));
				L->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.76f, 0.68f, 1.0f)));
				{
					FSlateFontInfo Font = L->GetFont();
					Font.Size = 11;
					L->SetFont(Font);
				}
				HeaderStrip->SetContent(L);
				UVerticalBoxSlot* LS = Wrapper->AddChildToVerticalBox(HeaderStrip);
				if (LS) LS->SetPadding(FMargin(0, 10, 0, 2));

				OutGrid = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
				UVerticalBoxSlot* GS = Wrapper->AddChildToVerticalBox(OutGrid);
				if (GS) GS->SetPadding(FMargin(0, 0, 0, 6));
			};
			// Tarkov layout: bare body = POCKETS only (4x1). Rig/backpack
			// sections appear when their container is equipped.
			AddLabeledGrid(TEXT("Pockets"),   BodyGrid,     nullptr);
			AddLabeledGrid(TEXT("Chest Rig"), ChestGrid,    &ChestGridBox);
			AddLabeledGrid(TEXT("Backpack"),  BackpackGrid, &BackpackGridBox);
		}

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
	RebuildPaperDoll();
	RebuildKind(BodyGrid,     EQRContainerKind::Body);
	RebuildKind(ChestGrid,    EQRContainerKind::ChestRig);
	RebuildKind(BackpackGrid, EQRContainerKind::Backpack);

	// Tarkov behavior: an unequipped container contributes NO section to
	// the items panel (the GEAR slot on the paper-doll is its only UI).
	// Equipping the rig/backpack makes its grid section appear.
	const bool bRig  = Inventory->EquippedChestRig  != nullptr;
	const bool bPack = Inventory->EquippedBackpack  != nullptr;
	if (ChestGridBox)    ChestGridBox->SetVisibility(
		bRig  ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (BackpackGridBox) BackpackGridBox->SetVisibility(
		bPack ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	RefreshHeader();
}

void UQRInventoryGridWidget::RebuildPaperDoll()
{
	if (!PaperDoll || !Inventory) return;
	PaperDoll->ClearChildren();

	AddSilhouette();

	// Slot positions in PaperDoll-local coordinates. Layout mirrors Tarkov's
	// GEAR tab: head up top, body armour mid-chest, legs lower, rig + pack
	// on the flanks, both hands at the bottom. Sizes anchor to PaperDollSlot.
	const float CX  = PaperDollWidth  * 0.5f;
	const float Sz  = PaperDollSlot;
	const float Pad = Sz * 0.5f;

	UQRItemInstance* Helm  = Inventory->GetEquippedArmour(EQRArmourSlot::Helm);
	UQRItemInstance* Chest = Inventory->GetEquippedArmour(EQRArmourSlot::Chest);
	UQRItemInstance* Legs  = Inventory->GetEquippedArmour(EQRArmourSlot::Legs);
	UQRItemInstance* Rig   = Inventory->EquippedChestRig.Get();
	UQRItemInstance* Pack  = Inventory->EquippedBackpack.Get();
	UQRItemInstance* Hand  = Inventory->HandSlot.Get();
	UQRItemInstance* Off   = Inventory->OffhandSlot.Get();

	// Head, body, legs run down the vertical centerline.
	AddPaperDollSlot(TEXT("Helm"),  1, Helm,  CX - Pad,  10.0f);
	AddPaperDollSlot(TEXT("Chest"), 2, Chest, CX - Pad,  10.0f + Sz + 12.0f);
	AddPaperDollSlot(TEXT("Legs"),  3, Legs,  CX - Pad,  10.0f + (Sz + 12.0f) * 2.0f);

	// Rig sits over the right shoulder, backpack over the left -- matches
	// the Tarkov GEAR tab's rig-right / pack-left arrangement.
	AddPaperDollSlot(TEXT("Rig"),      4, Rig,  CX + Sz * 0.9f,  10.0f + Sz + 12.0f);
	AddPaperDollSlot(TEXT("Backpack"), 5, Pack, CX - Sz * 2.9f,  10.0f + Sz + 12.0f);

	// Primary + offhand at the bottom, centered.
	const float HandY = PaperDollHeight - Sz - 12.0f;
	AddPaperDollSlot(TEXT("Hand"),    6, Hand, CX - Sz - 6.0f, HandY);
	AddPaperDollSlot(TEXT("Offhand"), 7, Off,  CX + 6.0f,      HandY);
}

void UQRInventoryGridWidget::AddSilhouette()
{
	if (!PaperDoll) return;

	if (BodySilhouette)
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Img->SetBrushFromTexture(BodySilhouette, /*bMatchSize*/ false);
		Img->SetColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.90f, 0.35f));
		UCanvasPanelSlot* S = PaperDoll->AddChildToCanvas(Img);
		if (S)
		{
			S->SetAnchors(FAnchors(0, 0, 1, 1));
			S->SetOffsets(FMargin(0));
			S->SetZOrder(0);
		}
		return;
	}

	// Fallback: schematic humanoid drawn from three rounded Borders so the
	// paper-doll still reads as a body when no silhouette texture is set.
	auto AddShape = [&](float X, float Y, float W, float H, FLinearColor Tint)
	{
		UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(Tint);
		UCanvasPanelSlot* S = PaperDoll->AddChildToCanvas(B);
		if (S)
		{
			S->SetPosition(FVector2D(X, Y));
			S->SetSize(FVector2D(W, H));
			S->SetZOrder(0);
		}
	};

	const float CX = PaperDollWidth * 0.5f;
	const FLinearColor Skin(0.30f, 0.32f, 0.36f, 0.55f);

	// Head.
	AddShape(CX - 26, 14, 52, 56, Skin);
	// Torso.
	AddShape(CX - 60, 80, 120, 150, Skin);
	// Legs.
	AddShape(CX - 50, 235, 44, 200, Skin);
	AddShape(CX +  6, 235, 44, 200, Skin);
}

void UQRInventoryGridWidget::AddPaperDollSlot(const TCHAR* Label, int32 KindIndex,
	UQRItemInstance* Item, float X, float Y)
{
	if (!PaperDoll) return;

	UQRInventoryEquipButton* Btn = WidgetTree->ConstructWidget<UQRInventoryEquipButton>(UQRInventoryEquipButton::StaticClass());
	Btn->OwnerWidget = this;
	Btn->KindIndex = KindIndex;
	Btn->SlotItem = Item;

	FButtonStyle Style = Btn->GetStyle();
	const FLinearColor Fill = Item
		? FLinearColor(0.20f, 0.45f, 0.35f, 0.92f)
		: FLinearColor(0.10f, 0.11f, 0.14f, 0.82f);
	Style.Normal.TintColor   = FSlateColor(Fill);
	Style.Hovered.TintColor  = FSlateColor(Fill * 1.25f);
	Style.Pressed.TintColor  = FSlateColor(Fill * 0.85f);
	Btn->SetStyle(Style);

	UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	const FString Display = Item && Item->Definition
		? FString::Printf(TEXT("%s\n%s"), Label, *Item->Definition->ItemId.ToString())
		: FString(Label);
	Lbl->SetText(FText::FromString(Display));
	Lbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = Lbl->GetFont();
		Font.Size = 9;
		Lbl->SetFont(Font);
	}
	Btn->SetContent(Lbl);

	UCanvasPanelSlot* S = PaperDoll->AddChildToCanvas(Btn);
	if (S)
	{
		S->SetPosition(FVector2D(X, Y));
		// Rig + backpack icons are a bit wider, to read as containers.
		const bool bWide = (KindIndex == 4 || KindIndex == 5);
		S->SetSize(FVector2D(bWide ? PaperDollSlot * 1.4f : PaperDollSlot, PaperDollSlot));
		S->SetZOrder(5);
	}
}

void UQRInventoryGridWidget::HandleEquipSlotClicked(int32 KindIndex)
{
	if (!Inventory) return;
	const EEquipKind Kind = static_cast<EEquipKind>(KindIndex);

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
		case EEquipKind::Hand:
			bEquipped = Inventory->TryEquipToHandSlot(GrabbedItem);
			break;
		case EEquipKind::Offhand:
			bEquipped = Inventory->TryEquipToOffhand(GrabbedItem);
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

	// Nothing grabbed (or it didn't fit) -- unequip what's in the slot.
	UQRItemInstance* Removed = nullptr;
	switch (Kind)
	{
	case EEquipKind::Helm:     Inventory->TryUnequipArmour(EQRArmourSlot::Helm, Removed); break;
	case EEquipKind::Chest:    Inventory->TryUnequipArmour(EQRArmourSlot::Chest, Removed); break;
	case EEquipKind::Legs:     Inventory->TryUnequipArmour(EQRArmourSlot::Legs, Removed); break;
	case EEquipKind::Rig:      Inventory->TryUnequipContainer(EQRContainerSlotType::ChestRig, Removed); break;
	case EEquipKind::Backpack: Inventory->TryUnequipContainer(EQRContainerSlotType::Backpack, Removed); break;
	case EEquipKind::Hand:     Inventory->ClearHandSlot(); break;
	case EEquipKind::Offhand:  Inventory->ClearOffhand();  break;
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

			// Tarkov palette: near-black cell with a faint warm border read
			// (the 1px gap between cells against the panel acts as the
			// grid line).
			CellBtn->SetBackgroundColor(FLinearColor(0.055f, 0.058f, 0.052f, 0.95f));

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
	if (Inventory->OffhandSlot                    == Item) return true;
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
		// Local position within this widget -- robust against the widget not
		// sitting at screen 0,0 (the menu was placing wrong before).
		const FVector2D Local = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		if (HoveredItem)
		{
			OpenContextMenu(HoveredItem, Local);
		}
		else
		{
			CloseContextMenu();
		}
		return FReply::Handled();
	}
	// Outside-click close is handled by the full-screen backdrop button the
	// menu spawns; nothing to do here for LMB.
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

	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root) return;

	// Full-screen invisible backdrop that intercepts off-menu clicks. It's a
	// real UQRInventoryActionButton with ActionId=6 -> CloseContextMenu.
	{
		UQRInventoryActionButton* Backdrop = WidgetTree->ConstructWidget<UQRInventoryActionButton>(UQRInventoryActionButton::StaticClass());
		Backdrop->OwnerWidget = this;
		Backdrop->ActionId = 6;
		FButtonStyle BStyle = Backdrop->GetStyle();
		const FLinearColor Clear(0, 0, 0, 0.01f);   // hit-testable but invisible
		BStyle.Normal.TintColor  = FSlateColor(Clear);
		BStyle.Hovered.TintColor = FSlateColor(Clear);
		BStyle.Pressed.TintColor = FSlateColor(Clear);
		Backdrop->SetStyle(BStyle);
		UCanvasPanelSlot* BD = Root->AddChildToCanvas(Backdrop);
		if (BD)
		{
			BD->SetAnchors(FAnchors(0, 0, 1, 1));
			BD->SetOffsets(FMargin(0));
			BD->SetZOrder(100);   // above grids, below the menu itself
		}
		ContextBackdrop = Backdrop;
	}

	// Menu body: a Border that auto-sizes to its VerticalBox content. The old
	// version was an autosize UCanvasPanel wrapping a fill-anchored child --
	// fill anchors contribute zero to autosize, so the canvas collapsed to
	// 0x0 and every action button's hit-testing was dead. A plain Border
	// adopts its content's desired size correctly.
	ContextMenu = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ContextMenu->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.10f, 0.96f));
	ContextMenu->SetPadding(FMargin(2));

	UCanvasPanelSlot* RootSlot = Root->AddChildToCanvas(ContextMenu);
	if (RootSlot)
	{
		RootSlot->SetAnchors(FAnchors(0, 0));
		RootSlot->SetAutoSize(true);
		RootSlot->SetPosition(ScreenPos);
		RootSlot->SetZOrder(101);   // above the backdrop
	}

	UVerticalBox* List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ContextMenu->SetContent(List);

	// Header: item id, dim.
	{
		UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Header->SetText(FText::FromString(Item->Definition ? Item->Definition->ItemId.ToString() : TEXT("?")));
		Header->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
		FSlateFontInfo Font = Header->GetFont();
		Font.Size = 11;
		Header->SetFont(Font);
		UVerticalBoxSlot* HS = List->AddChildToVerticalBox(Header);
		if (HS) HS->SetPadding(FMargin(10, 6, 10, 8));
	}

	// Helper -- the proven button-subclass pattern (bind in the button's
	// own ctor). The previous fix had this but the autosize-collapse bug
	// killed the hit area; now that ContextMenu is a Border the buttons
	// get real geometry.
	auto AddRow = [&](const FString& Label, int32 ActionId)
	{
		UQRInventoryActionButton* B = WidgetTree->ConstructWidget<UQRInventoryActionButton>(UQRInventoryActionButton::StaticClass());
		B->OwnerWidget = this;
		B->ActionId = ActionId;

		// Pad the label inside the button so the row has a sane minimum width.
		UTextBlock* T = _MakeMenuLabel(WidgetTree, Label);
		USizeBox* Sz = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Sz->SetMinDesiredWidth(140.0f);
		Sz->SetContent(T);
		B->SetContent(Sz);

		UVerticalBoxSlot* S = List->AddChildToVerticalBox(B);
		if (S) S->SetPadding(FMargin(2));
	};

	// ActionId: 1=Equip 2=Remove 3=Inspect 4=Destroy 5=Cancel.
	if (CanEquipItem(Item) && !IsItemEquipped(Item)) AddRow(TEXT("Equip"),  1);
	if (IsItemEquipped(Item))                         AddRow(TEXT("Remove"), 2);
	AddRow(TEXT("Inspect"), 3);
	AddRow(TEXT("Destroy"), 4);
	AddRow(TEXT("Cancel"),  5);
}

void UQRInventoryGridWidget::OpenContextMenuForEquipped(UQRItemInstance* Item, FVector2D ScreenPos)
{
	OpenContextMenu(Item, ScreenPos);
}

void UQRInventoryGridWidget::CloseContextMenu()
{
	if (ContextBackdrop)
	{
		ContextBackdrop->RemoveFromParent();
		ContextBackdrop = nullptr;
	}
	if (ContextMenu)
	{
		ContextMenu->RemoveFromParent();
		ContextMenu = nullptr;
	}
	ContextTarget = nullptr;
}

void UQRInventoryGridWidget::HandleContextAction(int32 ActionId)
{
	// 6 = backdrop click (close), 7 = inspect popup close.
	if (ActionId == 6) { CloseContextMenu(); return; }
	if (ActionId == 7) { HideInspectPopup();  return; }

	UQRItemInstance* Item = ContextTarget;
	if (!Inventory || !Item || !Item->Definition) { CloseContextMenu(); return; }

	switch (ActionId)
	{
	case 1: // Equip
	{
		const EQRItemCategory Cat = Item->Definition->Category;
		if      (Cat == EQRItemCategory::Clothing)  Inventory->TryEquipArmour(Item);
		else if (Cat == EQRItemCategory::ChestRig
		      || Cat == EQRItemCategory::Backpack)  Inventory->TryEquipContainer(Item);
		else if (Cat == EQRItemCategory::Weapon)    Inventory->TryEquipToHandSlot(Item);
		break;
	}
	case 2: // Remove (unequip)
	{
		UQRItemInstance* Removed = nullptr;
		if      (Inventory->EquippedHelm        == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Helm,  Removed);
		else if (Inventory->EquippedChestArmour == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Chest, Removed);
		else if (Inventory->EquippedLegsArmour  == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Legs,  Removed);
		else if (Inventory->EquippedChestRig    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::ChestRig, Removed);
		else if (Inventory->EquippedBackpack    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::Backpack, Removed);
		else if (Inventory->HandSlot            == Item) Inventory->ClearHandSlot();
		break;
	}
	case 3: // Inspect
		ShowInspectPopup(Item);
		CloseContextMenu();
		return;  // keep inspect popup open
	case 4: // Destroy -- unequip from any slot first, then drop from Items.
	{
		UQRItemInstance* Dummy = nullptr;
		if      (Inventory->EquippedHelm        == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Helm,  Dummy);
		else if (Inventory->EquippedChestArmour == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Chest, Dummy);
		else if (Inventory->EquippedLegsArmour  == Item) Inventory->TryUnequipArmour(EQRArmourSlot::Legs,  Dummy);
		else if (Inventory->EquippedChestRig    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::ChestRig, Dummy);
		else if (Inventory->EquippedBackpack    == Item) Inventory->TryUnequipContainer(EQRContainerSlotType::Backpack, Dummy);
		else if (Inventory->HandSlot            == Item) Inventory->ClearHandSlot();
		Inventory->Items.Remove(Item);
		if (HoveredItem == Item) HoveredItem = nullptr;
		break;
	}
	case 5: // Cancel
	default:
		break;
	}

	CloseContextMenu();
	Rebuild();
}

void UQRInventoryGridWidget::ShowInspectPopup(UQRItemInstance* Item)
{
	HideInspectPopup();
	if (!Item || !Item->Definition) return;

	UCanvasPanel* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!Root) return;

	// Border directly (not a canvas wrapping a border) for the same reason as
	// the context menu -- autosize canvases collapse around fill-anchored
	// children and kill hit-testing on the close button.
	InspectPopup = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	InspectPopup->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.98f));
	UCanvasPanelSlot* RS = Root->AddChildToCanvas(InspectPopup);
	if (RS)
	{
		RS->SetAnchors(FAnchors(0.5f, 0.5f));
		RS->SetAlignment(FVector2D(0.5f, 0.5f));
		RS->SetAutoSize(true);
		RS->SetZOrder(200);
	}

	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	InspectPopup->SetContent(Col);

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

	UQRInventoryActionButton* Close = WidgetTree->ConstructWidget<UQRInventoryActionButton>(UQRInventoryActionButton::StaticClass());
	Close->OwnerWidget = this;
	Close->ActionId = 7;   // InspectClose
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
