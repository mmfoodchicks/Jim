#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "QRTypes.h"
#include "QRInventoryGridWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UVerticalBox;
class UBorder;
class UTextBlock;
class UQRInventoryComponent;
class UQRItemInstance;

/**
 * Tarkov-style spatial inventory overlay. Renders the three containers
 * (Body / ChestRig / Backpack) as fixed-cell grids and the hand slot
 * as a single cell. Each placed item is a colored Border button
 * positioned by its (GridX, GridY) × footprint.
 *
 * Interaction model — two-click place:
 *   1. Left-click an item    → "grab" it (highlights, stays in place).
 *   2. R                     → rotate the grabbed item.
 *   3. Left-click empty cell → TryMoveItem(NewKind, X, Y, rotation).
 *   4. Right-click an item   → TryRotateItem in place (single-click rotate).
 *   5. Esc                   → cancel grab or close widget.
 *
 * No drag preview that follows the cursor — instead the cells highlight
 * on hover when something is grabbed. Lighter to author, equally clear.
 *
 * Mounted by AQRCharacter on the I key (toggle). The widget binds to
 * UQRInventoryComponent::OnInventoryChanged so it rebuilds when the
 * server confirms a placement / rotation.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRInventoryGridWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRInventoryGridWidget(const FObjectInitializer& OI);

	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRInventoryComponent* InInventory);

	// Cell pixel size — overridable in BP, default 64.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|UI")
	float CellSize = 64.0f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY()
	TObjectPtr<UQRInventoryComponent> Inventory = nullptr;

	// Per-container grid canvases — one per EQRContainerKind value.
	UPROPERTY()
	TObjectPtr<UCanvasPanel> BodyGrid = nullptr;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> ChestGrid = nullptr;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> BackpackGrid = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> WeightText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText = nullptr;

	// Equipment-slot strip at the top: Helm / Chest armour / Legs armour /
	// Chest rig / Backpack. Each is a 64x64 border button. Click an empty
	// slot to drop in whatever is grabbed; click an occupied slot to
	// unequip (double-click on a container slot opens its grid).
	UPROPERTY()
	TObjectPtr<UHorizontalBox> EquipStrip = nullptr;

	UPROPERTY()
	TObjectPtr<UQRItemInstance> GrabbedItem = nullptr;
	bool bGrabbedRotation = false;

	UFUNCTION() void HandleInventoryChanged();

	// State for double-click container open. When the user clicks a rig
	// or backpack slot once, this records (slot, time); a second click in
	// the same slot within DoubleClickWindowSec toggles the corresponding
	// grid visibility. Single click = (un)equip / equip-from-grab.
	enum class EEquipKind : uint8 { None, Helm, Chest, Legs, Rig, Backpack };
	EEquipKind LastClickedEquip = EEquipKind::None;
	double LastClickedTime = 0.0;
	static constexpr double DoubleClickWindowSec = 0.35;

	// Per-container visibility -- the rig grid is hidden by default until
	// the user double-clicks the rig slot; same for the backpack grid.
	bool bShowChestGrid = true;
	bool bShowBackpackGrid = true;

public:
	// Called from sub-button click handlers (UQRInventoryCellButton / UQRInventoryItemButton).
	UFUNCTION() void HandleCellClicked(int32 PackedKey);
	UFUNCTION() void HandleEquipSlotClicked(int32 KindIndex);
	void HandleItemClicked(UQRItemInstance* Item);

private:
	void Rebuild();
	void RebuildKind(UCanvasPanel* Panel, EQRContainerKind Kind);
	void AddCellGrid(UCanvasPanel* Panel, EQRContainerKind Kind);
	void AddItem(UCanvasPanel* Panel, EQRContainerKind Kind, UQRItemInstance* Item);
	void RefreshHeader();
	void RebuildEquipStrip();
};

/**
 * Single placed-item button. Carries an item pointer + a back-reference
 * to its widget so click handlers dispatch correctly. Programmatic only.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRInventoryItemButton : public UButton
{
	GENERATED_BODY()

public:
	UQRInventoryItemButton();

	UPROPERTY()
	TWeakObjectPtr<UQRInventoryGridWidget> OwnerWidget;

	UPROPERTY()
	TObjectPtr<UQRItemInstance> Item = nullptr;

	UFUNCTION() void HandleClicked();
};

/**
 * Equipment-slot button. Click toggles equip/unequip; double-click on a
 * container slot (Rig / Backpack) toggles whether its grid is visible.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRInventoryEquipButton : public UButton
{
	GENERATED_BODY()

public:
	UQRInventoryEquipButton();

	UPROPERTY()
	TWeakObjectPtr<UQRInventoryGridWidget> OwnerWidget;

	// 1=Helm 2=Chest armour 3=Legs armour 4=Rig 5=Backpack. Matches
	// UQRInventoryGridWidget::EEquipKind.
	UPROPERTY()
	int32 KindIndex = 0;

	UFUNCTION() void HandleClicked();
};

/**
 * Single empty-cell button. Carries (Kind, X, Y) so click dispatches a
 * placement when something is grabbed.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRInventoryCellButton : public UButton
{
	GENERATED_BODY()

public:
	UQRInventoryCellButton();

	UPROPERTY()
	TWeakObjectPtr<UQRInventoryGridWidget> OwnerWidget;

	UPROPERTY()
	EQRContainerKind Kind = EQRContainerKind::Body;

	UPROPERTY()
	int32 X = 0;

	UPROPERTY()
	int32 Y = 0;

	UFUNCTION() void HandleClicked();

	// Pack (Kind, X, Y) into an int32 so the owner widget's UFUNCTION
	// callback can take a single integer payload across the dynamic-
	// delegate boundary.
	int32 PackedKey() const
	{
		return (static_cast<int32>(Kind) << 24) | ((X & 0xFFF) << 12) | (Y & 0xFFF);
	}
	static void Unpack(int32 Key, EQRContainerKind& OutKind, int32& OutX, int32& OutY)
	{
		OutKind = static_cast<EQRContainerKind>((Key >> 24) & 0xFF);
		OutX    = (Key >> 12) & 0xFFF;
		OutY    = Key         & 0xFFF;
	}
};
