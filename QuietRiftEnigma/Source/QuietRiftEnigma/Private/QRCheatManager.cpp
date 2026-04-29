#include "QRCheatManager.h"
#include "QRCharacter.h"
#include "QRGameMode.h"
#include "QRSurvivalComponent.h"
#include "QRInventoryComponent.h"
#include "QRItemDefinition.h"
#include "QRColonyStateComponent.h"
#include "QRResearchComponent.h"
#include "QRRaidScheduler.h"
#include "QRGameplayTags.h"
#include "QRWeatherComponent.h"
#include "QRScentComponent.h"
#include "QRWeaponComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

// Helper: find the local player's pawn cast to AQRCharacter, or nullptr
static AQRCharacter* GetLocalCharacter(APlayerController* PC)
{
	return PC ? Cast<AQRCharacter>(PC->GetPawn()) : nullptr;
}

void UQRCheatManager::QRGiveItem(const FString& ItemId, int32 Quantity)
{
#if !UE_BUILD_SHIPPING
	APlayerController* PC  = GetPlayerController();
	AQRCharacter*      Chr = GetLocalCharacter(PC);
	if (!Chr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRCheat] QRGiveItem: no local character"));
		return;
	}

	UQRInventoryComponent* Inv = Chr->FindComponentByClass<UQRInventoryComponent>();
	if (!Inv)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRCheat] QRGiveItem: character has no UQRInventoryComponent"));
		return;
	}

	const int32 SafeQty = FMath::Clamp(Quantity, 1, 9999);
	EQRInventoryResult Result = Inv->TryAddByDefinition(FName(*ItemId), SafeQty);
	UE_LOG(LogTemp, Log, TEXT("[QRCheat] GiveItem %s x%d → result %d"), *ItemId, SafeQty, (int32)Result);
#endif
}

void UQRCheatManager::QRSetMorale(float Value)
{
#if !UE_BUILD_SHIPPING
	if (AGameStateBase* GS = UGameplayStatics::GetGameState(GetPlayerController()))
	{
		if (UQRColonyStateComponent* Colony = GS->FindComponentByClass<UQRColonyStateComponent>())
		{
			Colony->ColonyMorale = FMath::Clamp(Value, 0.0f, 100.0f);
			UE_LOG(LogTemp, Log, TEXT("[QRCheat] SetMorale → %.1f"), Colony->ColonyMorale);
		}
	}
#endif
}

void UQRCheatManager::QRForceRaid()
{
#if !UE_BUILD_SHIPPING
	// AQRRaidScheduler is an actor placed in the level — find the first one and trigger it
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetPlayerController(), AQRRaidScheduler::StaticClass(), Found);
	if (Found.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRCheat] No AQRRaidScheduler in world — place one in your level"));
		return;
	}

	if (AQRRaidScheduler* Sched = Cast<AQRRaidScheduler>(Found[0]))
	{
		Sched->TriggerRaid(QRGameplayTags::Faction_Hostile_Generic, EQRRaidExperienceTier::Competent);
		UE_LOG(LogTemp, Log, TEXT("[QRCheat] ForceRaid triggered (Hostile.Generic / Competent tier)"));
	}
#endif
}

void UQRCheatManager::QRUnlockTech(const FString& TechNodeId)
{
#if !UE_BUILD_SHIPPING
	if (AGameStateBase* GS = UGameplayStatics::GetGameState(GetPlayerController()))
	{
		if (UQRResearchComponent* Research = GS->FindComponentByClass<UQRResearchComponent>())
		{
			Research->ForceUnlockTechNode(FName(*TechNodeId));
			UE_LOG(LogTemp, Log, TEXT("[QRCheat] UnlockTech %s"), *TechNodeId);
		}
	}
#endif
}

void UQRCheatManager::QRSetHealth(float Value)
{
#if !UE_BUILD_SHIPPING
	AQRCharacter* Chr = GetLocalCharacter(GetPlayerController());
	if (!Chr) return;
	if (UQRSurvivalComponent* Surv = Chr->FindComponentByClass<UQRSurvivalComponent>())
	{
		const float Clamped = FMath::Clamp(Value, 0.0f, Surv->MaxHealth);
		Surv->Health = Clamped;
		UE_LOG(LogTemp, Log, TEXT("[QRCheat] SetHealth → %.1f"), Clamped);
	}
#endif
}

