#include "QRSurvivalComponent.h"
#include "QRItemInstance.h"
#include "QRItemDefinition.h"
#include "QRGameplayTags.h"
#include "QRCoreSettings.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

UQRSurvivalComponent::UQRSurvivalComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f;
	SetIsReplicatedByDefault(true);
}

void UQRSurvivalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRSurvivalComponent, Health);
	DOREPLIFETIME(UQRSurvivalComponent, Hunger);
	DOREPLIFETIME(UQRSurvivalComponent, Thirst);
	DOREPLIFETIME(UQRSurvivalComponent, Fatigue);
	DOREPLIFETIME(UQRSurvivalComponent, CoreTemperature);
	DOREPLIFETIME(UQRSurvivalComponent, Oxygen);
	DOREPLIFETIME(UQRSurvivalComponent, ActiveInjuries);
	DOREPLIFETIME(UQRSurvivalComponent, ActiveStatusTags);
	DOREPLIFETIME(UQRSurvivalComponent, bIsDead);
}

void UQRSurvivalComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority() || bIsDead) return;

	DrainNeeds(DeltaTime);
	ApplyNeedDamage(DeltaTime);
	TickInjuries(DeltaTime);
	RefreshStatusTags();
}

void UQRSurvivalComponent::DrainNeeds(float DeltaTime)
{
	Hunger  = FMath::Max(0.0f, Hunger  - HungerDrainPerSecond  * DeltaTime);
	Thirst  = FMath::Max(0.0f, Thirst  - ThirstDrainPerSecond  * DeltaTime);

	// Sprint burns fatigue faster — multiplier read from config so designers
	// can retune without code changes. Owner state is reflected (we don't
	// hard-link to AQRCharacter so any sprinting subclass works).
	const float FatigueMult = QueryOwnerIsSprinting() ? SprintFatigueDrainMultiplier : 1.0f;
	Fatigue = FMath::Max(0.0f, Fatigue - FatigueDrainPerSecond * FatigueMult * DeltaTime);

	// Oxygen drains in environments that have OxygenDrainPerSecond > 0.
	// Triggers / volumes / suit gear should set this on entry/exit.
	if (OxygenDrainPerSecond > 0.0f)
	{
		Oxygen = FMath::Max(0.0f, Oxygen - OxygenDrainPerSecond * DeltaTime);
	}
}

void UQRSurvivalComponent::ApplyNeedDamage(float DeltaTime)
{
	float Dmg = 0.0f;
	if (IsStarving())    Dmg += 0.2f * DeltaTime;
	if (IsDehydrated())  Dmg += 0.4f * DeltaTime;
	if (IsExhausted())   Dmg += 0.05f * DeltaTime;

	// Hypothermia (cold)
	if (CoreTemperature < 35.0f)
		Dmg += (35.0f - CoreTemperature) * 0.1f * DeltaTime;

	// Hyperthermia (hot) — symmetric. Scales with how far over the threshold.
	if (CoreTemperature > HyperthermiaThreshold)
		Dmg += (CoreTemperature - HyperthermiaThreshold) * 0.1f * DeltaTime;

	// Suffocation — sharp damage curve, kills fast when oxygen is critical.
	if (IsSuffocating())
		Dmg += 2.0f * DeltaTime;

	if (Dmg > 0.0f) ApplyDamage(Dmg);
}

void UQRSurvivalComponent::TickInjuries(float DeltaTime)
{
	// Derive game-hours-per-second from the project day length setting so injury
	// durations stay consistent if DayLengthSeconds is ever tuned.
	const UQRCoreSettings* S        = GetDefault<UQRCoreSettings>();
	const float DayLenSec           = S ? S->DayLengthSeconds : 1200.0f;
	const float GameHoursPerSecond  = 24.0f / DayLenSec;
	float HoursElapsed = DeltaTime * GameHoursPerSecond;

	for (int32 i = ActiveInjuries.Num() - 1; i >= 0; --i)
	{
		FQRInjury& Inj = ActiveInjuries[i];
		if (Inj.DamagePerSecond > 0.0f)
			Health = FMath::Max(0.0f, Health - Inj.DamagePerSecond * DeltaTime);

		if (Inj.RemainingHours > 0.0f)
		{
			Inj.RemainingHours -= HoursElapsed;
			if (Inj.RemainingHours <= 0.0f)
				ActiveInjuries.RemoveAt(i);
		}
	}

	if (Health <= 0.0f && !bIsDead) TriggerDeath();
}

