#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QRTypes.h"
#include "QRLeaderComponent.generated.h"

// A leader complaint/directive mission generated from the system
USTRUCT(BlueprintType)
struct QRCOLONYAI_API FQRLeaderDirective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName DirectiveId;

	UPROPERTY(BlueprintReadOnly)
	FText Title;

	UPROPERTY(BlueprintReadOnly)
	FText Description;

	UPROPERTY(BlueprintReadOnly)
	EQRLeaderType LeaderType = EQRLeaderType::None;

	// What stat is this directive trying to fix
	UPROPERTY(BlueprintReadOnly)
	FName AffectedStat;

	// Severity (0..1) drives how urgently the leader escalates
	UPROPERTY(BlueprintReadOnly)
	float Severity = 0.0f;

	// How long until this directive escalates to a debuff
	UPROPERTY(BlueprintReadOnly)
	float EscalationTimeHours = 48.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bIsResolved = false;
};

// A leader condition / debuff state
USTRUCT(BlueprintType)
struct QRCOLONYAI_API FQRLeaderCondition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName ConditionId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	// Which stat this debuffs and by how much
	UPROPERTY(BlueprintReadOnly)
	FName AffectedStat;

	UPROPERTY(BlueprintReadOnly)
	float DebuffAmount = 0.0f;

	// Healed by successful relevant leadership events
	UPROPERTY(BlueprintReadOnly)
	float RemainingHours = 24.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderDirectiveAdded, FQRLeaderDirective, Directive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderDirectiveResolved, FName, DirectiveId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderConditionAdded, FQRLeaderCondition, Condition);

