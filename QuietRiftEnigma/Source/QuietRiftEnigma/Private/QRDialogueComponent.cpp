#include "QRDialogueComponent.h"
#include "QRPronounLibrary.h"
#include "UObject/UnrealType.h"

UQRDialogueComponent::UQRDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

const FQRDialogueNodeRow* UQRDialogueComponent::FindNode(FName NodeId) const
{
	if (!DialogueTable || NodeId.IsNone()) return nullptr;
	return DialogueTable->FindRow<FQRDialogueNodeRow>(NodeId, TEXT("QRDialogue"), false);
}

FQRPlayerIdentity UQRDialogueComponent::ResolveIdentity(AActor* Initiator) const
{
	if (!Initiator) return UQRPronounLibrary::MakeDefaultIdentity();

	// Reflective lookup of a UPROPERTY named "PlayerIdentity" — any actor
	// class that exposes one will be picked up. Falls back to the default
	// identity if the actor doesn't have the field.
	const FStructProperty* Prop = FindFProperty<FStructProperty>(
		Initiator->GetClass(), TEXT("PlayerIdentity"));
	if (!Prop) return UQRPronounLibrary::MakeDefaultIdentity();

	const void* StructAddr = Prop->ContainerPtrToValuePtr<void>(Initiator);
	if (!StructAddr) return UQRPronounLibrary::MakeDefaultIdentity();

	// Verify the struct type matches FQRPlayerIdentity. If a different
	// struct happens to share the name, bail out safely.
	if (Prop->Struct != FQRPlayerIdentity::StaticStruct())
		return UQRPronounLibrary::MakeDefaultIdentity();

	return *reinterpret_cast<const FQRPlayerIdentity*>(StructAddr);
}

bool UQRDialogueComponent::StartConversation(AActor* Initiator)
{
	if (IsTalking()) return false;
	if (!bRestartable && bHasFinishedOnce) return false;
	if (StartNodeId.IsNone()) return false;
	if (!FindNode(StartNodeId)) return false;

	CurrentInitiator = Initiator;
	CachedIdentity = ResolveIdentity(Initiator);

	OnDialogueStarted.Broadcast(Initiator);
	EnterNode(StartNodeId);
	return true;
}

void UQRDialogueComponent::EnterNode(FName NodeId)
{
	const FQRDialogueNodeRow* Node = FindNode(NodeId);
	if (!Node || Node->Lines.Num() == 0)
	{
		EndConversation();
		return;
	}

	CurrentNodeId    = NodeId;
	CurrentLineIndex = 0;
	EmitCurrentLine();
}

void UQRDialogueComponent::EmitCurrentLine()
{
	const FQRDialogueNodeRow* Node = FindNode(CurrentNodeId);
	if (!Node || !Node->Lines.IsValidIndex(CurrentLineIndex))
	{
		EndConversation();
		return;
	}

	const FQRDialogueLine& Line = Node->Lines[CurrentLineIndex];
	const FText Speaker = Line.Speaker.IsEmpty() ? DefaultSpeakerName : Line.Speaker;

	// Pronoun + name substitution. FText→FString round-trip is fine for
	// this layer; FText localization should happen on the template itself
	// (the dialogue table) before it ever reaches this code.
	const FString Substituted = UQRPronounLibrary::Substitute(
		Line.Text.ToString(), CachedIdentity);

	OnLineSpoken.Broadcast(Speaker, FText::FromString(Substituted));
}

void UQRDialogueComponent::Advance()
{
	if (!IsTalking()) return;

	const FQRDialogueNodeRow* Node = FindNode(CurrentNodeId);
	if (!Node)
	{
		EndConversation();
		return;
	}

	CurrentLineIndex++;
	if (Node->Lines.IsValidIndex(CurrentLineIndex))
	{
		EmitCurrentLine();
		return;
	}

	// End of node — follow up if a NextNodeId is set.
	if (!Node->NextNodeId.IsNone() && FindNode(Node->NextNodeId))
	{
		EnterNode(Node->NextNodeId);
		return;
	}

	EndConversation();
}

void UQRDialogueComponent::Cancel()
{
	if (!IsTalking()) return;
	EndConversation();
}

void UQRDialogueComponent::EndConversation()
{
	CurrentNodeId    = NAME_None;
	CurrentLineIndex = -1;
	CurrentInitiator = nullptr;
	bHasFinishedOnce = true;
	OnDialogueEnded.Broadcast();
}

FText UQRDialogueComponent::GetCurrentSubstitutedText() const
{
	const FQRDialogueNodeRow* Node = FindNode(CurrentNodeId);
	if (!Node || !Node->Lines.IsValidIndex(CurrentLineIndex)) return FText::GetEmpty();
	const FString Out = UQRPronounLibrary::Substitute(
		Node->Lines[CurrentLineIndex].Text.ToString(), CachedIdentity);
	return FText::FromString(Out);
}

FText UQRDialogueComponent::GetCurrentSpeaker() const
{
	const FQRDialogueNodeRow* Node = FindNode(CurrentNodeId);
	if (!Node || !Node->Lines.IsValidIndex(CurrentLineIndex)) return FText::GetEmpty();
	const FText& Speaker = Node->Lines[CurrentLineIndex].Speaker;
	return Speaker.IsEmpty() ? DefaultSpeakerName : Speaker;
}
