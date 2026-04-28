using UnrealBuildTool;

public class QRSaveNet : ModuleRules
{
	public QRSaveNet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"QRCore",
			"QRItems",
			"QRSurvival",
			"QRCraftingResearch",
			"QRColonyAI",
			"Net",
			"NetCore"
		});
	}
}
