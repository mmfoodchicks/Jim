using UnrealBuildTool;

public class QRCombatThreat : ModuleRules
{
	public QRCombatThreat(ReadOnlyTargetRules Target) : base(Target)
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
			"QRCore",
			"QRItems",
			"QRSurvival",
			"QRColonyAI"
		});
	}
}
