#include "QRResearchComponent.h"
#include "QRInventoryComponent.h"
#include "Net/UnrealNetwork.h"

UQRResearchComponent::UQRResearchComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f;
	SetIsReplicatedByDefault(true);
}

void UQRResearchComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRResearchComponent, TechNodeStates);
	DOREPLIFETIME(UQRResearchComponent, MicroResearchStates);
	DOREPLIFETIME(UQRResearchComponent, MicroResearchQueue);
	DOREPLIFETIME(UQRResearchComponent, ResearchPointsPerSecond);
}

void UQRResearchComponent::InitializeTechNodes(const TArray<UQRTechNode*>& AllNodes)
{
	NodeDefinitions.Empty();
	TechNodeStates.Empty();

	for (const UQRTechNode* Node : AllNodes)
	{
		if (!Node) continue;
		NodeDefinitions.Add(Node->TechNodeId, Node);

		FQRTechNodeRuntime Runtime;
		Runtime.TechNodeId = Node->TechNodeId;
		Runtime.State      = Node->Prerequisites.IsEmpty() ? EQRTechNodeState::ResearchPending : EQRTechNodeState::Locked;
		TechNodeStates.Add(Runtime);
	}
}

EQRTechNodeState UQRResearchComponent::GetTechNodeState(FName NodeId) const
{
	for (const FQRTechNodeRuntime& RT : TechNodeStates)
	{
		if (RT.TechNodeId == NodeId) return RT.State;
	}
	return EQRTechNodeState::Locked;
}

bool UQRResearchComponent::IsTechUnlocked(FName NodeId) const
{
	return GetTechNodeState(NodeId) == EQRTechNodeState::Unlocked;
}

bool UQRResearchComponent::DeliverReferenceComponent(FName ReferenceComponentId)
{
	// Find the tech node that needs this ref component
	for (FQRTechNodeRuntime& RT : TechNodeStates)
	{
		const UQRTechNode** NodePtr = NodeDefinitions.Find(RT.TechNodeId);
		if (!NodePtr || !(*NodePtr)) continue;

		if ((*NodePtr)->RequiredReferenceComponentId == ReferenceComponentId && !RT.bRefComponentDelivered)
		{
			RT.bRefComponentDelivered = true;
			TryUnlockNode(RT);
			return true;
		}
	}
	return false;
}

void UQRResearchComponent::AddResearchPoints(float Points, EQRResearchFamily Family)
{
	// Distribute to nodes in ResearchPending state that match the family
	for (FQRTechNodeRuntime& RT : TechNodeStates)
	{
		if (RT.State != EQRTechNodeState::ResearchPending) continue;

		const UQRTechNode** NodePtr = NodeDefinitions.Find(RT.TechNodeId);
		if (!NodePtr || !(*NodePtr)) continue;
		if ((*NodePtr)->Family != Family) continue;

		RT.AccumulatedResearchPoints += Points;
		TryUnlockNode(RT);
	}
}

void UQRResearchComponent::TryUnlockNode(FQRTechNodeRuntime& Runtime)
{
	const UQRTechNode** NodePtr = NodeDefinitions.Find(Runtime.TechNodeId);
	if (!NodePtr || !(*NodePtr)) return;
	const UQRTechNode* Node = *NodePtr;

	// Check prerequisites
	if (!ArePrerequisitesMet(Node)) return;

	// Check research progress
	if (Node->ResearchPointsRequired > 0.0f && Runtime.AccumulatedResearchPoints < Node->ResearchPointsRequired)
		return;

	// Check ref component
	if (!Node->RequiredReferenceComponentId.IsNone() && !Runtime.bRefComponentDelivered)
	{
		Runtime.State = EQRTechNodeState::AwaitingRefComp;
		return;
	}

	// All conditions met — unlock!
	Runtime.State = EQRTechNodeState::Unlocked;
	OnTechNodeUnlocked.Broadcast(Runtime.TechNodeId);

	// Unlock children that were waiting on this
	for (FQRTechNodeRuntime& OtherRT : TechNodeStates)
	{
		if (OtherRT.State == EQRTechNodeState::Locked)
		{
			const UQRTechNode** OtherNodePtr = NodeDefinitions.Find(OtherRT.TechNodeId);
			if (OtherNodePtr && *OtherNodePtr && ArePrerequisitesMet(*OtherNodePtr))
				OtherRT.State = EQRTechNodeState::ResearchPending;
		}
	}
}

bool UQRResearchComponent::ArePrerequisitesMet(const UQRTechNode* Node) const
{
	for (const FName& Prereq : Node->Prerequisites)
	{
		if (!IsTechUnlocked(Prereq)) return false;
	}
	return true;
}

