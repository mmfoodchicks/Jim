#include "QRInputDefaults.h"
#include "QRCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/PlayerController.h"

UInputAction* UQRInputDefaults::MakeAction(UObject* Outer, FName Name, EInputActionValueType ValueType)
{
	UInputAction* A = NewObject<UInputAction>(Outer, Name);
	A->ValueType = ValueType;
	return A;
}

void UQRInputDefaults::Apply(AQRCharacter* Character)
{
	if (!Character) return;
	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC || !PC->IsLocalController()) return;

	// ── Fill any unset IA slots with runtime defaults ──
	if (!Character->MoveAction)     Character->MoveAction     = MakeAction(Character, "IA_Move_RT",     EInputActionValueType::Axis2D);
	if (!Character->LookAction)     Character->LookAction     = MakeAction(Character, "IA_Look_RT",     EInputActionValueType::Axis2D);
	if (!Character->JumpAction)     Character->JumpAction     = MakeAction(Character, "IA_Jump_RT",     EInputActionValueType::Boolean);
	if (!Character->CrouchAction)   Character->CrouchAction   = MakeAction(Character, "IA_Crouch_RT",   EInputActionValueType::Boolean);
	if (!Character->InteractAction) Character->InteractAction = MakeAction(Character, "IA_Interact_RT", EInputActionValueType::Boolean);
	if (!Character->FireAction)     Character->FireAction     = MakeAction(Character, "IA_Fire_RT",     EInputActionValueType::Boolean);
	if (!Character->ReloadAction)   Character->ReloadAction   = MakeAction(Character, "IA_Reload_RT",   EInputActionValueType::Boolean);
	if (!Character->SprintAction)   Character->SprintAction   = MakeAction(Character, "IA_Sprint_RT",   EInputActionValueType::Boolean);
	if (!Character->LeanLeftAction) Character->LeanLeftAction = MakeAction(Character, "IA_LeanL_RT",   EInputActionValueType::Boolean);
	if (!Character->LeanRightAction)Character->LeanRightAction= MakeAction(Character, "IA_LeanR_RT",   EInputActionValueType::Boolean);
	if (!Character->DropAction)     Character->DropAction     = MakeAction(Character, "IA_Drop_RT",     EInputActionValueType::Boolean);
	if (!Character->CreativeBrowserAction) Character->CreativeBrowserAction = MakeAction(Character, "IA_Browser_RT", EInputActionValueType::Boolean);
	if (!Character->HotbarNextAction) Character->HotbarNextAction = MakeAction(Character, "IA_HotbarNext_RT", EInputActionValueType::Boolean);
	if (!Character->HotbarPrevAction) Character->HotbarPrevAction = MakeAction(Character, "IA_HotbarPrev_RT", EInputActionValueType::Boolean);
	if (!Character->UseHeldAction)    Character->UseHeldAction    = MakeAction(Character, "IA_UseHeld_RT",    EInputActionValueType::Boolean);
	if (!Character->PauseAction)      Character->PauseAction      = MakeAction(Character, "IA_Pause_RT",      EInputActionValueType::Boolean);
	if (!Character->InventoryAction)  Character->InventoryAction  = MakeAction(Character, "IA_Inventory_RT",  EInputActionValueType::Boolean);

	if (Character->HotbarSlotActions.Num() < 9) Character->HotbarSlotActions.SetNum(9);
	for (int32 i = 0; i < 9; ++i)
	{
		if (!Character->HotbarSlotActions[i])
		{
			Character->HotbarSlotActions[i] = MakeAction(Character,
				FName(*FString::Printf(TEXT("IA_Slot%d_RT"), i + 1)),
				EInputActionValueType::Boolean);
		}
	}

	// ── Build the mapping context ──
	UInputMappingContext* IMC = NewObject<UInputMappingContext>(Character, "IMC_QR_Runtime");

	// Move: WASD with axis modifiers (W = +Y, S = -Y, D = +X, A = -X).
	auto AddMoveKey = [&](FKey Key, bool bNegate, bool bSwizzleYX)
	{
		FEnhancedActionKeyMapping& M = IMC->MapKey(Character->MoveAction, Key);
		if (bSwizzleYX) M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(IMC));
		if (bNegate)    M.Modifiers.Add(NewObject<UInputModifierNegate>(IMC));
	};
	AddMoveKey(EKeys::W, /*negate*/ false, /*swizzleYX*/ true);   // +Y
	AddMoveKey(EKeys::S, /*negate*/ true,  /*swizzleYX*/ true);   // -Y
	AddMoveKey(EKeys::D, /*negate*/ false, /*swizzleYX*/ false);  // +X
	AddMoveKey(EKeys::A, /*negate*/ true,  /*swizzleYX*/ false);  // -X

	// Look: mouse XY → action Axis2D. Negate Y so up = look up (UE Y-down).
	{
		FEnhancedActionKeyMapping& MX = IMC->MapKey(Character->LookAction, EKeys::MouseX);
		// MouseX is X, default — no modifiers needed (action wants X in .X).
		FEnhancedActionKeyMapping& MY = IMC->MapKey(Character->LookAction, EKeys::MouseY);
		MY.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(IMC));
		MY.Modifiers.Add(NewObject<UInputModifierNegate>(IMC));
	}

	IMC->MapKey(Character->JumpAction,             EKeys::SpaceBar);
	IMC->MapKey(Character->SprintAction,           EKeys::LeftShift);
	IMC->MapKey(Character->CrouchAction,           EKeys::LeftControl);
	IMC->MapKey(Character->InteractAction,         EKeys::F);
	IMC->MapKey(Character->FireAction,             EKeys::LeftMouseButton);
	IMC->MapKey(Character->ReloadAction,           EKeys::R);
	IMC->MapKey(Character->LeanLeftAction,         EKeys::Q);
	IMC->MapKey(Character->LeanRightAction,        EKeys::E);
	IMC->MapKey(Character->DropAction,             EKeys::G);
	IMC->MapKey(Character->CreativeBrowserAction,  EKeys::Tab);
	IMC->MapKey(Character->HotbarNextAction,       EKeys::MouseScrollDown);
	IMC->MapKey(Character->HotbarPrevAction,       EKeys::MouseScrollUp);
	IMC->MapKey(Character->UseHeldAction,          EKeys::RightMouseButton);
	IMC->MapKey(Character->PauseAction,            EKeys::Escape);
	IMC->MapKey(Character->InventoryAction,        EKeys::I);

	const FKey SlotKeys[9] = {
		EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
	};
	for (int32 i = 0; i < 9; ++i)
	{
		IMC->MapKey(Character->HotbarSlotActions[i], SlotKeys[i]);
	}

	// Save as the character's default mapping context. If one is already
	// authored in BP we leave it as the primary and add ours at lower
	// priority so the BP mapping wins on key collisions.
	const int32 Priority = Character->DefaultMappingContext ? 50 : 0;
	if (!Character->DefaultMappingContext)
	{
		Character->DefaultMappingContext = IMC;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(IMC, Priority);
	}
}