void UQRSurvivalComponent::RefreshStatusTags()
{
	FGameplayTagContainer NewTags;

	if (IsStarving())   NewTags.AddTag(QRGameplayTags::Status_Starving);
	else if (Hunger / MaxHunger < 0.35f) NewTags.AddTag(QRGameplayTags::Status_Hungry);

	if (IsDehydrated()) NewTags.AddTag(QRGameplayTags::Status_Dehydrated);
	else if (Thirst / MaxThirst < 0.35f) NewTags.AddTag(QRGameplayTags::Status_Thirsty);

	if (IsExhausted())  NewTags.AddTag(QRGameplayTags::Status_Exhausted);
	if (CoreTemperature < 35.0f) NewTags.AddTag(QRGameplayTags::Status_Hypothermia);
	if (IsOverheating()) NewTags.AddTag(QRGameplayTags::Status_Hyperthermia);

	if (IsSuffocating()) NewTags.AddTag(QRGameplayTags::Status_Suffocating);
	else if (IsLowOxygen()) NewTags.AddTag(QRGameplayTags::Status_LowOxygen);

	for (const FQRInjury& Inj : ActiveInjuries)
	{
		switch (Inj.Type)
		{
		case EQRInjuryType::Bleeding:   NewTags.AddTag(QRGameplayTags::Status_Bleeding);   break;
		case EQRInjuryType::Fracture:   NewTags.AddTag(QRGameplayTags::Status_Fracture);   break;
		case EQRInjuryType::Infection:  NewTags.AddTag(QRGameplayTags::Status_Infection);  break;
		case EQRInjuryType::Concussion: NewTags.AddTag(QRGameplayTags::Status_Concussion); break;
		case EQRInjuryType::Toxin:      NewTags.AddTag(QRGameplayTags::Status_Toxin);      break;
		default: break;
		}
	}

	if (NewTags != ActiveStatusTags)
	{
		ActiveStatusTags = NewTags;
		OnStatusChanged.Broadcast(ActiveStatusTags);
	}
}

void UQRSurvivalComponent::ApplyDamage(float Amount, EQRInjuryType InjuryType)
{
	if (bIsDead || Amount <= 0.0f) return;
	Health = FMath::Max(0.0f, Health - Amount);
	OnHealthChanged.Broadcast(Health);

	if (InjuryType != EQRInjuryType::None)
		AddInjury(InjuryType, Amount > 40.0f ? EQRInjurySeverity::Severe : EQRInjurySeverity::Minor);

	if (Health <= 0.0f) TriggerDeath();
}

void UQRSurvivalComponent::ApplyHealing(float Amount)
{
	if (bIsDead || Amount <= 0.0f) return;
	Health = FMath::Min(MaxHealth, Health + Amount);
	OnHealthChanged.Broadcast(Health);
}

void UQRSurvivalComponent::ConsumeFood(UQRItemInstance* FoodItem)
{
	if (!FoodItem || !FoodItem->Definition) return;
	if (FoodItem->Quantity <= 0) return;  // prevent multi-consume exploit on same instance

	const FQRFoodStats& Stats  = FoodItem->Definition->FoodStats;
	const UQRItemDefinition* Def = FoodItem->Definition;

	// v1.17 SafeKnown rule: EarthSealed/ShipRation items are inherently safe UNLESS
	// PackageIntegrity is compromised (< 0.5). Native/Unknown food requires explicit Safe state.
	bool bIsSafe = (FoodItem->EdibilityState == EQREdibilityState::Safe ||
	                FoodItem->EdibilityState == EQREdibilityState::Researched);

	if (!bIsSafe)
	{
		const EQRFoodOriginClass Origin = Def->FoodOriginClass;
		if ((Origin == EQRFoodOriginClass::EarthSealed || Origin == EQRFoodOriginClass::ShipRation)
		    && Def->PackageIntegrity >= 0.5f)
		{
			bIsSafe = true;
		}
	}

	const float Cals = bIsSafe ? Stats.CaloriesCooked : Stats.CaloriesRaw;
	Hunger = FMath::Min(MaxHunger, Hunger + (Cals / 2200.0f) * MaxHunger);
	Thirst = FMath::Min(MaxThirst, Thirst + (Stats.WaterContentML / 2500.0f) * MaxThirst);

	// Toxin risk for raw/unknown food
	if (!bIsSafe && Stats.RawRiskChance > 0.0f)
	{
		if (FMath::FRand() < Stats.RawRiskChance)
			AddInjury(EQRInjuryType::Toxin, EQRInjurySeverity::Minor);
	}

	// Spoiled food always risks infection regardless of origin; spread to nearby eaters
	if (FoodItem->IsSpoiled())
	{
		AddInjury(EQRInjuryType::Infection, EQRInjurySeverity::Moderate);
		SpreadInfectionNearby(300.0f, EQRInjuryType::Infection, 0.30f);
	}

	// Decrement quantity — caller is responsible for removing the instance when Quantity == 0
	--FoodItem->Quantity;
}

void UQRSurvivalComponent::SpreadInfectionNearby(float RadiusCm, EQRInjuryType Type, float Chance)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(NAME_None, /*bTraceComplex=*/false, Owner);
	Owner->GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Owner->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(RadiusCm),
		Params
	);

	for (const FOverlapResult& Hit : Overlaps)
	{
		AActor* Other = Hit.GetActor();
		if (!Other) continue;
		UQRSurvivalComponent* OtherSurv = Other->FindComponentByClass<UQRSurvivalComponent>();
		if (!OtherSurv || OtherSurv->bIsDead) continue;
		if (FMath::FRand() < Chance)
			OtherSurv->AddInjury(Type, EQRInjurySeverity::Minor);
	}
}

