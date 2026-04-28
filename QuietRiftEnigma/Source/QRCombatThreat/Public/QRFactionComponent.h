#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRFactionComponent.generated.h"

// Diplomatic relationship entry between this faction and another
USTRUCT(BlueprintType)
struct QRCOMBATTHREAT_API FQRFactionRelation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag OtherFactionTag;

	UPROPERTY(BlueprintReadOnly)
	EQRFactionStance Stance = EQRFactionStance::Unknown;

	// Trust score [-100 .. 100]; thresholds shift stance
	UPROPERTY(BlueprintReadOnly)
	float TrustScore = 0.0f;

	// History of completed missions/interactions
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> CompletedContracts;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFactionStanceChanged, FGameplayTag, FactionTag, EQRFactionStance, NewStance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFactionTrustChanged, FGameplayTag, FactionTag, float, NewTrust);

// Manages faction identity, stance tracking, and market access
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRCOMBATTHREAT_API UQRFactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRFactionComponent();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	FText FactionDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Faction")
	bool bIsPlayerFaction = false;

	// ── Relations ─────────────────────────────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Faction")
	TArray<FQRFactionRelation> Relations;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Faction|Events")
	FOnFactionStanceChanged OnStanceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Faction|Events")
	FOnFactionTrustChanged OnTrustChanged;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Faction")
	void InitRelation(FGameplayTag OtherFaction, EQRFactionStance InitialStance, float InitialTrust = 0.0f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Faction")
	void ModifyTrust(FGameplayTag OtherFaction, float Delta);

	UFUNCTION(BlueprintPure, Category = "Faction")
	EQRFactionStance GetStance(FGameplayTag OtherFaction) const;

	UFUNCTION(BlueprintPure, Category = "Faction")
	float GetTrust(FGameplayTag OtherFaction) const;

	UFUNCTION(BlueprintPure, Category = "Faction")
	bool IsHostileTo(FGameplayTag OtherFaction) const;

	UFUNCTION(BlueprintPure, Category = "Faction")
	bool HasTradeAccess(FGameplayTag OtherFaction) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Faction")
	void CompleteContract(FGameplayTag OtherFaction, FName ContractId, float TrustReward);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	FQRFactionRelation* FindRelation(FGameplayTag OtherFaction);
	const FQRFactionRelation* FindRelation(FGameplayTag OtherFaction) const;
	void RecalculateStance(FQRFactionRelation& Relation);
};
