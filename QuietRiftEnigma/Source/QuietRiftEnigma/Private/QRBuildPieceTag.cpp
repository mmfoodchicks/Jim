#include "QRBuildPieceTag.h"
#include "Net/UnrealNetwork.h"

UQRBuildPieceTag::UQRBuildPieceTag()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UQRBuildPieceTag::BeginPlay()
{
	Super::BeginPlay();
	if (!PieceGuid.IsValid())
	{
		PieceGuid = FGuid::NewGuid();
	}
}

void UQRBuildPieceTag::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UQRBuildPieceTag, PieceId);
	DOREPLIFETIME(UQRBuildPieceTag, PieceGuid);
}
