using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFWorld : ModuleRules
{
	public DFWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"DFGameplay",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"NavigationSystem",
			"Landscape",
			"PhysicsCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			// (none)
		});
	}
}
