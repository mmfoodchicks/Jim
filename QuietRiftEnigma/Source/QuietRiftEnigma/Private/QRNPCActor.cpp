#include "QRNPCActor.h"
#include "QRDialogueComponent.h"
#include "QRFactionComponent.h"
#include "QRNPCBrainComponent.h"
#include "QRCivilianReactionComponent.h"
#include "QRSurvivalComponent.h"
#include "QRRaidPartyAI.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "TimerManager.h"

AQRNPCActor::AQRNPCActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(CapsuleComp);
	CapsuleComp->InitCapsuleSize(40.0f, 90.0f);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));

	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(CapsuleComp);
	MeshComp->SetRelativeLocation(FVector(0, 0, -90.0f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Dialogue = CreateDefaultSubobject<UQRDialogueComponent>(TEXT("Dialogue"));
	Faction  = CreateDefaultSubobject<UQRFactionComponent>(TEXT("Faction"));
	Brain    = CreateDefaultSubobject<UQRNPCBrainComponent>(TEXT("Brain"));
	Reaction = CreateDefaultSubobject<UQRCivilianReactionComponent>(TEXT("Reaction"));
	Survival = CreateDefaultSubobject<UQRSurvivalComponent>(TEXT("Survival"));
	// NPCs use Survival as a pure health pool — no self-feeding loop
	// exists yet, so metabolic drains would silently starve the whole
	// village over a long session.
	Survival->HungerDrainPerSecond  = 0.0f;
	Survival->ThirstDrainPerSecond  = 0.0f;
	Survival->FatigueDrainPerSecond = 0.0f;

	DisplayName = FText::FromString(TEXT("Survivor"));
}


void AQRNPCActor::BeginPlay()
{
	// Resolve the default skeletal mesh BEFORE Super::BeginPlay so the
	// brain's BeginPlay (which looks up the SMC and starts the idle
	// animation) sees the mesh already assigned. Otherwise the brain
	// would force single-node mode on an empty mesh and the first frame
	// after BeginPlay would T-pose until Tick caught up.
	if (MeshComp && !MeshComp->GetSkeletalMeshAsset() && !DefaultSkeletalMesh.IsNull())
	{
		if (USkeletalMesh* Mesh = DefaultSkeletalMesh.LoadSynchronous())
		{
			MeshComp->SetSkeletalMesh(Mesh);
		}
	}
	Super::BeginPlay();

	if (Survival)
	{
		Survival->OnDeath.AddDynamic(this, &AQRNPCActor::HandleDied);
	}
}

float AQRNPCActor::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	const float Applied = Super::TakeDamage(DamageAmount, DamageEvent,
		EventInstigator, DamageCauser);
	if (HasAuthority() && Survival && DamageAmount > 0.0f)
	{
		Survival->ApplyDamage(DamageAmount);
	}
	return Applied;
}

void AQRNPCActor::HandleDied()
{
	// Freeze every AI layer so the corpse stops wandering / raiding.
	if (Brain)    Brain->SetComponentTickEnabled(false);
	if (Reaction) Reaction->SetComponentTickEnabled(false);
	if (UQRRaidPartyAI* RaidAI = FindComponentByClass<UQRRaidPartyAI>())
	{
		RaidAI->SetComponentTickEnabled(false);
	}

	// Corpse: keep it targetable by the interact trace but stop it from
	// standing as an invisible pawn-blocking wall.
	if (CapsuleComp)
	{
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// Death pose from the brain's authored slot; freeze the current pose
	// when no clip is assigned.
	if (MeshComp)
	{
		UAnimSequence* Death = (Brain ? Brain->DeathAnim.LoadSynchronous() : nullptr);
		if (Death)
		{
			MeshComp->PlayAnimation(Death, /*bLooping*/ false);
		}
		else
		{
			MeshComp->bPauseAnims = true;
		}
	}

	if (UWorld* W = GetWorld())
	{
		FTimerHandle H;
		W->GetTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsValid(this)) Destroy();
		}), FMath::Max(CorpseDespawnSeconds, 1.0f), false);
	}
}


AQRNPCSpawner::AQRNPCSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	NPCClass = AQRNPCActor::StaticClass();
}

void AQRNPCSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority() || !NPCClass) return;

	UWorld* W = GetWorld();
	if (!W) return;

	const FVector Center = GetActorLocation();

	for (int32 i = 0; i < NumToSpawn; ++i)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Dist  = FMath::FRandRange(0.0f, Radius);
		const FVector Loc = Center + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0);

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AQRNPCActor* NPC = W->SpawnActor<AQRNPCActor>(NPCClass, Loc, GetActorRotation(), Params);
		if (NPC && DisplayNames.IsValidIndex(i))
		{
			NPC->DisplayName = DisplayNames[i];
			if (NPC->Dialogue) NPC->Dialogue->DefaultSpeakerName = DisplayNames[i];
		}
	}
}
