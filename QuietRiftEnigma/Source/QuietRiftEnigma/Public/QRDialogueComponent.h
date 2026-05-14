#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "QRTypes.h"
#include "QRDialogueComponent.generated.h"

/**
 * One spoken line in a conversation node. The Text field may contain
 * UQRPronounLibrary tokens ({name}, {he}, {they}, {is}, {has}, etc.)
 * that will be substituted using the initiating player's identity at
 * runtime — writers don't have to author multiple variants per pronoun.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRDialogueLine
{
	GENERATED_BODY()

	// Who's talking. Empty = use the NPC's display name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Speaker;

	// The line, with optional {tokens} for pronoun + name substitution.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true))
	FText Text;
};

/**
 * One row in a dialogue DataTable. A node holds a sequence of lines
 * that play in order when the conversation reaches the node. After the
 * last line:
 *   - If NextNodeId is set, the conversation jumps to that node.
 *   - Otherwise the conversation ends.
 *
 * Branching dialogue isn't supported in this first pass — every node
 * has at most one follow-up. The pattern keeps the authoring surface
 * tight; we can add branching later by adding a Choices array.
 */
USTRUCT(BlueprintType)
struct QUIETRIFTENIGMA_API FQRDialogueNodeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FQRDialogueLine> Lines;

	// Optional jump target after the last line. NAME_None = end conversation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName NextNodeId;

	// Optional gating — the conversation only fires this node when the
	// initiating player has all of these tags (their faction tag, quest
	// state, etc.). Empty container = always allowed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueLineSpoken,
	FText, Speaker, FText, SubstitutedText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueStarted, AActor*, Initiator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueEnded);

/**
 * Per-actor dialogue runner. Drop on any AActor (NPCs, terminals, vending
 * machines, intercoms) — when an AQRCharacter interacts and the target
 * has this component, the conversation starts. The component:
 *
 *   1. Looks up the start node in DialogueTable by StartNodeId
 *   2. For each line, runs UQRPronounLibrary::Substitute against the
 *      initiating player's FQRPlayerIdentity so {name}/{he}/{they}/etc.
 *      resolve correctly
 *   3. Fires OnLineSpoken — your UI binds here to display the text
 *   4. Waits for Advance() (player presses Continue / E) before emitting
 *      the next line
 *   5. When done, jumps to NextNodeId or fires OnDialogueEnded
 *
 * UI integration: bind a UMG widget to OnLineSpoken. Show the Speaker
 * + SubstitutedText, give the player a Continue button that calls
 * Advance(). Bind OnDialogueEnded to dismiss the widget.
 */
UCLASS(ClassGroup=(QuietRift), meta=(BlueprintSpawnableComponent))
class QUIETRIFTENIGMA_API UQRDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQRDialogueComponent();

	// ── Config ───────────────────────────────
	// DataTable of FQRDialogueNodeRow rows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TObjectPtr<UDataTable> DialogueTable;

	// Which node the conversation begins from when StartConversation runs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName StartNodeId;

	// Display name shown when a line's Speaker field is empty. Defaults
	// to "NPC" — set this per-NPC blueprint to "Engineer Marsh" etc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText DefaultSpeakerName;

	// If true, the conversation can be restarted after it ends. If false,
	// it plays exactly once and then refuses further interactions. Quest-
	// givers usually set this to true; one-shot intro NPCs set false.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bRestartable = true;

	// ── Events ───────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Dialogue|Events")
	FOnDialogueStarted OnDialogueStarted;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue|Events")
	FOnDialogueLineSpoken OnLineSpoken;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue|Events")
	FOnDialogueEnded OnDialogueEnded;

	// ── API ──────────────────────────────────
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dialogue")
	bool StartConversation(AActor* Initiator);

	// Move to the next line. If at end of node, follows NextNodeId or ends.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dialogue")
	void Advance();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dialogue")
	void Cancel();

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsTalking() const { return !CurrentNodeId.IsNone(); }

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool HasBeenSpoken() const { return bHasFinishedOnce; }

	// Returns the substituted text of the line currently displayed. Useful
	// when UI needs to re-fetch (e.g. on widget redraw without re-firing).
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FText GetCurrentSubstitutedText() const;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	FText GetCurrentSpeaker() const;

private:
	UPROPERTY()
	TObjectPtr<AActor> CurrentInitiator = nullptr;

	UPROPERTY()
	FQRPlayerIdentity CachedIdentity;

	FName CurrentNodeId;
	int32 CurrentLineIndex = -1;
	bool  bHasFinishedOnce = false;

	const FQRDialogueNodeRow* FindNode(FName NodeId) const;
	void EnterNode(FName NodeId);
	void EmitCurrentLine();
	void EndConversation();

	// Pull the initiator's FQRPlayerIdentity reflectively — we don't hard-
	// link to AQRCharacter so any actor with a UPROPERTY named
	// "PlayerIdentity" works (the player, an NPC roleplaying a player,
	// a debug stand-in).
	FQRPlayerIdentity ResolveIdentity(AActor* Initiator) const;
};