// Manages a colony leader's directives, conditions, XP, and moral compass
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QRCOLONYAI_API UQRLeaderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRLeaderComponent();

	// ── Identity ─────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leader")
	EQRLeaderType LeaderType = EQRLeaderType::None;

	// The leader type this NPC is naturally suited for, based on their primary role.
	// Set once during NPC creation or discovery. Mismatching LeaderType vs NativeLeaderType
	// triggers the cross-craft penalty applied to LeaderBuff and condition debuffs.
	// None = generalist / no affinity (no penalty in any type).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leader")
	EQRLeaderType NativeLeaderType = EQRLeaderType::None;

	// Set true when this NPC was discovered as an existing leader in the world rather than
	// promoted from scratch. World-found leaders start with meaningful XP already set and
	// receive a reduced cross-craft penalty (0.8 instead of 0.6) because their experience
	// lets them adapt to new roles more readily than a freshly promoted survivor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leader")
	bool bIsWorldFoundLeader = false;

	// Cross-craft penalty multiplier applied to LeaderBuff. Also amplifies condition debuffs.
	// 1.0 = no penalty (native craft or generalist).
	// 0.8 = experienced cross-craft (world-found leader in wrong type).
	// 0.6 = fresh cross-craft (locally promoted into a type outside their native affinity).
	// Recomputed by RecalculateLeaderDerivedStats().
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Aptitude")
	float CrossCraftPenaltyMult = 1.0f;

	// ── Core Aptitude (v1.4 Leader_Parameters) ──
	// Leadership aptitude L [0..10]. LeaderBuff = Clamp(1+0.02*L+0.01*S, 1.0, 1.35)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Leader|Aptitude",
		meta = (ClampMin = "0", ClampMax = "10"))
	float LeadershipAptitude = 5.0f;    // L

	// Skill aptitude S [0..10]. Combined with L for LeaderLevel formula.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Leader|Aptitude",
		meta = (ClampMin = "0", ClampMax = "10"))
	float SkillAptitude = 5.0f;         // S

	// Composure COM [0..10]. Governs stress resistance and escalation dampening.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Leader|Aptitude",
		meta = (ClampMin = "0", ClampMax = "10"))
	float Composure = 5.0f;             // COM

	// Computed: Clamp(1 + 0.02*L + 0.01*S, 1.0, 1.35)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Aptitude")
	float LeaderBuff = 1.0f;

	// Computed: floor(1 + 4*((0.6*L + 0.4*S) / 10)), range 1..5
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Aptitude")
	int32 LeaderLevel = 1;

	// True when LeaderLevel == 1 and no significant XP earned — enables tutorial scaffolding
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Aptitude")
	bool bIsInexperiencedLeader = false;

	// ── Stats (replicated) ───────────────────
	// Morale Index (MI): 0..100
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoraleIndex = 50.0f;

	// Morale Resilience (MR): how much MI buffers before cracking
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoraleResilience = 30.0f;

	// Morale Gradient (MG): recent trend in MI (+/- per game-day)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoraleGradient = 0.0f;

	// Leader XP (gained from successful events)
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float LeaderXP = 0.0f;

	// Defection risk [0..1] — high when MI is chronically low
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float DefectionRisk = 0.0f;

	// Moral compass scalar [-1..1] (negative = ruthless, positive = humane) — legacy scalar
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Stats")
	float MoralCompassVector = 0.0f;

	// ── Issue Escalation (v1.17) ─────────────
	// Current phase of the leader blocker→quest pipeline
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Issues")
	EQRLeaderIssueState IssueState = EQRLeaderIssueState::None;

	// IssueEscalationScore = BlockerSeverity * max(BlockerDurationHours - GuidanceDelayHours, 0) * LeaderAwarenessMult
	// Quest issued when score >= 100
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Issues")
	float IssueEscalationScore = 0.0f;

	// How long (in-game hours) the current blocker has persisted
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Issues")
	float BlockerDurationHours = 0.0f;

	// Grace window (hours) before the leader reacts — COM-scaled
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leader|Issues",
		meta = (ClampMin = "0"))
	float GuidanceDelayHours = 6.0f;

	// Multiplier driven by leader awareness/perception traits [0.5..2.0]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leader|Issues",
		meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float LeaderAwarenessMult = 1.0f;

	// ── Camp Alignment / Policy (v1.4 Moral Compass) ──
	// Dot-product alignment of leader preferences with camp policy [-1..1]
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Policy")
	float CampAlignmentScore = 0.0f;

	// Per-axis policy weights — index maps to EQRMoralCompassAxis (8 axes)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leader|Policy")
	TArray<float> CampPolicyVector;

	// ── Active Directives / Conditions ───────
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Directives")
	TArray<FQRLeaderDirective> ActiveDirectives;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Leader|Conditions")
	TArray<FQRLeaderCondition> ActiveConditions;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Leader|Events")
	FOnLeaderDirectiveAdded OnDirectiveAdded;

	UPROPERTY(BlueprintAssignable, Category = "Leader|Events")
	FOnLeaderDirectiveResolved OnDirectiveResolved;

	UPROPERTY(BlueprintAssignable, Category = "Leader|Events")
	FOnLeaderConditionAdded OnConditionAdded;

	// ── Interface ────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void AddDirective(FQRLeaderDirective Directive);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void ResolveDirective(FName DirectiveId, float MoralBias = 0.0f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void AddCondition(FQRLeaderCondition Condition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void HealCondition(FName ConditionId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void ApplyMoraleChange(float Delta, bool bIsEvent = false);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void GainLeaderXP(float XP);

	// Recompute LeaderBuff, LeaderLevel, bIsInexperiencedLeader, and CrossCraftPenaltyMult
	// from current L/S and the NativeLeaderType vs LeaderType comparison.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void RecalculateLeaderDerivedStats();

	// Maps an NPC's primary work role to the leader type they are naturally suited for.
	// Used to populate NativeLeaderType during NPC creation.
	UFUNCTION(BlueprintPure, Category = "Leader")
	static EQRLeaderType GetNaturalLeaderTypeForRole(EQRNPCRole Role);

	// Tick the issue escalation score and transition IssueState FSM
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void AdvanceIssueEscalation(float BlockerSeverity, float DeltaGameHours);

	// Resolve the current blocker issue; resets escalation state
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void ResolveCurrentIssue();

	// Update CampAlignmentScore by dot-producting CampPolicyVector with supplied camp preference vector
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Leader")
	void UpdateCampAlignment(const TArray<float>& CampPreferenceVector);

	UFUNCTION(BlueprintPure, Category = "Leader")
	float GetTotalDebuff(FName StatName) const;

	UFUNCTION(BlueprintPure, Category = "Leader")
	bool IsDefecting() const { return DefectionRisk >= 0.85f; }

	// Eff_M formula: 0.50 + 0.007 * MoraleIndex
	UFUNCTION(BlueprintPure, Category = "Leader")
	float GetEfficiencyMultiplier() const { return FMath::Clamp(0.50f + 0.007f * MoraleIndex, 0.0f, 1.2f); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void TickDirectiveEscalation(float DeltaTime);
	void TickConditionRecovery(float DeltaTime);
	void RecalculateDefectionRisk();
};
