#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRNPCActor.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;
class UQRDialogueComponent;
class UQRFactionComponent;

/**
 * Minimal NPC actor: capsule + skeletal mesh + dialogue + faction
 * components. Designer drops one in a level, picks a mesh (Mannequin /
 * FuturisticWarrior / etc.), assigns a DialogueTable + StartNodeId on
 * the Dialogue component, and the existing F-interact path in
 * AQRCharacter::Server_Interact auto-starts the conversation through
 * UQRDialogueComponent's reflective lookup.
 *
 * No AI / behavior tree — this is a stationary talkable NPC for v1.
 * Wildlife / hostile AI lives in QRColonyAI and is its own thing.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRNPCActor : public AActor
{
	GENERATED_BODY()

public:
	AQRNPCActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|NPC")
	TObjectPtr<UCapsuleComponent> CapsuleComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|NPC")
	TObjectPtr<USkeletalMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|NPC")
	TObjectPtr<UQRDialogueComponent> Dialogue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|NPC")
	TObjectPtr<UQRFactionComponent> Faction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC")
	FText DisplayName;
};


/**
 * Level-placed spawner. On BeginPlay, spawns N copies of
 * NPCClass at the configured offsets and sets DisplayName.
 *
 * Designer use: drag into a level, set NPCClass to AQRNPCActor (or a
 * subclass with mesh/dialogue defaults filled in), tweak NumToSpawn
 * and Radius. Wildlife spawners use the same pattern with the
 * Wildlife actor swapped in.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRNPCSpawner : public AActor
{
	GENERATED_BODY()

public:
	AQRNPCSpawner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner")
	TSubclassOf<AQRNPCActor> NPCClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner",
		meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumToSpawn = 1;

	// Random scatter around the spawner location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner",
		meta = (ClampMin = "0", ClampMax = "5000"))
	float Radius = 250.0f;

	// Optional override list of display names — one per spawned NPC.
	// If shorter than NumToSpawn, leftovers use the actor default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Spawner")
	TArray<FText> DisplayNames;

	virtual void BeginPlay() override;
};
