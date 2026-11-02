#!/usr/bin/env python3
"""Exercise production OoT title setup and native horse creation without a GPU.

The fixture replaces engine services, not the boot/rider functions under test.
It does not certify rendered frames or console playback.
"""
from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import tempfile

from codequery import DEF_RE, brace_range

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Shipwright/soh/src"


def function(path: str, name: str) -> str:
    lines = (SOURCE / path).read_text().splitlines()
    pattern = re.compile(DEF_RE.format(sym=re.escape(name)))
    matches = [i for i, line in enumerate(lines)
               if pattern.match(line) and not line.rstrip().endswith(";")]
    if len(matches) != 1:
        raise AssertionError(f"Expected one definition: {path}:{name}")
    start = matches[0]
    return "\n".join(lines[start:brace_range(lines, start) + 1])


def main() -> None:
    functions = [
        ("overlays/gamestates/ovl_opening/z_opening.c", "Opening_SetupTitleScreen"),
        ("overlays/gamestates/ovl_title/z_title.c", "Title_Init"),
        ("code/z_horse.c", "Horse_CanSpawn"),
        ("code/z_horse.c", "Horse_SetupInCutscene"),
        ("code/z_horse.c", "Horse_InitPlayerHorse"),
        ("code/z_play.c", "Zelda3D_SkipInit"),
        ("code/z_player_lib.c", "Player_SetBootData"),
        ("overlays/actors/ovl_player_actor/z_player.c", "func_8084B000"),
    ]
    fixture = (ROOT / "tools/oot_original_boot_test.c").read_text()
    boot_source = (SOURCE / "code/z_player_lib.c").read_text()
    start = boot_source.index('s16 sBootData[')
    boot_table = boot_source[start:boot_source.index('\n};', start) + 3]
    source = fixture.replace("/* PRODUCTION_FUNCTIONS */",
                             boot_table + '\n' + "\n\n".join(function(*item) for item in functions))
    with tempfile.TemporaryDirectory(prefix="oot-original-boot-") as temp:
        work = Path(temp)
        path = work / "test.c"
        path.write_text(source)
        executable = work / "test"
        subprocess.run([os.environ.get("CC", "cc"), "-std=c17", "-Wall", "-Wextra",
                        "-Werror", "-Wno-unused-parameter", "-Wno-unused-variable", "-Wno-unused-function",
                        "-Wno-sign-compare", "-Wno-overflow",
                        f"-I{ROOT / 'Shipwright/soh/include'}", str(path),
                        "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
