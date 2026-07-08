#pragma once
#include "QRWildlifeBase.h"
#include "QRWildlife_ColonyDog.generated.h"

// ANM_COLONY_DOG_001 — German Shepherd, colony companion animal.
//
// The only Earth fauna in the game: working dogs shipped frozen-embryo
// on the colony ark and raised planetside. Lives around settlements,
// not in the wild -- qr_spawn_starter_village places a couple with the
// colonists. Fully skinned out of the box: the German_Shepherd_3D_Model
// Fab pack ships mesh + idle/walk/run loops on its own skeleton, wired
// here through the base class's single-node anim slots (the first
// species to exercise that path -- everything else still uses the
// shape-coded placeholder until its pack lands).
//
// Ambient role: never attacks, flees briefly when hurt, and barks are
// a future hook (Free_Sounds_Pack). No death drops -- shooting the
// colony dog gets you nothing but regret.
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWildlife_ColonyDog : public AQRWildlifeBase
{
	GENERATED_BODY()
public:
	AQRWildlife_ColonyDog();

	virtual void OnThreatDetected_Implementation(AActor* Threat) override;
};
