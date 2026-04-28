using UnrealBuildTool;

public class QRColonyAI : ModuleRules
{
	public QRColonyAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"QRCore",
			"QRItems",
			"QRSurvival",
			"QRLogistics",
			"QRCraftingResearch",
			"Net"
		});
	}
}
