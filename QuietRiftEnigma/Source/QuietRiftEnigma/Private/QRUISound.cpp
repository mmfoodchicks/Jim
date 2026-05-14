#include "QRUISound.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// Each helper caches its sound across calls. UE GC keeps the asset
	// alive while the cache pointer is non-null because PlaySound2D
	// hands the reference to the AudioComponent it spawns.
	USoundBase* LazyLoadSound(const TCHAR* AssetPath, USoundBase*& Cache)
	{
		if (!Cache)
		{
			Cache = LoadObject<USoundBase>(nullptr, AssetPath);
		}
		return Cache;
	}
}

namespace QRUISound
{
	void PlayClick(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Fabs/PhoneSystem/Sound/SW_Beep.SW_Beep"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayConfirm(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Fabs/PhoneSystem/Sound/SW_Cash.SW_Cash"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayDeny(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Fabs/PhoneSystem/Sound/SW_Rejected.SW_Rejected"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayFootstep(UObject* WC, FVector Location, float VolumeMult)
	{
		static USoundBase* Cached = nullptr;
		// _Cue contains the 4-sample random selector, so each step varies.
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Fabs/Bodycam_VHS_Effect/Sounds/FootSteps/S_FootStep_Carpet_Cue.S_FootStep_Carpet_Cue"),
			Cached);
		if (S && WC)
		{
			UGameplayStatics::PlaySoundAtLocation(WC, S, Location, VolumeMult);
		}
	}
}
