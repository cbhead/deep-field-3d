using UnrealBuildTool;

// Editor-only module (PROGRAMME.md §5.1 WS-01): the JSON -> DataTable commandlet and the tests that make
// the tables trustworthy. Among DF modules it depends on DFCore only (layer 0); it is never in a runtime
// target, so nothing at runtime may depend on it.
public class DFContentPipeline : ModuleRules
{
	public DFContentPipeline(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {
			"DFCore",
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
		});
	}
}
