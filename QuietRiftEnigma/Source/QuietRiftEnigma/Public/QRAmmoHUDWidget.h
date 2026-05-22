#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "QRAmmoHUDWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UQRWeaponComponent;
class UQRHotbarComponent;
class UQRInventoryComponent;
class UQRItemInstance;

/**
 * Bottom-right ammo readout. Shows the held weapon's icon (or its name
 * when no icon is authored), the rounds in the magazine, and the spare
 * ammo carried in the inventory. Hidden whenever the active hotbar item
 * isn't a weapon.
 *
 * Programmatic widget — no UMG asset required. Designer can subclass to
 * swap in a polished WBP via AQRCharacter::AmmoHUDClass.
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRAmmoHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UQRAmmoHUDWidget(const FObjectInitializer& OI);

	// Bind to the firing components so the readout stays live.
	UFUNCTION(BlueprintCallable, Category = "QR|UI")
	void Bind(UQRWeaponComponent* InWeapon, UQRHotbarComponent* InHotbar,
		UQRInventoryComponent* InInventory);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	UPROPERTY() TObjectPtr<UQRWeaponComponent>    Weapon = nullptr;
	UPROPERTY() TObjectPtr<UQRHotbarComponent>    Hotbar = nullptr;
	UPROPERTY() TObjectPtr<UQRInventoryComponent> Inventory = nullptr;

	UPROPERTY() TObjectPtr<UBorder>    Panel = nullptr;
	UPROPERTY() TObjectPtr<UImage>     WeaponIcon = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> WeaponName = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> MagText = nullptr;
	UPROPERTY() TObjectPtr<UTextBlock> ReserveText = nullptr;

	void RefreshAll();

	UFUNCTION() void HandleAmmoChanged(int32 Remaining);
	UFUNCTION() void HandleReloaded();
	UFUNCTION() void HandleActiveSlotChanged(int32 NewActiveSlot);
	UFUNCTION() void HandleSlotChanged(int32 SlotIndex, UQRItemInstance* NewItem);
	UFUNCTION() void HandleInventoryChanged();
};