void UQRCheatManager::QRKillAllInjuries()
{
#if !UE_BUILD_SHIPPING
	AQRCharacter* Chr = GetLocalCharacter(GetPlayerController());
	if (!Chr) return;
	if (UQRSurvivalComponent* Surv = Chr->FindComponentByClass<UQRSurvivalComponent>())
	{
		Surv->ActiveInjuries.Empty();
		UE_LOG(LogTemp, Log, TEXT("[QRCheat] All injuries cleared"));
	}
#endif
}

void UQRCheatManager::QRSetWeather(const FString& WeatherType, float Intensity, float DurationHours)
{
#if !UE_BUILD_SHIPPING
	// Find the weather component anywhere in the world (lives on GameState or a world actor)
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetPlayerController(), AActor::StaticClass(), Actors);
	for (AActor* A : Actors)
	{
		UQRWeatherComponent* W = A->FindComponentByClass<UQRWeatherComponent>();
		if (!W) continue;

		static const TMap<FString, EQRWeatherEvent> EventMap =
		{
			{ TEXT("DustStorm"),     EQRWeatherEvent::DustStorm     },
			{ TEXT("AcidRain"),      EQRWeatherEvent::AcidRain      },
			{ TEXT("VentEruption"),  EQRWeatherEvent::VentEruption  },
			{ TEXT("HeatWave"),      EQRWeatherEvent::HeatWave      },
			{ TEXT("IceFog"),        EQRWeatherEvent::IceFog        },
			{ TEXT("MagneticStorm"), EQRWeatherEvent::MagneticStorm },
		};

		const EQRWeatherEvent* Found = EventMap.Find(WeatherType);
		if (!Found)
		{
			UE_LOG(LogTemp, Warning, TEXT("[QRCheat] Unknown weather type '%s'. Valid: DustStorm AcidRain VentEruption HeatWave IceFog MagneticStorm"), *WeatherType);
			return;
		}

		W->TriggerWeatherEvent(*Found, FMath::Clamp(Intensity, 0.0f, 1.0f), FMath::Max(DurationHours, 0.1f));
		UE_LOG(LogTemp, Log, TEXT("[QRCheat] SetWeather %s intensity=%.2f duration=%.1fh"), *WeatherType, Intensity, DurationHours);
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("[QRCheat] No UQRWeatherComponent found in world"));
#endif
}

void UQRCheatManager::QRClearWeather()
{
#if !UE_BUILD_SHIPPING
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetPlayerController(), AActor::StaticClass(), Actors);
	for (AActor* A : Actors)
	{
		if (UQRWeatherComponent* W = A->FindComponentByClass<UQRWeatherComponent>())
		{
			W->EndWeatherEvent();
			UE_LOG(LogTemp, Log, TEXT("[QRCheat] Weather cleared"));
			return;
		}
	}
#endif
}

void UQRCheatManager::QRClearScent()
{
#if !UE_BUILD_SHIPPING
	AQRCharacter* Chr = GetLocalCharacter(GetPlayerController());
	if (!Chr) return;
	if (UQRScentComponent* Scent = Chr->FindComponentByClass<UQRScentComponent>())
	{
		Scent->ClearScent();
		UE_LOG(LogTemp, Log, TEXT("[QRCheat] Scent cleared"));
	}
#endif
}

void UQRCheatManager::QRPrintVitals()
{
#if !UE_BUILD_SHIPPING
	AQRCharacter* Chr = GetLocalCharacter(GetPlayerController());
	if (!Chr) { UE_LOG(LogTemp, Warning, TEXT("[QRCheat] No character")); return; }

	if (UQRSurvivalComponent* Surv = Chr->FindComponentByClass<UQRSurvivalComponent>())
	{
		const FString Msg = FString::Printf(
			TEXT("[QRVitals] HP=%.1f/%.1f  Hunger=%.1f  Thirst=%.1f  Fatigue=%.1f  Temp=%.1fC  Dead=%d  Injuries=%d"),
			Surv->Health, Surv->MaxHealth, Surv->Hunger, Surv->Thirst, Surv->Fatigue,
			Surv->CoreTemperature, Surv->bIsDead ? 1 : 0, Surv->ActiveInjuries.Num()
		);
		UE_LOG(LogTemp, Log, TEXT("%s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Cyan, Msg);
	}
#endif
}