bool UQRResearchComponent::QueueMicroResearch(FName MicroResearchId, UQRInventoryComponent* CostSource)
{
	const UQRMicroResearchDefinition** DefPtr = MicroResearchDefinitions.Find(MicroResearchId);
	if (!DefPtr || !(*DefPtr)) return false;

	// Check stack cap
	int32 Stacks = GetMicroResearchStacks(MicroResearchId);
	if (Stacks >= (*DefPtr)->MaxStacks) return false;

	// Verify cost items exist
	for (const FName& ItemId : (*DefPtr)->RequiredItemIds)
	{
		if (!CostSource || !CostSource->HasItem(ItemId, 1)) return false;
	}

	// Consume cost
	for (const FName& ItemId : (*DefPtr)->RequiredItemIds)
	{
		CostSource->TryRemoveItem(ItemId, 1);
	}

	if (!MicroResearchQueue.Contains(MicroResearchId))
		MicroResearchQueue.Add(MicroResearchId);

	OnResearchQueued.Broadcast(MicroResearchId);
	return true;
}

int32 UQRResearchComponent::GetMicroResearchStacks(FName MicroResearchId) const
{
	for (const FQRMicroResearchRuntime& RT : MicroResearchStates)
	{
		if (RT.MicroResearchId == MicroResearchId) return RT.CompletedStacks;
	}
	return 0;
}

float UQRResearchComponent::GetMicroResearchMultiplier(FName MicroResearchId) const
{
	const UQRMicroResearchDefinition** DefPtr = MicroResearchDefinitions.Find(MicroResearchId);
	if (!DefPtr) return 1.0f;

	for (const FQRMicroResearchRuntime& RT : MicroResearchStates)
	{
		if (RT.MicroResearchId == MicroResearchId)
			return RT.GetCurrentMultiplier((*DefPtr)->FactorPerStack);
	}
	return 1.0f;
}

void UQRResearchComponent::AdvanceCodexState(FName Id, EQRCodexDiscoveryState NewState)
{
	EQRCodexDiscoveryState* Existing = CodexStates.Find(Id);
	if (!Existing || *Existing < NewState)
		CodexStates.Add(Id, NewState);
}

EQRCodexDiscoveryState UQRResearchComponent::GetCodexState(FName Id) const
{
	const EQRCodexDiscoveryState* Found = CodexStates.Find(Id);
	return Found ? *Found : EQRCodexDiscoveryState::Undiscovered;
}

void UQRResearchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()->HasAuthority()) return;

	TickResearchQueue(DeltaTime);
}

void UQRResearchComponent::TickResearchQueue(float DeltaTime)
{
	if (MicroResearchQueue.IsEmpty()) return;

	const FName CurrentId = MicroResearchQueue[0];
	const UQRMicroResearchDefinition** DefPtr = MicroResearchDefinitions.Find(CurrentId);
	if (!DefPtr || !(*DefPtr))
	{
		MicroResearchQueue.RemoveAt(0);
		return;
	}

	// Find or create runtime entry
	FQRMicroResearchRuntime* RT = nullptr;
	for (FQRMicroResearchRuntime& Entry : MicroResearchStates)
	{
		if (Entry.MicroResearchId == CurrentId) { RT = &Entry; break; }
	}
	if (!RT)
	{
		FQRMicroResearchRuntime NewRT;
		NewRT.MicroResearchId = CurrentId;
		MicroResearchStates.Add(NewRT);
		RT = &MicroResearchStates.Last();
	}

	RT->AccumulatedPoints += ResearchPointsPerSecond * DeltaTime;

	if (RT->AccumulatedPoints >= (*DefPtr)->PointsPerStack)
	{
		RT->AccumulatedPoints -= (*DefPtr)->PointsPerStack;
		RT->CompletedStacks   = FMath::Min(RT->CompletedStacks + 1, (*DefPtr)->MaxStacks);
		OnMicroResearchStacked.Broadcast(CurrentId, RT->CompletedStacks);

		if (RT->CompletedStacks >= (*DefPtr)->MaxStacks)
			MicroResearchQueue.RemoveAt(0);
	}
}


void UQRResearchComponent::ForceUnlockTechNode(FName NodeId)
{
	for (FQRTechNodeRuntime& RT : TechNodeStates)
	{
		if (RT.TechNodeId == NodeId)
		{
			RT.AccumulatedResearchPoints = TNumericLimits<float>::Max();
			RT.bRefComponentDelivered    = true;
			TryUnlockNode(RT);
			return;
		}
	}
	// Node not yet in TechNodeStates — add a minimal runtime and mark unlocked
	FQRTechNodeRuntime New;
	New.TechNodeId               = NodeId;
	New.State                    = EQRTechNodeState::Unlocked;
	New.bRefComponentDelivered   = true;
	TechNodeStates.Add(New);
	OnTechNodeUnlocked.Broadcast(NodeId);
}
