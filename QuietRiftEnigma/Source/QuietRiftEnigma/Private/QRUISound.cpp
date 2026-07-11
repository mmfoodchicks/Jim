#include "QRUISound.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Chaos/ChaosEngineInterface.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// Each PlayX helper caches its sound across calls. UE GC keeps the
	// asset alive while the cache pointer is non-null because
	// PlaySound2D / PlaySoundAtLocation hands the reference to the
	// audio component it spawns.
	//
	// Layout-proof: the Fab library lived under /Game/Fabs/<Pack> for a
	// while and now lives at /Game/<Pack> (2026-06-11 re-upload). The
	// loader tries the root layout first, then the legacy Fabs prefix,
	// so either checkout state works.
	USoundBase* LazyLoadSound(const TCHAR* AssetPath, USoundBase*& Cache)
	{
		if (!Cache)
		{
			Cache = LoadObject<USoundBase>(nullptr, AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Cache)
			{
				// Retry with the legacy /Game/Fabs/ prefix.
				FString Legacy(AssetPath);
				if (Legacy.StartsWith(TEXT("/Game/")) && !Legacy.StartsWith(TEXT("/Game/Fabs/")))
				{
					Legacy = TEXT("/Game/Fabs/") + Legacy.RightChop(6);
					Cache = LoadObject<USoundBase>(nullptr, *Legacy, nullptr, LOAD_NoWarn | LOAD_Quiet);
				}
			}
		}
		return Cache;
	}

	// Surface-keyed footstep cue cache. There are 10 surfaces × 5 gaits;
	// we lazy-fill the matrix on first use so the static allocation is
	// trivial and uncached entries silently fall back.
	constexpr int32 NumSurfaces = 10;
	constexpr int32 NumGaits    = 5;
	USoundBase* GFootstepCache[NumSurfaces][NumGaits] = {};

	const TCHAR* SurfaceFolderName(EQRFootSurface S)
	{
		switch (S)
		{
		case EQRFootSurface::Dirt:    return TEXT("Dirt");
		case EQRFootSurface::Glass:   return TEXT("Glass");
		case EQRFootSurface::Gravel:  return TEXT("Gravel");
		case EQRFootSurface::Leaves:  return TEXT("Leaves");
		case EQRFootSurface::Metal:   return TEXT("Metal");
		case EQRFootSurface::Sand:    return TEXT("Sand");
		case EQRFootSurface::Slush:   return TEXT("Slush");
		case EQRFootSurface::Snow:    return TEXT("Snow");
		case EQRFootSurface::Wood:    return TEXT("Wood");
		case EQRFootSurface::Concrete:
		default:                      return TEXT("Concrete");
		}
	}

	// The pack uses a "_Boots_" infix on most surfaces but Glass omits it.
	const TCHAR* SurfaceBootsInfix(EQRFootSurface S)
	{
		return (S == EQRFootSurface::Glass) ? TEXT("") : TEXT("Boots_");
	}

	// Gait base name WITHOUT the numeric variant -- the variant number
	// differs per surface (Concrete ships Run_8/Jog_8/Jump_4/Land_4 but
	// Walk_1; the old "_1 is universal" assumption was wrong, so Run/Jog
	// footsteps silently fell back to the Walk sound). GetFootstepCue
	// now probes a candidate list of numbers and takes the first that
	// resolves.
	const TCHAR* GaitBase(EQRFootGait G)
	{
		switch (G)
		{
		case EQRFootGait::Jog:   return TEXT("Jog");
		case EQRFootGait::Run:   return TEXT("Run");
		case EQRFootGait::Jump:  return TEXT("Jump");
		case EQRFootGait::Land:  return TEXT("Land");
		case EQRFootGait::Walk:
		default:                 return TEXT("Walk");
		}
	}

	USoundBase* GetFootstepCue(EQRFootSurface S, EQRFootGait G)
	{
		const int32 SI = static_cast<int32>(S);
		const int32 GI = static_cast<int32>(G);
		if (SI < 0 || SI >= NumSurfaces || GI < 0 || GI >= NumGaits) return nullptr;
		if (GFootstepCache[SI][GI]) return GFootstepCache[SI][GI];

		// Cue path convention from the Fab pack:
		//   /Game/Essential_Foosteps_SK/CUE/<Surface>/
		//     Footstep_<Surface>_<Boots_?><Gait>_<n>_Cue.<asset>
		// Glass omits the "Boots_" infix; everything else keeps it. The
		// variant number <n> is inconsistent across surfaces, so probe a
		// candidate list and take the first that resolves.
		static const int32 Variants[] = { 1, 8, 4, 2, 3, 5, 6, 7 };
		USoundBase* S2 = nullptr;
		for (int32 N : Variants)
		{
			const FString Stem = FString::Printf(
				TEXT("Footstep_%s_%s%s_%d_Cue"),
				SurfaceFolderName(S), SurfaceBootsInfix(S), GaitBase(G), N);
			const FString Path = FString::Printf(
				TEXT("/Game/Essential_Foosteps_SK/CUE/%s/%s.%s"),
				SurfaceFolderName(S), *Stem, *Stem);
			S2 = LoadObject<USoundBase>(nullptr, *Path);
			if (S2) break;
		}
		GFootstepCache[SI][GI] = S2;

		// Concrete Walk is the universal fallback if the surface-gait
		// combo doesn't exist; populate it lazily so subsequent misses
		// don't keep retrying the disk lookup.
		if (!S2 && !(S == EQRFootSurface::Concrete && G == EQRFootGait::Walk))
		{
			GFootstepCache[SI][GI] = GetFootstepCue(EQRFootSurface::Concrete, EQRFootGait::Walk);
			S2 = GFootstepCache[SI][GI];
		}
		return S2;
	}
}

