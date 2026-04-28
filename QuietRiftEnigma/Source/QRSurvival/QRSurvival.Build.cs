using UnrealBuildTool;

public class QRSurvival : ModuleRules
{
	public QRSurvival(ReadOnlyTargetRules Target) : base(Target)
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
			"Net"
		});
	}
}
