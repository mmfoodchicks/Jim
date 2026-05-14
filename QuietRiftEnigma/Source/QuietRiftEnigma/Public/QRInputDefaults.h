#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InputActionValue.h"
#include "QRInputDefaults.generated.h"

class AQRCharacter;
class UInputAction;
class UInputMappingContext;

/**
 * Runtime default input setup.
 *
 * Builds all the Enhanced Input assets the character expects — IA_Move,
 * IA_Look, IA_Jump, IA_Sprint, IA_Crouch, IA_Interact, IA_Fire, IA_Reload,
 * IA_LeanLeft, IA_LeanRight, IA_Drop, IA_CreativeBrowser, IA_HotbarSlot1..9,
 * IA_HotbarNext, IA_HotbarPrev — as transient UObjects, and wires them up
 * into a UInputMappingContext with sensible default keys. The character
 * calls Apply(this) in BeginPlay so any input slot that's null in BP
 * gets a runtime default instead of just silently doing nothing.
 *
 * If the user has already authored matching IA / IMC assets in BP and
 * assigned them to the character, those win — Apply only fills holes.
 *
 * Default keys (none of these collide with Q/E which are lean):
 *   WASD             move
 *   Mouse            look
 *   Space            jump (consults vault first)
 *   Left Shift       sprint
 *   Left Ctrl        crouch
 *   F                interact
 *   LMB              fire
 *   R                reload
 *   Q / E            lean L / R
 *   G                drop held item
 *   Tab              toggle creative browser
 *   1..9             hotbar slot select
 *   Mouse wheel      hotbar prev / next
 */
UCLASS()
class QUIETRIFTENIGMA_API UQRInputDefaults : public UObject
{
	GENERATED_BODY()

public:
	// Fills any null input fields on the character with runtime-constructed
	// UInputAction objects, builds an IMC mapping all of them to default
	// keys, and pushes the IMC into the local player's Enhanced Input
	// subsystem. Safe to call on remote pawns (no-ops gracefully).
	static void Apply(AQRCharacter* Character);

private:
	static UInputAction* MakeAction(UObject* Outer, FName Name, EInputActionValueType ValueType);
};
