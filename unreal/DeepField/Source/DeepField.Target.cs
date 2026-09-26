using UnrealBuildTool;
using System.Collections.Generic;

// Game target (listen host and client). No dedicated-server target on the launcher build
// (ADR-0003 / PROGRAMME.md §1.2); the L2 seam is IDFSessionBackend, not a target.
public class DeepFieldTarget : TargetRules
{
	public DeepFieldTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] {
			"DFCore", "DFGameplay", "DFPlayer", "DFTowers", "DFEnemies", "DFWorld",
			"DFVehicles", "DFMatch", "DFUI", "DFOnline", "DFAudio", "DFVfx"
		});
	}
}
