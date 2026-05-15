#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "QRTypes.h"
#include "QRWeatherFXManager.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class UAudioComponent;
class UQRWeatherComponent;


/**
 * One mapping from weather event → Niagara FX + ambient loop. The
 * FX attach to the player's camera so the storm follows them around
 * regardless of world position.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRWeatherFXBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EQRWeatherEvent Event = EQRWeatherEvent::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UNiagaraSystem> FX;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase>     AmbientLoop;
};


/**
 * Drives weather visuals + audio. Drop one in any gameplay level;
 * it locates UQRWeatherComponent on the GameState and subscribes to
 * OnWeatherEventStarted / OnWeatherEventEnded. On a started event,
 * spawns the matching Niagara at the local player camera + plays
 * the ambient loop; on end, despawns.
 *
 * Designer fills the Bindings array with one row per event type
 * (DustStorm / AcidRain / etc.). v1 defaults pull from Fab packs
 * where assets exist; missing assets cleanly skip.
 */
UCLASS(BlueprintType, Blueprintable)
class QUIETRIFTENIGMA_API AQRWeatherFXManager : public AActor
{
	GENERATED_BODY()

public:
	AQRWeatherFXManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QR|Weather")
	TArray<FQRWeatherFXBinding> Bindings;

	UFUNCTION(BlueprintCallable, Category = "QR|Weather")
	void RefreshSubscription();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION()
	void HandleWeatherStarted(EQRWeatherEvent EventType, float Intensity);

	UFUNCTION()
	void HandleWeatherEnded(EQRWeatherEvent EventType);

	UPROPERTY()
	TWeakObjectPtr<UQRWeatherComponent> BoundComp;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveFXComp;

	UPROPERTY()
	TObjectPtr<UAudioComponent> ActiveAudioComp;

	const FQRWeatherFXBinding* FindBinding(EQRWeatherEvent Event) const;
	AActor* LocalPlayerCamera() const;
};
