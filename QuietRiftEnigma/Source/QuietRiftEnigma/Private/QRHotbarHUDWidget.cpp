#include "QRHotbarHUDWidget.h"
#include "QRHotbarComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"

UQRHotbarHUDWidget::UQRHotbarHUDWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRHotbarHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HotbarRow"));

		SlotBorders.Reset();
		SlotLabels.Reset();
		SlotIcons.Reset();
		for (int32 i = 0; i < 9; ++i)
		{
			USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
				*FString::Printf(TEXT("SlotSize_%d"), i));
			SizeBox->SetWidthOverride(74.0f);
			SizeBox->SetHeightOverride(74.0f);

			// Slot tile — dark translucent slate; the active slot is
			// re-tinted amber in RefreshSlot.
			UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("SlotBorder_%d"), i));
			Border->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.07f, 0.82f));
			Border->SetPadding(FMargin(3.0f));
			Border->SetHorizontalAlignment(HAlign_Fill);
			Border->SetVerticalAlignment(VAlign_Fill);

			// Overlay stacks the item icon (fill) under the text label.
			UOverlay* Cell = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),
				*FString::Printf(TEXT("SlotCell_%d"), i));

			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
				*FString::Printf(TEXT("SlotIcon_%d"), i));
			Icon->SetVisibility(ESlateVisibility::Collapsed);
			if (UOverlaySlot* IconSlot = Cell->AddChildToOverlay(Icon))
			{
				IconSlot->SetHorizontalAlignment(HAlign_Fill);
				IconSlot->SetVerticalAlignment(VAlign_Fill);
			}

			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				*FString::Printf(TEXT("SlotLabel_%d"), i));
			Label->SetText(FText::FromString(FString::Printf(TEXT("%d"), i + 1)));
			Label->SetJustification(ETextJustify::Center);
			{
				FSlateFontInfo Font = Label->GetFont();
				Font.Size = 11;
				Label->SetFont(Font);
			}
			if (UOverlaySlot* LabelSlot = Cell->AddChildToOverlay(Label))
			{
				LabelSlot->SetHorizontalAlignment(HAlign_Fill);
				LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			}

			Border->SetContent(Cell);
			SizeBox->SetContent(Border);

			UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(SizeBox);
			HSlot->SetPadding(FMargin(4.0f, 0.0f));

			SlotBorders.Add(Border);
			SlotLabels.Add(Label);
			SlotIcons.Add(Icon);
		}

		UCanvasPanelSlot* RowSlot = Root->AddChildToCanvas(Row);
		RowSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		RowSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		RowSlot->SetAutoSize(true);
		RowSlot->SetPosition(FVector2D(0.0f, -40.0f));
	}

	return Super::RebuildWidget();
}

void UQRHotbarHUDWidget::NativeDestruct()
{
	if (Hotbar)
	{
		Hotbar->OnSlotChanged.RemoveDynamic(this, &UQRHotbarHUDWidget::HandleSlotChanged);
		Hotbar->OnActiveSlotChanged.RemoveDynamic(this, &UQRHotbarHUDWidget::HandleActiveSlotChanged);
	}
	Super::NativeDestruct();
}

void UQRHotbarHUDWidget::Bind(UQRHotbarComponent* InHotbar)
{
	if (Hotbar == InHotbar) return;
	if (Hotbar)
	{
		Hotbar->OnSlotChanged.RemoveDynamic(this, &UQRHotbarHUDWidget::HandleSlotChanged);
		Hotbar->OnActiveSlotChanged.RemoveDynamic(this, &UQRHotbarHUDWidget::HandleActiveSlotChanged);
	}
	Hotbar = InHotbar;
	if (Hotbar)
	{
		Hotbar->OnSlotChanged.AddDynamic(this, &UQRHotbarHUDWidget::HandleSlotChanged);
		Hotbar->OnActiveSlotChanged.AddDynamic(this, &UQRHotbarHUDWidget::HandleActiveSlotChanged);
	}
	RefreshAll();
}

void UQRHotbarHUDWidget::HandleSlotChanged(int32 SlotIndex, UQRItemInstance* /*NewItem*/)
{
	if (SlotIndex < 0) RefreshAll();
	else RefreshSlot(SlotIndex);
}

void UQRHotbarHUDWidget::HandleActiveSlotChanged(int32 /*NewActiveSlot*/)
{
	RefreshAll();
}

void UQRHotbarHUDWidget::RefreshAll()
{
	for (int32 i = 0; i < SlotBorders.Num(); ++i) RefreshSlot(i);
}

void UQRHotbarHUDWidget::RefreshSlot(int32 SlotIndex)
{
	if (!SlotBorders.IsValidIndex(SlotIndex) || !SlotLabels.IsValidIndex(SlotIndex)
		|| !SlotIcons.IsValidIndex(SlotIndex)) return;

	const UQRItemInstance* Item = Hotbar ? Hotbar->GetSlot(SlotIndex) : nullptr;
	const UQRItemDefinition* Def = Item ? Item->Definition : nullptr;

	// Resolve the item's icon. Many auto-seeded items have no icon yet,
	// so fall back to the item name as text when the texture is missing.
	UTexture2D* IconTex = Def ? Def->InventoryIcon.LoadSynchronous() : nullptr;

	if (UImage* Icon = SlotIcons[SlotIndex])
	{
		if (IconTex)
		{
			Icon->SetBrushFromTexture(IconTex);
			Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Label: slot number alone when empty or icon-backed; the item name
	// is shown only as a fallback for icon-less items.
	FString DisplayText = FString::Printf(TEXT("%d"), SlotIndex + 1);
	if (Def)
	{
		if (IconTex)
		{
			if (Item->Quantity > 1)
				DisplayText = FString::Printf(TEXT("%d   x%d"), SlotIndex + 1, Item->Quantity);
		}
		else
		{
			const FString Name = Def->DisplayName.IsEmpty()
				? Def->ItemId.ToString()
				: Def->DisplayName.ToString();
			DisplayText = FString::Printf(TEXT("%d\n%s\nx%d"), SlotIndex + 1, *Name, Item->Quantity);
		}
	}
	SlotLabels[SlotIndex]->SetText(FText::FromString(DisplayText));

	const bool bActive = Hotbar && Hotbar->ActiveSlotIndex == SlotIndex;
	SlotBorders[SlotIndex]->SetBrushColor(bActive
		? FLinearColor(0.95f, 0.72f, 0.15f, 0.95f)
		: FLinearColor(0.04f, 0.05f, 0.07f, 0.82f));
}
