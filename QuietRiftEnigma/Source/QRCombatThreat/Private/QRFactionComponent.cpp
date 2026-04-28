#include "QRFactionComponent.h"
#include "Net/UnrealNetwork.h"

UQRFactionComponent::UQRFactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQRFactionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRFactionComponent, Relations);
}

void UQRFactionComponent::InitRelation(FGameplayTag OtherFaction, EQRFactionStance InitialStance, float InitialTrust)
{
	if (FindRelation(OtherFaction)) return;

	FQRFactionRelation NewRel;
	NewRel.OtherFactionTag = OtherFaction;
	NewRel.Stance          = InitialStance;
	NewRel.TrustScore      = InitialTrust;
	Relations.Add(NewRel);
}

void UQRFactionComponent::ModifyTrust(FGameplayTag OtherFaction, float Delta)
{
	FQRFactionRelation* Rel = FindRelation(OtherFaction);
	if (!Rel)
	{
		InitRelation(OtherFaction, EQRFactionStance::Neutral, Delta);
		return;
	}

	Rel->TrustScore = FMath::Clamp(Rel->TrustScore + Delta, -100.0f, 100.0f);
	OnTrustChanged.Broadcast(OtherFaction, Rel->TrustScore);
	RecalculateStance(*Rel);
}

void UQRFactionComponent::RecalculateStance(FQRFactionRelation& Relation)
{
	EQRFactionStance OldStance = Relation.Stance;
	float T = Relation.TrustScore;

	if      (T >=  60.0f) Relation.Stance = EQRFactionStance::Allied;
	else if (T >=  25.0f) Relation.Stance = EQRFactionStance::Friendly;
	else if (T >= -20.0f) Relation.Stance = EQRFactionStance::Neutral;
	else if (T >= -60.0f) Relation.Stance = EQRFactionStance::Hostile;
	else                  Relation.Stance = EQRFactionStance::AtWar;

	if (Relation.Stance != OldStance)
		OnStanceChanged.Broadcast(Relation.OtherFactionTag, Relation.Stance);
}

EQRFactionStance UQRFactionComponent::GetStance(FGameplayTag OtherFaction) const
{
	const FQRFactionRelation* Rel = FindRelation(OtherFaction);
	return Rel ? Rel->Stance : EQRFactionStance::Unknown;
}

float UQRFactionComponent::GetTrust(FGameplayTag OtherFaction) const
{
	const FQRFactionRelation* Rel = FindRelation(OtherFaction);
	return Rel ? Rel->TrustScore : 0.0f;
}

bool UQRFactionComponent::IsHostileTo(FGameplayTag OtherFaction) const
{
	EQRFactionStance Stance = GetStance(OtherFaction);
	return Stance == EQRFactionStance::Hostile || Stance == EQRFactionStance::AtWar;
}

bool UQRFactionComponent::HasTradeAccess(FGameplayTag OtherFaction) const
{
	EQRFactionStance Stance = GetStance(OtherFaction);
	return Stance == EQRFactionStance::Friendly || Stance == EQRFactionStance::Allied;
}

void UQRFactionComponent::CompleteContract(FGameplayTag OtherFaction, FName ContractId, float TrustReward)
{
	ModifyTrust(OtherFaction, TrustReward);

	FQRFactionRelation* Rel = FindRelation(OtherFaction);
	if (Rel) Rel->CompletedContracts.AddUnique(ContractId);
}

FQRFactionRelation* UQRFactionComponent::FindRelation(FGameplayTag OtherFaction)
{
	for (FQRFactionRelation& Rel : Relations)
	{
		if (Rel.OtherFactionTag == OtherFaction) return &Rel;
	}
	return nullptr;
}

const FQRFactionRelation* UQRFactionComponent::FindRelation(FGameplayTag OtherFaction) const
{
	for (const FQRFactionRelation& Rel : Relations)
	{
		if (Rel.OtherFactionTag == OtherFaction) return &Rel;
	}
	return nullptr;
}
