#pragma once

#include "CoreMinimal.h"

class UObject;

/**
 * Tiny helper that plays UI feedback sounds borrowed from the PhoneSystem
 * Fab pack. Lazy-loads the SoundBase asset on first call so missing
 * Fab content degrades to a silent no-op instead of a load failure.
 */
namespace QRUISound
{
	// Light beep on a generic button press or hover.
	QUIETRIFTENIGMA_API void PlayClick(UObject* WorldContextObject);

	// Cash-register-ish confirm on equip / accept.
	QUIETRIFTENIGMA_API void PlayConfirm(UObject* WorldContextObject);

	// "Rejected" buzz on a denied action (full inventory, no recipe, etc.).
	QUIETRIFTENIGMA_API void PlayDeny(UObject* WorldContextObject);

	// 3D footstep at world location. Only one carpet variant is in the
	// Fab pack so every surface uses it for now; surface-specific lookup
	// can be added once we have more samples.
	QUIETRIFTENIGMA_API void PlayFootstep(
		UObject* WorldContextObject, FVector Location, float VolumeMult = 1.0f);
}
