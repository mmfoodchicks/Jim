using UnrealBuildTool;

public class QuietRiftEnigma : ModuleRules
{
	public QuietRiftEnigma(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"AIModule",
			"NavigationSystem",
			"GameplayTags",
			"GameplayTasks",
			"QRCore",
			"QRItems",
			"QRSurvival",
			"QRLogistics",
			"QRCraftingResearch",
			"QRColonyAI",
			"QRCombatThreat",
			"QRSaveNet",
			"QRUI",
			"NetCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"PhysicsCore"
		});
	}
}
