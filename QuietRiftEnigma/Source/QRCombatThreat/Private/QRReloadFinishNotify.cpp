#include "QRReloadFinishNotify.h"
#include "QRWeaponComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UQRReloadFinishNotify::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* /*Animation*/,
	const FAnimNotifyEventReference& /*EventReference*/)
{
	if (!MeshComp) return;
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner || !Owner->HasAuthority()) return;

	UQRWeaponComponent* W = Owner->FindComponentByClass<UQRWeaponComponent>();
	if (!W) return;

	const int32 Resolved = (NewAmmoCount < 0) ? W->MagazineCapacity : NewAmmoCount;
	W->FinishReload(Resolved);
}
