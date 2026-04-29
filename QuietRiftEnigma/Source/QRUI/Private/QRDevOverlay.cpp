#include "QRDevOverlay.h"
#include "UObject/UnrealType.h"
#include "QRSurvivalComponent.h"
#include "QRInventoryComponent.h"
#include "QRWeaponComponent.h"
#include "QRLeaderComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// Weather and scent components live in the QuietRiftEnigma module which QRUI does not depend on.
// Access them by class name at runtime so no hard compile-time dependency is needed.
// Blueprints can also bind OnWeatherEventStarted / OnScentChanged directly.
#define QR_FIND_COMPONENT_BY_NAME(Actor, ClassName) \
	(UActorComponent*)Actor->FindComponentByClass(FindObject<UClass>(ANY_PACKAGE, TEXT(ClassName)))

void UQRDevOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

#if !UE_BUILD_SHIPPING
	TimeSinceRefresh += InDeltaTime;
	if (RefreshIntervalSeconds <= 0.0f || TimeSinceRefresh >= RefreshIntervalSeconds)
	{
		TimeSinceRefresh = 0.0f;

		APlayerController* PC = GetOwningPlayer();
		if (PC)
			RefreshFromPawn(PC->GetPawnOrSpectator());
	}
#endif
}

void UQRDevOverlay::RefreshFromPawn(APawn* TargetPawn)
{
	if (!TargetPawn) return;

	// ── Survival vitals ──────────────────────────────────────
	if (UQRSurvivalComponent* Surv = TargetPawn->FindComponentByClass<UQRSurvivalComponent>())
	{
		HealthPct       = Surv->GetHealthPercent();
		HungerPct       = Surv->MaxHunger  > 0.0f ? Surv->Hunger  / Surv->MaxHunger  : 0.0f;
		ThirstPct       = Surv->MaxThirst  > 0.0f ? Surv->Thirst  / Surv->MaxThirst  : 0.0f;
		FatiguePct      = Surv->MaxFatigue > 0.0f ? Surv->Fatigue / Surv->MaxFatigue : 0.0f;
		CoreTemperature = Surv->CoreTemperature;
	}

	// ── Encumbrance ──────────────────────────────────────────
	if (UQRInventoryComponent* Inv = TargetPawn->FindComponentByClass<UQRInventoryComponent>())
	{
		const float MaxKg = Inv->MaxCarryWeightKg;
		EncumbranceRatio  = (MaxKg > 0.0f) ? FMath::Clamp(Inv->GetCurrentWeightKg() / MaxKg, 0.0f, 2.0f) : 0.0f;
	}

	// ── Weapon ───────────────────────────────────────────────
	if (UQRWeaponComponent* Wpn = TargetPawn->FindComponentByClass<UQRWeaponComponent>())
	{
		WeaponFouling = Wpn->FoulingFactor;
		bWeaponJammed = Wpn->bIsJammed;
	}

	// ── Scent (runtime lookup — avoids hard dep on QuietRiftEnigma module) ────
	if (UClass* ScentClass = FindObject<UClass>(ANY_PACKAGE, TEXT("QRScentComponent")))
	{
		if (UActorComponent* ScentComp = TargetPawn->FindComponentByClass(ScentClass))
		{
			// Read ScentIntensity via reflection so we don't need the header
			if (FFloatProperty* Prop = CastField<FFloatProperty>(ScentClass->FindPropertyByName(TEXT("ScentIntensity"))))
				ScentIntensity = Prop->GetPropertyValue_InContainer(ScentComp);
		}
	}

	// ── Leader (lives on colony GameState, not the pawn) ─────
	if (AGameStateBase* GS = UGameplayStatics::GetGameState(TargetPawn))
	{
		if (UQRLeaderComponent* Leader = GS->FindComponentByClass<UQRLeaderComponent>())
		{
			MoraleIndex          = Leader->MoraleIndex;
			LeaderBuff           = Leader->LeaderBuff;
			LeaderLevel          = Leader->LeaderLevel;
			IssueEscalationScore = Leader->IssueEscalationScore;
		}

		// Weather (runtime lookup)
		if (UClass* WeatherClass = FindObject<UClass>(ANY_PACKAGE, TEXT("QRWeatherComponent")))
		{
			if (UActorComponent* WeatherComp = GS->FindComponentByClass(WeatherClass))
			{
				if (FByteProperty* EProp = CastField<FByteProperty>(WeatherClass->FindPropertyByName(TEXT("ActiveWeatherEvent"))))
					ActiveWeather = static_cast<EQRWeatherEvent>(EProp->GetPropertyValue_InContainer(WeatherComp));
				if (FFloatProperty* IProp = CastField<FFloatProperty>(WeatherClass->FindPropertyByName(TEXT("EventIntensity"))))
					WeatherIntensity = IProp->GetPropertyValue_InContainer(WeatherComp);
			}
		}
	}
}

void UQRDevOverlay::ToggleOverlayVisibility()
{
	bVisible = !bVisible;
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
