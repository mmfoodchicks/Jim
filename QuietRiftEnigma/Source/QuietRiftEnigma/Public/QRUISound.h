#pragma once

#include "CoreMinimal.h"
#include "QRUISound.generated.h"

class UObject;

/**
 * Audio surface enum for footstep dispatch. Mirrors the folders inside
 * /Game/Fabs/Essential_Foosteps_SK/CUE/. Default Concrete works for any
 * indoor / hard surface fallback when no PhysicalMaterial is wired.
 */
UENUM(BlueprintType)
enum class EQRFootSurface : uint8
{
	Concrete	UMETA(DisplayName = "Concrete"),
	Dirt		UMETA(DisplayName = "Dirt"),
	Glass		UMETA(DisplayName = "Glass"),
	Gravel		UMETA(DisplayName = "Gravel"),
	Leaves		UMETA(DisplayName = "Leaves"),
	Metal		UMETA(DisplayName = "Metal"),
	Sand		UMETA(DisplayName = "Sand"),
	Slush		UMETA(DisplayName = "Slush"),
	Snow		UMETA(DisplayName = "Snow"),
	Wood		UMETA(DisplayName = "Wood"),
};

/**
 * Footstep gait — picks Walk / Jog / Run / Jump / Land cues from the
 * Essential_Foosteps_SK pack. Stops (Walk_Stop, Run_Stop) are handled
 * separately on the velocity transition rather than via this enum.
 */
UENUM(BlueprintType)
enum class EQRFootGait : uint8
{
	Walk	UMETA(DisplayName = "Walk"),
	Jog		UMETA(DisplayName = "Jog"),
	Run		UMETA(DisplayName = "Run"),
	Jump	UMETA(DisplayName = "Jump"),
	Land	UMETA(DisplayName = "Land"),
};

/**
 * Tiny helper that plays UI feedback sounds and 3D footsteps using the
 * Free_Sounds_Pack and Essential_Foosteps_SK Fab packs respectively.
 * Lazy-loads every SoundBase asset on first call so missing Fab content
 * degrades to a silent no-op instead of a load failure.
 */
namespace QRUISound
{
	// Light beep on a generic button press / hover / hotbar scroll.
	QUIETRIFTENIGMA_API void PlayClick(UObject* WorldContextObject);

	// Confirm sting on equip / accept / slot-assign.
	QUIETRIFTENIGMA_API void PlayConfirm(UObject* WorldContextObject);

	// "Rejected" buzz on a denied action (full inventory, no recipe, …).
	QUIETRIFTENIGMA_API void PlayDeny(UObject* WorldContextObject);

	// Weapon fire SFX, 3D at muzzle location. Used by UQRWeaponComponent.
	QUIETRIFTENIGMA_API void PlayWeaponFire(
		UObject* WorldContextObject, FVector Location, float VolumeMult = 1.0f);

	// 3D footstep at world location, surface + gait aware.
	QUIETRIFTENIGMA_API void PlayFootstep(
		UObject* WorldContextObject,
		FVector Location,
		EQRFootSurface Surface = EQRFootSurface::Concrete,
		EQRFootGait Gait = EQRFootGait::Walk,
		float VolumeMult = 1.0f);

	// Resolve EPhysicalSurface (from a UPhysicalMaterial) into our footstep
	// surface enum. Unknown / unmapped surfaces fall back to Concrete.
	QUIETRIFTENIGMA_API EQRFootSurface SurfaceFromPhysMat(
		uint8 PhysicalSurfaceType);
}
