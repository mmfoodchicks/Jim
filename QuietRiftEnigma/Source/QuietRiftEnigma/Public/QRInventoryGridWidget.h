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
class UImage;
class UTexture2D;
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
	// Right-click anywhere on the widget pops the context menu against
	// HoveredItem. Left-clicks fall through to the per-button handlers.
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

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

	// Label+grid wrappers for the container grids, so the whole block
	// (heading included) collapses when the container isn't equipped.
	UPROPERTY()
	TObjectPtr<UVerticalBox> ChestGridBox = nullptr;

	UPROPERTY()
	TObjectPtr<UVerticalBox> BackpackGridBox = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> WeightText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText = nullptr;

	// Tarkov-style paper-doll: a character silhouette with equip slots
	// (Helm / Body armour / Legs / Chest rig / Backpack / Hand) anchored over
	// the matching body locations. Built into PaperDoll each Rebuild().
	UPROPERTY()
	TObjectPtr<UCanvasPanel> PaperDoll = nullptr;

	// Optional silhouette texture drawn behind the slots. If unset, a neutral
	// humanoid is drawn from simple shapes so the panel still reads as a body.
	// Designer can assign /Game/QuietRift/UI/T_BodySilhouette to swap it in.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "QR|UI")
	TObjectPtr<UTexture2D> BodySilhouette = nullptr;

	UPROPERTY()
	TObjectPtr<UQRItemInstance> GrabbedItem = nullptr;
	bool bGrabbedRotation = false;

	// Item currently under the mouse (set by per-item OnHovered/OnUnhovered).
	// Right-click anywhere on the widget pops a context menu against this.
	UPROPERTY()
	TObjectPtr<UQRItemInstance> HoveredItem = nullptr;

	// Right-click context menu (built lazily). A content-sized Border holding
	// a vertical list of action buttons -- NOT an autosize canvas, which
	// collapses to 0x0 around fill-anchored children and kills hit-testing.
	UPROPERTY()
	TObjectPtr<UBorder> ContextMenu = nullptr;

	UPROPERTY()
	TObjectPtr<UQRItemInstance> ContextTarget = nullptr;

	// Full-screen click-catcher behind the context menu (closes on outside
	// click). Torn down together with the menu.
	UPROPERTY()
	TObjectPtr<UButton> ContextBackdrop = nullptr;

	// One-off inspect popup -- text dump of the item's metadata. A Border
	// (not a canvas) for the same hit-test reasons as ContextMenu.
	UPROPERTY()
	TObjectPtr<UBorder> InspectPopup = nullptr;

	UFUNCTION() void HandleInventoryChanged();

	// Paper-doll slot identity. KindIndex on UQRInventoryEquipButton maps to
	// this: 1=Helm 2=Body armour 3=Legs 4=Chest rig 5=Backpack 6=Primary hand
	// 7=Offhand.
	enum class EEquipKind : uint8 { None, Helm, Chest, Legs, Rig, Backpack, Hand, Offhand };

	// Paper-doll canvas footprint and slot square size (pixels).
	static constexpr float PaperDollWidth  = 300.0f;
	static constexpr float PaperDollHeight = 480.0f;
	static constexpr float PaperDollSlot   = 66.0f;

public:
	// Called from sub-button click handlers (UQRInventoryCellButton / UQRInventoryItemButton).
	UFUNCTION() void HandleCellClicked(int32 PackedKey);
	UFUNCTION() void HandleEquipSlotClicked(int32 KindIndex);
	void HandleItemClicked(UQRItemInstance* Item);

	// Hover tracking -- called by UQRInventoryItemButton on its OnHovered /
	// OnUnhovered, so right-click knows what's under the cursor.
	void HandleItemHovered(UQRItemInstance* Item);
	void HandleItemUnhovered(UQRItemInstance* Item);

	// Single dispatch for context-menu action buttons (UQRInventoryAction
	// Button). ActionId: 1=Equip 2=Remove 3=Inspect 4=Destroy 5=Cancel
	// 6=CloseBackdrop 7=InspectClose.
	void HandleContextAction(int32 ActionId);
	// Right-click on an equipped slot -> open the menu against its item.
	void OpenContextMenuForEquipped(UQRItemInstance* Item, FVector2D ScreenPos);

private:
	void Rebuild();
	void RebuildKind(UCanvasPanel* Panel, EQRContainerKind Kind);
	void AddCellGrid(UCanvasPanel* Panel, EQRContainerKind Kind);
	void AddItem(UCanvasPanel* Panel, EQRContainerKind Kind, UQRItemInstance* Item);
	void RefreshHeader();

	// Paper-doll construction. RebuildPaperDoll lays the six slots over the
	// silhouette; AddSilhouette draws the body (texture or shape fallback);
	// AddPaperDollSlot places one equip button at a canvas offset.
	void RebuildPaperDoll();
	void AddSilhouette();
	void AddPaperDollSlot(const TCHAR* Label, int32 KindIndex,
		UQRItemInstance* Item, float X, float Y);

	// Build + show / hide the context menu. ScreenPos is in absolute
	// screen pixels; the menu is positioned just below that point.
	void OpenContextMenu(UQRItemInstance* Item, FVector2D ScreenPos);
	void CloseContextMenu();
	void ShowInspectPopup(UQRItemInstance* Item);
	void HideInspectPopup();

	// Predicates so the menu shows only the actions that apply.
	bool CanEquipItem(UQRItemInstance* Item) const;
	bool IsItemEquipped(UQRItemInstance* Item) const;
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
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
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

	// The item currently in this slot (null when empty) -- lets right-click
	// open the context menu against an equipped item.
	UPROPERTY()
	TObjectPtr<UQRItemInstance> SlotItem = nullptr;

	UFUNCTION() void HandleClicked();
	UFUNCTION() void HandleHovered();
	UFUNCTION() void HandleUnhovered();
};

/**
 * Context-menu action button. Carries an action id + owner back-ref and
 * binds OnClicked in its constructor (the pattern the cell/item buttons
 * use, which works reliably -- binding a plain UButton to a widget method
 * at runtime did not fire). ActionId: 1=Equip 2=Remove 3=Inspect
 * 4=Destroy 5=Cancel 6=CloseBackdrop 7=InspectClose.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRInventoryActionButton : public UButton
{
	GENERATED_BODY()

public:
	UQRInventoryActionButton();

	UPROPERTY()
	TWeakObjectPtr<UQRInventoryGridWidget> OwnerWidget;

	UPROPERTY()
	int32 ActionId = 0;

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
