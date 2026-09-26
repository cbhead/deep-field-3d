using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFGameplay : ModuleRules
{
	public DFGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"GameplayAbilities",
			"GameplayTasks",
			"ModularGameplay",
			"NetCore",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			// (none)
		});

		if (Target.bBuildEditor)
		{
			// DF.Func.Status.* open a map and run it in PIE: FEndPlayMapCommand (Tests/AutomationEditorCommon.h).
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
