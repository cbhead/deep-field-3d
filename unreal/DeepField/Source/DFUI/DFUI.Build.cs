using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFUI : ModuleRules
{
	public DFUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"DFMatch",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"UMG",
			"Slate",
			"SlateCore",
			"CommonUI",
			"CommonInput",
			"ModelViewViewModel",
			"EnhancedInput",
			"InputCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			// (none)
		});
	}
}
