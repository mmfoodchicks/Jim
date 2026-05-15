#include "QRWorldGenSeedActor.h"
#include "QRWorldGenSubsystem.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

AQRWorldGenSeedActor::AQRWorldGenSeedActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AQRWorldGenSeedActor::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoGenerateOnBeginPlay) Generate();
}

void AQRWorldGenSeedActor::Generate()
{
	UWorld* W = GetWorld();
	if (!W)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRWorldGenSeed] no world"));
		return;
	}
	UQRWorldGenSubsystem* Sub = W->GetSubsystem<UQRWorldGenSubsystem>();
	if (!Sub)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRWorldGenSeed] subsystem not loaded — compile C++"));
		return;
	}
	Sub->Generate(WorldSeed, WorldMapSizeKm, CellSizeMeters);
}

void AQRWorldGenSeedActor::ExportMinimap()
{
	UWorld* W = GetWorld();
	if (!W) return;
	UQRWorldGenSubsystem* Sub = W->GetSubsystem<UQRWorldGenSubsystem>();
	if (!Sub || !Sub->bGenerated)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRWorldGenSeed] Generate() first"));
		return;
	}

	UTexture2D* Tex = Sub->BuildMinimapTexture(MinimapPixelsPerCell);
	if (!Tex)
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRWorldGenSeed] minimap texture creation failed"));
		return;
	}

	// Write as PNG under Saved/QRWorldGen/ for quick inspection. We
	// avoid creating an editor asset so the file can be opened with
	// any image viewer; designer can drag-import to /Game/ if needed.
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("QRWorldGen");
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.DirectoryExists(*Dir)) PF.CreateDirectoryTree(*Dir);
	const FString FileName = FString::Printf(TEXT("Minimap_seed_%d.png"), WorldSeed);
	const FString FilePath = Dir / FileName;

	// Read the texture's source pixels (CreateTexture2D embeds them).
	TArray<FColor> Out;
	const int32 W2 = Tex->GetSizeX();
	const int32 H2 = Tex->GetSizeY();
	Out.Init(FColor::Black, W2 * H2);
	if (Tex->GetPlatformData() && Tex->GetPlatformData()->Mips.Num() > 0)
	{
		FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
		const void* Data = Mip.BulkData.LockReadOnly();
		if (Data)
		{
			FMemory::Memcpy(Out.GetData(), Data, W2 * H2 * sizeof(FColor));
		}
		Mip.BulkData.Unlock();
	}

	TArray64<uint8> CompressedPng;
	FImageUtils::PNGCompressImageArray(W2, H2, Out, CompressedPng);
	if (FFileHelper::SaveArrayToFile(CompressedPng, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("[QRWorldGenSeed] wrote minimap to %s"), *FilePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[QRWorldGenSeed] PNG save failed: %s"), *FilePath);
	}
}
