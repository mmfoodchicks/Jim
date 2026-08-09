#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRNPCActor.generated.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UCapsuleComponent;
class UQRDialogueComponent;
class UQRFactionComponent;
class UQRNPCBrainComponent;
class UQRCivilianReactionComponent;
class UQRSurvivalComponent;

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

	// Living-NPC layer. Brain drives wander/work/sleep loop; Reaction
	// owns Flee/Fight/Hide during raids and out-prioritizes the brain.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|NPC")
	TObjectPtr<UQRNPCBrainComponent> Brain;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|NPC")
	TObjectPtr<UQRCivilianReactionComponent> Reaction;

	// Health model. Weapons apply damage through this (the weapon
	// component looks it up first), raider melee hits it, medics heal
	// through it. NPCs had NO health sink at all before — every NPC and
	// raider was silently unkillable.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QR|NPC")
	TObjectPtr<UQRSurvivalComponent> Survival;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC")
	FText DisplayName;

	// Corpse lifetime after death before the actor is destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC",
		meta = (ClampMin = "1", ClampMax = "600"))
	float CorpseDespawnSeconds = 30.0f;

	// Engine damage (bullets via ApplyPointDamage fallback, hazards,
	// explosions) routes into Survival — same pattern as AQRCharacter.
	virtual float TakeDamage(float DamageAmount, const struct FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// Soft pointer assigned by qr_assign_npc_appearance.py or a designer
	// BP subclass. Loaded on BeginPlay if MeshComp's slot is still empty.
	// Defaults to None so a designer-authored BP subclass with an
	// inline mesh isn't stomped on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|NPC|Appearance")
	TSoftObjectPtr<USkeletalMesh> DefaultSkeletalMesh;

protected:
	virtual void BeginPlay() override;

	// Bound to Survival->OnDeath: freezes the AI, poses the corpse,
	// drops blocking collision, schedules despawn.
	UFUNCTION()
	void HandleDied();
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
