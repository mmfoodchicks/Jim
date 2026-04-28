using UnrealBuildTool;

public class QRLogistics : ModuleRules
{
	public QRLogistics(ReadOnlyTargetRules Target) : base(Target)
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
			"AIModule",
			"NavigationSystem",
			"Net"
		});
	}
}
