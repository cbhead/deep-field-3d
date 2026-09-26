#!/usr/bin/env python3
"""Generate module skeletons (Build.cs + module .h/.cpp) from unreal/Build/modules.json.

Never overwrites an existing file: a module's Build.cs is owned by its workstream after
creation (adding engine deps is the owner's; adding DF deps must respect the layering,
which layering-check.py enforces).

  python3 unreal/Build/new-module.py            # create every missing module
  python3 unreal/Build/new-module.py DFTowers   # one module
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "unreal" / "DeepField" / "Source"
MODULES = json.loads((REPO / "unreal" / "Build" / "modules.json").read_text())["modules"]

BUILD_CS = """using UnrealBuildTool;

// Layer {layer} module (unreal/Build/modules.json). DF dependencies may only point at lower
// layers; unreal/Build/layering-check.py fails the build otherwise.
public class {name} : ModuleRules
{{
	public {name}(ReadOnlyTargetRules Target) : base(Target)
	{{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(new string[] {{
{public}
		}});

		PrivateDependencyModuleNames.AddRange(new string[] {{
{private}
		}});
	}}
}}
"""

MODULE_H = """#pragma once

#include "Modules/ModuleManager.h"

class F{name}Module : public IModuleInterface
{{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
}};
"""

MODULE_CPP = """#include "{name}Module.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDF{short}, Log, All);

void F{name}Module::StartupModule()
{{
	UE_LOG(LogDF{short}, Log, TEXT("{name} module started"));
}}

void F{name}Module::ShutdownModule()
{{
}}

{impl}
"""


def fmt(names: list[str]) -> str:
    return "\n".join(f'\t\t\t"{n}",' for n in names) if names else "\t\t\t// (none)"


def make(name: str, spec: dict) -> list[str]:
    created = []
    root = SRC / name
    (root / "Public").mkdir(parents=True, exist_ok=True)
    (root / "Private").mkdir(parents=True, exist_ok=True)
    build = root / f"{name}.Build.cs"
    if not build.exists():
        build.write_text(BUILD_CS.format(name=name, layer=spec["layer"],
                                         public=fmt(spec["df"] + spec["public"]),
                                         private=fmt(spec["private"])))
        created.append(str(build.relative_to(REPO)))
    h = root / "Public" / f"{name}Module.h"
    if not h.exists():
        h.write_text(MODULE_H.format(name=name))
        created.append(str(h.relative_to(REPO)))
    cpp = root / "Private" / f"{name}Module.cpp"
    if not cpp.exists():
        short = name.removeprefix("DF") or name
        impl = (f'IMPLEMENT_PRIMARY_GAME_MODULE(F{name}Module, {name}, "DeepField");'
                if spec.get("primary") else f"IMPLEMENT_MODULE(F{name}Module, {name})")
        cpp.write_text(MODULE_CPP.format(name=name, short=short, impl=impl))
        created.append(str(cpp.relative_to(REPO)))
    return created


def main() -> int:
    wanted = sys.argv[1:] or list(MODULES)
    created: list[str] = []
    for name in wanted:
        if name not in MODULES:
            sys.exit(f"unknown module {name}; add it to unreal/Build/modules.json (INT)")
        created += make(name, MODULES[name])
    print("\n".join(created) if created else "nothing to create")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
