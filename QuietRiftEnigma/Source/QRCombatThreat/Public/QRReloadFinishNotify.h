#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "QRReloadFinishNotify.generated.h"

/**
 * AnimNotify designer drops at the end of a reload montage so the C++
 * weapon component finishes the reload on the exact animation frame
 * instead of via the existing duration timer fallback.
 *
 * Finds the owning actor's UQRWeaponComponent and calls FinishReload
 * with the new magazine count. NewAmmoCount defaults to -1 which means
 * "fill to MagazineCapacity"; designer can hand-set a value for
 * partial-reload anims.
 */
UCLASS(meta = (DisplayName = "QR Reload Finish"))
class QRCOMBATTHREAT_API UQRReloadFinishNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	// -1 = fill the magazine to MagazineCapacity. Set to a positive
	// integer for partial-reload anims that should leave a known count.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Reload")
	int32 NewAmmoCount = -1;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
