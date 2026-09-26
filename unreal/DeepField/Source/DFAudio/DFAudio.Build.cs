using UnrealBuildTool;

// DF module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class DFAudio : ModuleRules
{
	public DFAudio(ReadOnlyTargetRules Target) : base(Target)
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
			"GameplayAbilities",
			"AudioMixer",
			"MetasoundEngine",
			"MetasoundFrontend",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			// (none)
		});
	}
}