void UQRSurvivalComponent::DrinkWater(float WaterML, bool bIsPurified)
{
	Thirst = FMath::Min(MaxThirst, Thirst + (WaterML / 2500.0f) * MaxThirst);

	if (!bIsPurified && FMath::FRand() < 0.25f)
	{
		AddInjury(EQRInjuryType::Infection, EQRInjurySeverity::Minor);
		SpreadInfectionNearby(300.0f, EQRInjuryType::Infection, 0.20f);
	}
}

void UQRSurvivalComponent::Rest(float GameHours)
{
	Fatigue = FMath::Min(MaxFatigue, Fatigue + GameHours * 12.5f);
}

void UQRSurvivalComponent::AddInjury(EQRInjuryType Type, EQRInjurySeverity Severity)
{
	// Don't stack same injury type
	for (FQRInjury& Existing : ActiveInjuries)
	{
		if (Existing.Type == Type)
		{
			if (Severity > Existing.Severity) Existing.Severity = Severity;
			return;
		}
	}

	FQRInjury New;
	New.Type     = Type;
	New.Severity = Severity;

	switch (Type)
	{
	case EQRInjuryType::Bleeding:   New.DamagePerSecond = (Severity == EQRInjurySeverity::Severe) ? 0.3f : 0.1f; New.RemainingHours = 6.0f; break;
	case EQRInjuryType::Infection:  New.DamagePerSecond = 0.05f; New.RemainingHours = 48.0f; break;
	case EQRInjuryType::Toxin:      New.DamagePerSecond = 0.2f;  New.RemainingHours = 8.0f;  break;
	case EQRInjuryType::Fracture:   New.DamagePerSecond = 0.0f;  New.RemainingHours = -1.0f; break; // permanent until treated
	case EQRInjuryType::Concussion: New.DamagePerSecond = 0.01f; New.RemainingHours = 12.0f; break;
	default: New.RemainingHours = 4.0f; break;
	}

	ActiveInjuries.Add(New);
}

bool UQRSurvivalComponent::TreatInjury(EQRInjuryType Type, float HealBoost)
{
	for (int32 i = 0; i < ActiveInjuries.Num(); ++i)
	{
		if (ActiveInjuries[i].Type == Type)
		{
			ActiveInjuries.RemoveAt(i);
			if (HealBoost > 0.0f) ApplyHealing(HealBoost);
			return true;
		}
	}
	return false;
}

bool UQRSurvivalComponent::HasInjury(EQRInjuryType Type) const
{
	for (const FQRInjury& Inj : ActiveInjuries)
	{
		if (Inj.Type == Type) return true;
	}
	return false;
}

void UQRSurvivalComponent::TriggerDeath()
{
	bIsDead = true;
	Health  = 0.0f;
	ActiveInjuries.Empty();
	OnDeath.Broadcast();
}

void UQRSurvivalComponent::OnRep_Health()  { OnHealthChanged.Broadcast(Health); }
void UQRSurvivalComponent::OnRep_Hunger()  {}
void UQRSurvivalComponent::OnRep_Thirst()  {}
void UQRSurvivalComponent::OnRep_Fatigue() {}
void UQRSurvivalComponent::OnRep_Oxygen()  {}

void UQRSurvivalComponent::RefillOxygen(float Amount)
{
	if (bIsDead || Amount <= 0.0f) return;
	Oxygen = FMath::Min(MaxOxygen, Oxygen + Amount);
}

float UQRSurvivalComponent::GetMovementSpeedMultiplier() const
{
	float Mult = 1.0f;
	// Fracture is the big mover — broken leg means slowed for sure.
	for (const FQRInjury& Inj : ActiveInjuries)
	{
		if (Inj.Type == EQRInjuryType::Fracture)
		{
			Mult *= (Inj.Severity >= EQRInjurySeverity::Severe) ? 0.55f : 0.75f;
		}
		else if (Inj.Type == EQRInjuryType::Bleeding && Inj.Severity >= EQRInjurySeverity::Severe)
		{
			Mult *= 0.85f;
		}
	}
	if (IsExhausted())  Mult *= 0.75f;
	if (IsSuffocating()) Mult *= 0.7f;
	return FMath::Max(0.5f, Mult); // floor so player always has some agency
}

bool UQRSurvivalComponent::IsSprintBlockedByCondition() const
{
	if (IsExhausted())   return true;
	if (IsSuffocating()) return true;
	for (const FQRInjury& Inj : ActiveInjuries)
	{
		if (Inj.Type == EQRInjuryType::Fracture &&
		    Inj.Severity >= EQRInjurySeverity::Severe) return true;
	}
	return false;
}

bool UQRSurvivalComponent::QueryOwnerIsSprinting() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) return false;
	const FBoolProperty* Prop = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bIsSprinting"));
	if (!Prop) return false;
	return Prop->GetPropertyValue_InContainer(Owner);
}
