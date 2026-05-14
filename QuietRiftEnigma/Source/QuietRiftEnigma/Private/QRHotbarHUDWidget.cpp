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
		for (int32 i = 0; i < 9; ++i)
		{
			USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
				*FString::Printf(TEXT("SlotSize_%d"), i));
			SizeBox->SetWidthOverride(96.0f);
			SizeBox->SetHeightOverride(64.0f);

			UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
				*FString::Printf(TEXT("SlotBorder_%d"), i));
			Border->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f));
			Border->SetPadding(FMargin(6.0f));
			Border->SetHorizontalAlignment(HAlign_Center);
			Border->SetVerticalAlignment(VAlign_Center);

			UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
				*FString::Printf(TEXT("SlotLabel_%d"), i));
			Label->SetText(FText::FromString(FString::Printf(TEXT("%d\n—"), i + 1)));
			Label->SetJustification(ETextJustify::Center);

			Border->SetContent(Label);
			SizeBox->SetContent(Border);

			UHorizontalBoxSlot* HSlot = Row->AddChildToHorizontalBox(SizeBox);
			HSlot->SetPadding(FMargin(4.0f, 0.0f));

			SlotBorders.Add(Border);
			SlotLabels.Add(Label);
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
	if (!SlotBorders.IsValidIndex(SlotIndex) || !SlotLabels.IsValidIndex(SlotIndex)) return;

	const UQRItemInstance* Item = Hotbar ? Hotbar->GetSlot(SlotIndex) : nullptr;
	FString DisplayText = FString::Printf(TEXT("%d\n—"), SlotIndex + 1);
	if (Item && Item->Definition)
	{
		const FString Name = Item->Definition->DisplayName.IsEmpty()
			? Item->Definition->ItemId.ToString()
			: Item->Definition->DisplayName.ToString();
		DisplayText = FString::Printf(TEXT("%d\n%s\nx%d"), SlotIndex + 1, *Name, Item->Quantity);
	}
	SlotLabels[SlotIndex]->SetText(FText::FromString(DisplayText));

	const bool bActive = Hotbar && Hotbar->ActiveSlotIndex == SlotIndex;
	SlotBorders[SlotIndex]->SetBrushColor(bActive
		? FLinearColor(0.85f, 0.65f, 0.10f, 0.95f)
		: FLinearColor(0.05f, 0.05f, 0.05f, 0.85f));
}
