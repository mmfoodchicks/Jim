#include "QRWildlifeBase.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

AQRWildlifeBase::AQRWildlifeBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AQRWildlifeBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AQRWildlifeBase, CurrentHealth);
	DOREPLIFETIME(AQRWildlifeBase, AIState);
	DOREPLIFETIME(AQRWildlifeBase, bIsDead);
	DOREPLIFETIME(AQRWildlifeBase, HerdGroupId);
}

void AQRWildlifeBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;

	if (HasAuthority())
	{
		// Launch behavior tree via AI controller
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			if (BehaviorTree)
				AIC->RunBehaviorTree(BehaviorTree);
		}
	}
}

void AQRWildlifeBase::TakeDamage_Wildlife(float Amount, AActor* DamageCauser)
{
	if (!HasAuthority() || bIsDead || Amount <= 0.0f) return;

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Amount);

	if (CurrentHealth <= 0.0f)
		OnDied(DamageCauser);
	else if (AIState != EQRWildlifeAIState::Fleeing_Injured && CurrentHealth / MaxHealth < 0.3f)
		SetAIState(EQRWildlifeAIState::Fleeing_Injured);
}

void AQRWildlifeBase::SetAIState(EQRWildlifeAIState NewState)
{
	if (AIState == NewState) return;
	AIState = NewState;

	float NewSpeed = MoveSpeedWalk;
	switch (NewState)
	{
	case EQRWildlifeAIState::Fleeing:
	case EQRWildlifeAIState::Fleeing_Injured:
		NewSpeed = MoveSpeedFlee;
		break;
	case EQRWildlifeAIState::Charging:
	case EQRWildlifeAIState::Attacking:
		NewSpeed = MoveSpeedCharge;
		break;
	default:
		NewSpeed = MoveSpeedWalk;
		break;
	}
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;

	// Propagate to BT blackboard via AI controller
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
			BB->SetValueAsEnum(FName("AIState"), static_cast<uint8>(NewState));
	}
}

void AQRWildlifeBase::AlertHerd(AActor* Threat)
{
	if (HerdGroupId == 0 || !GetWorld()) return;

	// Find all wildlife in same herd group and alert them
	for (TActorIterator<AQRWildlifeBase> It(GetWorld()); It; ++It)
	{
		AQRWildlifeBase* Other = *It;
		if (Other && Other != this && Other->HerdGroupId == HerdGroupId && !Other->bIsDead)
			Other->OnThreatDetected(Threat);
	}
}

TArray<FQRWildlifeDrop> AQRWildlifeBase::Harvest()
{
	TArray<FQRWildlifeDrop> Result;
	TArray<FQRWildlifeDrop>& Source = bIsDead ? DeathDrops : HarvestDrops;

	for (const FQRWildlifeDrop& Drop : Source)
	{
		if (FMath::FRand() <= Drop.DropChance)
		{
			FQRWildlifeDrop Actual = Drop;
			Actual.MinQuantity = FMath::RandRange(Drop.MinQuantity, Drop.MaxQuantity);
			Actual.MaxQuantity = Actual.MinQuantity;
			Result.Add(Actual);
		}
	}

	return Result;
}

void AQRWildlifeBase::OnDied_Implementation(AActor* Killer)
{
	bIsDead = true;
	SetAIState(EQRWildlifeAIState::Dead);

	// Ragdoll
	GetMesh()->SetSimulatePhysics(true);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Detach AI controller
	if (AAIController* AIC = Cast<AAIController>(GetController()))
		AIC->UnPossess();

	// TODO: Start carcass despawn timer
}