namespace QRUISound
{
	void PlayClick(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Free_Sounds_Pack/cue/Interface_1-1_Cue.Interface_1-1_Cue"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayConfirm(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Free_Sounds_Pack/cue/Cash_Register_1-2_Cue.Cash_Register_1-2_Cue"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayDeny(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Free_Sounds_Pack/cue/Interface_3-3_Cue.Interface_3-3_Cue"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayWeaponFire(UObject* WC, FVector Location, float VolumeMult)
	{
		static USoundBase* Cached = nullptr;
		// Gunshot_7-1 is the generic shot the pack actually ships (there
		// is no Gunshot_1-1_Cue on disk -- the old path loaded nothing,
		// which is why shooting was silent).
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Free_Sounds_Pack/cue/Gunshot_7-1_Cue.Gunshot_7-1_Cue"), Cached);
		if (S && WC)
		{
			UGameplayStatics::PlaySoundAtLocation(WC, S, Location, VolumeMult);
		}
	}

	void PlayHitImpact(UObject* WC, FVector Location, float VolumeMult)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Free_Sounds_Pack/cue/Hit_Generic_2-1_Cue.Hit_Generic_2-1_Cue"), Cached);
		if (S && WC)
		{
			UGameplayStatics::PlaySoundAtLocation(WC, S, Location, VolumeMult);
		}
	}

	void PlayDeathCry(UObject* WC, FVector Location, float VolumeMult)
	{
		static USoundBase* Cached = nullptr;
		// Creature_1-21 has a deep groan that reads as "downed humanoid"
		// well enough as placeholder; replace per-faction later.
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Free_Sounds_Pack/cue/Creature_1-21_Cue.Creature_1-21_Cue"), Cached);
		if (S && WC)
		{
			UGameplayStatics::PlaySoundAtLocation(WC, S, Location, VolumeMult);
		}
	}

	void PlayFootstep(UObject* WC, FVector Location, EQRFootSurface Surface,
		EQRFootGait Gait, float VolumeMult)
	{
		USoundBase* S = GetFootstepCue(Surface, Gait);
		if (S && WC)
		{
			UGameplayStatics::PlaySoundAtLocation(WC, S, Location, VolumeMult);
		}
	}

	EQRFootSurface SurfaceFromPhysMat(uint8 PhysicalSurfaceType)
	{
		// EPhysicalSurface is a uint8 enum the project author maps in
		// Project Settings → Physics → Physical Surfaces. We follow the
		// convention SurfaceType1=Concrete, 2=Dirt, 3=Glass, …, 10=Wood
		// to line up with our EQRFootSurface order. If the user maps
		// them differently this returns Concrete as a fallback.
		switch (static_cast<EPhysicalSurface>(PhysicalSurfaceType))
		{
		case SurfaceType1:  return EQRFootSurface::Concrete;
		case SurfaceType2:  return EQRFootSurface::Dirt;
		case SurfaceType3:  return EQRFootSurface::Glass;
		case SurfaceType4:  return EQRFootSurface::Gravel;
		case SurfaceType5:  return EQRFootSurface::Leaves;
		case SurfaceType6:  return EQRFootSurface::Metal;
		case SurfaceType7:  return EQRFootSurface::Sand;
		case SurfaceType8:  return EQRFootSurface::Slush;
		case SurfaceType9:  return EQRFootSurface::Snow;
		case SurfaceType10: return EQRFootSurface::Wood;
		default:            return EQRFootSurface::Concrete;
		}
	}
}
