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
	USoundBase* LazyLoadSound(const TCHAR* AssetPath, USoundBase*& Cache)
	{
		if (!Cache)
		{
			Cache = LoadObject<USoundBase>(nullptr, AssetPath);
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

	// Many cues have a numeric suffix per surface. We pick "_1_Cue" as the
	// canonical variant because every surface ships at least one; some
	// have _8_Cue (Concrete Jog / Run, Gravel Run) but _1_ is universal.
	const TCHAR* GaitInfix(EQRFootGait G)
	{
		switch (G)
		{
		case EQRFootGait::Jog:   return TEXT("Jog_1");
		case EQRFootGait::Run:   return TEXT("Run_1");
		case EQRFootGait::Jump:  return TEXT("Jump_1");
		case EQRFootGait::Land:  return TEXT("Land_1");
		case EQRFootGait::Walk:
		default:                 return TEXT("Walk_1");
		}
	}

	USoundBase* GetFootstepCue(EQRFootSurface S, EQRFootGait G)
	{
		const int32 SI = static_cast<int32>(S);
		const int32 GI = static_cast<int32>(G);
		if (SI < 0 || SI >= NumSurfaces || GI < 0 || GI >= NumGaits) return nullptr;
		if (GFootstepCache[SI][GI]) return GFootstepCache[SI][GI];

		// Cue path convention from the Fab pack:
		//   /Game/Fabs/Essential_Foosteps_SK/CUE/<Surface>/
		//     Footstep_<Surface>_<Boots_?><Gait>_<n>_Cue.<asset>
		// Glass omits the "Boots_" infix; everything else keeps it.
		const FString Path = FString::Printf(
			TEXT("/Game/Fabs/Essential_Foosteps_SK/CUE/%s/Footstep_%s_%s%s_Cue.Footstep_%s_%s%s_Cue"),
			SurfaceFolderName(S),
			SurfaceFolderName(S),
			SurfaceBootsInfix(S),
			GaitInfix(G),
			SurfaceFolderName(S),
			SurfaceBootsInfix(S),
			GaitInfix(G));

		USoundBase* S2 = LoadObject<USoundBase>(nullptr, *Path);
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
			TEXT("/Game/Fabs/Free_Sounds_Pack/cue/Interface_1-1_Cue.Interface_1-1_Cue"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayConfirm(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Fabs/Free_Sounds_Pack/cue/Cash_Register_1-2_Cue.Cash_Register_1-2_Cue"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayDeny(UObject* WC)
	{
		static USoundBase* Cached = nullptr;
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Fabs/Free_Sounds_Pack/cue/Interface_3-3_Cue.Interface_3-3_Cue"), Cached);
		if (S && WC) UGameplayStatics::PlaySound2D(WC, S);
	}

	void PlayWeaponFire(UObject* WC, FVector Location, float VolumeMult)
	{
		static USoundBase* Cached = nullptr;
		// Gunshot_1-1 is the cleanest generic shot in the pack; per-
		// weapon SFX can be slotted on the component to override.
		USoundBase* S = LazyLoadSound(
			TEXT("/Game/Fabs/Free_Sounds_Pack/cue/Gunshot_1-1_Cue.Gunshot_1-1_Cue"), Cached);
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
