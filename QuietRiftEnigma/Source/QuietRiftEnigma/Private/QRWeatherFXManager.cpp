#include "QRWeatherFXManager.h"
#include "QRWeatherComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"


AQRWeatherFXManager::AQRWeatherFXManager()
{
	PrimaryActorTick.bCanEverTick = false;
}


void AQRWeatherFXManager::BeginPlay()
{
	Super::BeginPlay();
	RefreshSubscription();
}


void AQRWeatherFXManager::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UQRWeatherComponent* Comp = BoundComp.Get())
	{
		Comp->OnWeatherEventStarted.RemoveDynamic(this, &AQRWeatherFXManager::HandleWeatherStarted);
		Comp->OnWeatherEventEnded.RemoveDynamic(this, &AQRWeatherFXManager::HandleWeatherEnded);
	}
	if (ActiveFXComp)    { ActiveFXComp->DestroyComponent();    ActiveFXComp = nullptr; }
	if (ActiveAudioComp) { ActiveAudioComp->DestroyComponent(); ActiveAudioComp = nullptr; }
	Super::EndPlay(Reason);
}


void AQRWeatherFXManager::RefreshSubscription()
{
	if (UQRWeatherComponent* Old = BoundComp.Get())
	{
		Old->OnWeatherEventStarted.RemoveDynamic(this, &AQRWeatherFXManager::HandleWeatherStarted);
		Old->OnWeatherEventEnded.RemoveDynamic(this, &AQRWeatherFXManager::HandleWeatherEnded);
	}

	BoundComp = nullptr;
	UWorld* W = GetWorld();
	if (!W) return;
	AGameStateBase* GS = W->GetGameState();
	if (!GS) return;
	UQRWeatherComponent* Comp = GS->FindComponentByClass<UQRWeatherComponent>();
	if (!Comp) return;

	Comp->OnWeatherEventStarted.AddDynamic(this, &AQRWeatherFXManager::HandleWeatherStarted);
	Comp->OnWeatherEventEnded.AddDynamic(this, &AQRWeatherFXManager::HandleWeatherEnded);
	BoundComp = Comp;
}


const FQRWeatherFXBinding* AQRWeatherFXManager::FindBinding(EQRWeatherEvent Event) const
{
	for (const FQRWeatherFXBinding& B : Bindings)
	{
		if (B.Event == Event) return &B;
	}
	return nullptr;
}


AActor* AQRWeatherFXManager::LocalPlayerCamera() const
{
	if (UWorld* W = GetWorld())
	{
		if (APlayerController* PC = W->GetFirstPlayerController())
		{
			return PC->GetPawn();
		}
	}
	return nullptr;
}


void AQRWeatherFXManager::HandleWeatherStarted(EQRWeatherEvent EventType, float Intensity)
{
	// Tear down previous FX (in case events overlap somehow).
	HandleWeatherEnded(EventType);

	const FQRWeatherFXBinding* B = FindBinding(EventType);
	if (!B) return;
	AActor* PlayerActor = LocalPlayerCamera();
	if (!PlayerActor) return;

	if (UNiagaraSystem* Sys = B->FX.LoadSynchronous())
	{
		ActiveFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			Sys, PlayerActor->GetRootComponent(), NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, /*bAutoDestroy*/ false);
		if (ActiveFXComp)
		{
			// Scale intensity into the spawn rate via a Niagara user
			// param if it exists ("UserIntensity") — designer can wire
			// it inside the system. Missing param = no-op.
			ActiveFXComp->SetVariableFloat(TEXT("User.Intensity"), Intensity);
		}
	}

	if (USoundBase* Snd = B->AmbientLoop.LoadSynchronous())
	{
		ActiveAudioComp = UGameplayStatics::SpawnSoundAttached(
			Snd, PlayerActor->GetRootComponent(),
			NAME_None, FVector::ZeroVector, EAttachLocation::SnapToTarget,
			/*bStopWhenAttachedToDestroyed*/ true);
	}
}


void AQRWeatherFXManager::HandleWeatherEnded(EQRWeatherEvent /*EventType*/)
{
	if (ActiveFXComp)
	{
		ActiveFXComp->Deactivate();
		ActiveFXComp->DestroyComponent();
		ActiveFXComp = nullptr;
	}
	if (ActiveAudioComp)
	{
		ActiveAudioComp->Stop();
		ActiveAudioComp->DestroyComponent();
		ActiveAudioComp = nullptr;
	}
}
