using UnrealBuildTool;

public class QRItems : ModuleRules
{
	public QRItems(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"QRCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NetCore"
		});
	}
}
