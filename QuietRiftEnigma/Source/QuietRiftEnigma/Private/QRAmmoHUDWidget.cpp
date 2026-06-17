#include "QRAmmoHUDWidget.h"
#include "QRWeaponComponent.h"
#include "QRHotbarComponent.h"
#include "QRInventoryComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

UQRAmmoHUDWidget::UQRAmmoHUDWidget(const FObjectInitializer& OI)
	: Super(OI)
{
}

TSharedRef<SWidget> UQRAmmoHUDWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Canvas;

		// Dark translucent panel, anchored bottom-right.
		Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AmmoPanel"));
		Panel->SetBrushColor(FLinearColor(0.03f, 0.04f, 0.06f, 0.78f));
		Panel->SetPadding(FMargin(14.0f, 10.0f));
		Panel->SetHorizontalAlignment(HAlign_Fill);
		Panel->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("AmmoStack"));

		// Held-weapon icon.
		WeaponIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("WeaponIcon"));
		WeaponIcon->SetVisibility(ESlateVisibility::Collapsed);
		WeaponIcon->SetBrushSize(FVector2D(132.0f, 54.0f));
		if (UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(WeaponIcon))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}

		// Weapon name — shown only when no icon is authored.
		WeaponName = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WeaponName"));
		WeaponName->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.78f, 0.82f, 1.0f)));
		WeaponName->SetJustification(ETextJustify::Right);
		{
			FSlateFontInfo F = WeaponName->GetFont();
			F.Size = 12;
			WeaponName->SetFont(F);
		}
		WeaponName->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(WeaponName))
		{
			S->SetHorizontalAlignment(HAlign_Right);
		}

		// Magazine / reserve row.
		UHorizontalBox* CountRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("CountRow"));

		MagText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MagText"));
		MagText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		MagText->SetText(FText::FromString(TEXT("0")));
		{
			FSlateFontInfo F = MagText->GetFont();
			F.Size = 38;
			MagText->SetFont(F);
		}
		if (UHorizontalBoxSlot* S = CountRow->AddChildToHorizontalBox(MagText))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}

		ReserveText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ReserveText"));
		ReserveText->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.66f, 0.72f, 1.0f)));
		ReserveText->SetText(FText::FromString(TEXT("/ 0")));
		{
			FSlateFontInfo F = ReserveText->GetFont();
			F.Size = 18;
			ReserveText->SetFont(F);
		}
		if (UHorizontalBoxSlot* S = CountRow->AddChildToHorizontalBox(ReserveText))
		{
			S->SetVerticalAlignment(VAlign_Bottom);
			S->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 6.0f));
		}

		if (UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(CountRow))
		{
			S->SetHorizontalAlignment(HAlign_Right);
		}

		Panel->SetContent(Stack);

		if (UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel))
		{
			PanelSlot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
			PanelSlot->SetAlignment(FVector2D(1.0f, 1.0f));
			PanelSlot->SetAutoSize(true);
			PanelSlot->SetPosition(FVector2D(-32.0f, -32.0f));
		}

		// Hidden until a weapon is equipped.
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}

	return Super::RebuildWidget();
}

void UQRAmmoHUDWidget::Bind(UQRWeaponComponent* InWeapon, UQRHotbarComponent* InHotbar,
	UQRInventoryComponent* InInventory)
{
	if (Weapon)
	{
		Weapon->OnAmmoChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleAmmoChanged);
		Weapon->OnWeaponReloaded.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleReloaded);
	}
	if (Hotbar)
	{
		Hotbar->OnActiveSlotChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleActiveSlotChanged);
		Hotbar->OnSlotChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleSlotChanged);
	}
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleInventoryChanged);
	}

	Weapon = InWeapon;
	Hotbar = InHotbar;
	Inventory = InInventory;

	if (Weapon)
	{
		Weapon->OnAmmoChanged.AddDynamic(this, &UQRAmmoHUDWidget::HandleAmmoChanged);
		Weapon->OnWeaponReloaded.AddDynamic(this, &UQRAmmoHUDWidget::HandleReloaded);
	}
	if (Hotbar)
	{
		Hotbar->OnActiveSlotChanged.AddDynamic(this, &UQRAmmoHUDWidget::HandleActiveSlotChanged);
		Hotbar->OnSlotChanged.AddDynamic(this, &UQRAmmoHUDWidget::HandleSlotChanged);
	}
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UQRAmmoHUDWidget::HandleInventoryChanged);
	}

	RefreshAll();
}

void UQRAmmoHUDWidget::NativeDestruct()
{
	if (Weapon)
	{
		Weapon->OnAmmoChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleAmmoChanged);
		Weapon->OnWeaponReloaded.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleReloaded);
	}
	if (Hotbar)
	{
		Hotbar->OnActiveSlotChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleActiveSlotChanged);
		Hotbar->OnSlotChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleSlotChanged);
	}
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &UQRAmmoHUDWidget::HandleInventoryChanged);
	}
	Super::NativeDestruct();
}

void UQRAmmoHUDWidget::HandleAmmoChanged(int32 /*Remaining*/)            { RefreshAll(); }
void UQRAmmoHUDWidget::HandleReloaded()                                 { RefreshAll(); }
void UQRAmmoHUDWidget::HandleActiveSlotChanged(int32 /*NewActiveSlot*/)  { RefreshAll(); }
void UQRAmmoHUDWidget::HandleSlotChanged(int32, UQRItemInstance*)        { RefreshAll(); }
void UQRAmmoHUDWidget::HandleInventoryChanged()                         { RefreshAll(); }

void UQRAmmoHUDWidget::RefreshAll()
{
	if (!Panel) return;

	// Only show the readout while a weapon is the active hotbar item.
	const UQRItemInstance* Active = Hotbar ? Hotbar->GetActiveItem() : nullptr;
	const UQRItemDefinition* Def = (Active && Active->IsValid()) ? Active->Definition : nullptr;
	const bool bWeapon = Def && Def->Category == EQRItemCategory::Weapon;

	if (!bWeapon || !Weapon)
	{
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Weapon icon, with the name as a fallback when no icon is authored.
	UTexture2D* IconTex = Def->InventoryIcon.LoadSynchronous();
	if (WeaponIcon)
	{
		if (IconTex)
		{
			WeaponIcon->SetBrushFromTexture(IconTex);
			WeaponIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			WeaponIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (WeaponName)
	{
		const FString Name = Def->DisplayName.IsEmpty()
			? Def->ItemId.ToString()
			: Def->DisplayName.ToString();
		WeaponName->SetText(FText::FromString(Name));
		WeaponName->SetVisibility(IconTex
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	// Magazine count.
	if (MagText)
	{
		MagText->SetText(FText::AsNumber(Weapon->CurrentAmmo));
	}

	// Reserve — total Ammo-category rounds carried in the inventory.
	int32 Reserve = 0;
	if (Inventory)
	{
		const TArray<UQRItemInstance*> AmmoStacks =
			Inventory->GetItemsByCategory(EQRItemCategory::Ammo);
		for (const UQRItemInstance* Stack : AmmoStacks)
		{
			if (Stack) Reserve += Stack->Quantity;
		}
	}
	if (ReserveText)
	{
		ReserveText->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), Reserve)));
	}
}
