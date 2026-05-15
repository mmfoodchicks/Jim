#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRHaulerComponent.generated.h"

class AQRDepotActor;
class AActor;
class UQRItemDefinition;
class UQRItemInstance;


/**
 * Hauler FSM state. Drives the tick-based routing.
 */
UENUM(BlueprintType)
enum class EQRHaulerState : uint8
{
	Idle               UMETA(DisplayName = "Idle"),
	SeekDemand         UMETA(DisplayName = "Seek Demand"),
	MoveToDepot        UMETA(DisplayName = "Move to Depot"),
	PickUp             UMETA(DisplayName = "Pick Up"),
	MoveToStation      UMETA(DisplayName = "Move to Station"),
	Deliver            UMETA(DisplayName = "Deliver"),
};


/**
 * Lightweight hauler AI — attach to any AQRNPCActor (or any actor with
 * a UQRInventoryComponent) and they'll move materials between depots
 * and crafting stations.
 *
 * Tick-based state machine (no behavior tree asset needed):
 *
 *   Idle           → SeekDemand after IdleSeconds
 *   SeekDemand     → searches the world for crafting stations whose
 *                    component reports stalled ingredients, picks one,
 *                    finds nearest depot with the item, transitions to
 *                    MoveToDepot. If nothing found, back to Idle.
 *   MoveToDepot    → walks toward depot. Arrives within InteractRange.
 *   PickUp         → transfers up to CarryCapacity items into self.
 *                    Transitions to MoveToStation.
 *   MoveToStation  → walks toward target station.
 *   Deliver        → transfers items into station's inventory.
 *                    Transitions to Idle.
 *
 * Movement is straight-line — no nav-mesh pathing in v1. Designer
 * keeps depots + stations on open ground. NavMesh pathfinding is a
 * follow-up once the worldgen produces nav volumes.
 *
 * This is the "Master GDD §9 Logistics" backbone the colony needs to
 * feel alive. With even a single hauler tasked, materials flow from
 * the stockpile into stations and recipes complete without the player
 * personally couriering everything.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRHaulerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRHaulerComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Hauler",
		meta = (ClampMin = "50", ClampMax = "1000"))
	float MoveSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Hauler",
		meta = (ClampMin = "50", ClampMax = "500"))
	float InteractRange = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Hauler",
		meta = (ClampMin = "1", ClampMax = "100"))
	int32 CarryCapacity = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Hauler",
		meta = (ClampMin = "0.5", ClampMax = "30"))
	float IdleSeconds = 3.0f;

	// Max search radius (cm) when looking for depots / stations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Hauler",
		meta = (ClampMin = "500", ClampMax = "50000"))
	float SearchRadiusCm = 8000.0f;

	UPROPERTY(BlueprintReadOnly, Category = "QR|Hauler|State")
	EQRHaulerState State = EQRHaulerState::Idle;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	float StateTimer = 0.0f;
	FName ActiveItemId;
	int32 ActiveQuantity = 0;
	TWeakObjectPtr<AQRDepotActor> ActiveDepot;
	TWeakObjectPtr<AActor>        ActiveStation;

	void SetState(EQRHaulerState NewState);
	void TickIdle(float DeltaTime);
	void TickSeekDemand();
	void TickMoveTo(AActor* Target, EQRHaulerState OnArrivedState, float DeltaTime);
	void TickPickUp();
	void TickDeliver();

	bool MoveToward(const FVector& Destination, float DeltaTime);
	AActor* FindStationWithDemand(FName& OutItemId, int32& OutQuantity) const;
	AQRDepotActor* FindDepotWithItem(FName ItemId, int32 MinQuantity) const;
};
