#!/usr/bin/env python3
"""Exercise Fusion's graphics lock against stale settings and runtime requests."""
from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from test_oot_original_boot import ROOT, SOURCE, function


def run(args: list[str]) -> None:
    subprocess.run(args, check=True)


def main() -> None:
    assert 'add_compile_definitions(ZELDA_FUSION=1)' in (ROOT / 'CMakeLists.txt').read_text()
    with tempfile.TemporaryDirectory(prefix='fusion-original-policy-') as temp:
        work = Path(temp)
        headers = {
            'global.h': '''#pragma once
#include <stddef.h>
typedef struct PlayState {
    int transitionTrigger, transitionMode, nextEntranceIndex, transitionType;
} PlayState;
typedef struct { int entranceIndex, nextTransitionType; } SaveContext;
extern SaveContext gSaveContext;
enum { TRANS_TRIGGER_OFF=0, TRANS_TRIGGER_START=1, TRANS_MODE_OFF=0, TRANS_TYPE_FADE_BLACK=2 };
''',
            'libultraship/bridge.h': '''#pragma once
int CVarGetInteger(const char*, int);
void CVarSetInteger(const char*, int);
''',
            'ship/utils/Environment.h': 'const char* ShipGetEnvironmentVariable(const char*);\n',
            'soh/cvar_prefixes.h': '#define CVAR_SETTING(name) "gSettings." name\n',
        }
        for name, content in headers.items():
            target = work / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(content)
        fixture = work / 'policy.c'
        fixture.write_text('''
#include <assert.h>
#include <string.h>
#include "zelda3d_runtime.h"
SaveContext gSaveContext;
PlayState* gPlayState;
static int setting = 1;
int CVarGetInteger(const char* key, int fallback) { (void)key; (void)fallback; return setting; }
void CVarSetInteger(const char* key, int value) { (void)key; setting = value; }
const char* ShipGetEnvironmentVariable(const char* key) { (void)key; return "1"; }
int main(void) {
    assert(Zelda3D_Enabled() == 0);
    Zelda3D_GraphicsModeInitialize();
    assert(setting == 0 && Zelda3D_GetRequestedGraphicsMode() == 0);
    PlayState play = {0};
    gPlayState = &play;
    for (int request = -1; request <= 2; ++request) {
        Zelda3D_RequestGraphicsMode(request);
        Zelda3D_ProcessGraphicsModeRequest(&play);
        Zelda3D_ApplyPendingGraphicsMode();
        assert(setting == 0 && Zelda3D_Enabled() == 0);
        assert(!Zelda3D_IsGraphicsModeSwitchPending() && !play.transitionTrigger);
    }
    setting = 1; // Preset or console edits bypass the menu callback.
    Zelda3D_ProcessGraphicsModeRequest(&play);
    Zelda3D_ApplyPendingGraphicsMode();
    assert(!Zelda3D_Enabled() && !Zelda3D_IsGraphicsModeSwitchPending() && !play.transitionTrigger);
    gZelda3dEnabled = 1; // A legacy direct assignment must not bypass Fusion's policy.
    assert(Zelda3D_Enabled() == 0);
    Zelda3D_GraphicsModeResetRunState();
    Zelda3D_GraphicsModeInitialize();
    assert(setting == 0 && Zelda3D_Enabled() == 0);
    return 0;
}
''')
        executable = work / 'policy'
        runtime = SOURCE / 'zelda3d/core/zelda3d_runtime.c'
        run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
             '-DZELDA_FUSION=1', f'-I{work}', f'-I{runtime.parent}',
             str(runtime), str(fixture), '-o', str(executable)])
        run([str(executable)])

        fixture = work / 'scale.cpp'
        fixture.write_text('''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#define CVAR_SETTING(name) name
static int savedScale = 0;
static constexpr uint32_t defaultImGuiScale = 0;
static const float imguiScaleOptionToValue[] = {0.75f, 1.0f, 1.5f, 2.0f};
int CVarGetInteger(const char*, int) { return savedScale; }
namespace ImGui {
struct Style { float size = 1; void ScaleAllSizes(float scale) { size *= scale; } } style;
struct IO { float FontGlobalScale; } io;
Style& GetStyle() { return style; }
IO& GetIO() { return io; }
}
struct OTRGlobals {
    int previousImGuiScaleIndex = -1;
    float previousImGuiScale = 1.0f;
    void ScaleImGui();
};
''' + function('../soh/OTRGlobals.cpp', 'OTRGlobals::ScaleImGui') + '''
int main() {
    OTRGlobals app;
    app.ScaleImGui();
    assert(ImGui::style.size == 0.75f && ImGui::io.FontGlobalScale == 0.75f);
    app.ScaleImGui();
    assert(ImGui::style.size == 0.75f);
    savedScale = 1; app.ScaleImGui();
    assert(ImGui::style.size == 1.0f);
    savedScale = 99; app.ScaleImGui();
    assert(ImGui::style.size == 2.0f && std::isfinite(ImGui::style.size));
    savedScale = -5; app.ScaleImGui();
    assert(ImGui::style.size == 0.75f);
}
''')
        executable = work / 'scale'
        run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
             str(fixture), '-o', str(executable)])
        run([str(executable)])
        assert 'previousImGuiScale = 1.0f;' in (ROOT / 'Shipwright/soh/soh/OTRGlobals.cpp').read_text()

        # Compile the production asset entry points without any loader/cache services.
        # The Fusion branches must be self-contained and unable to touch those services.
        fixture = work / 'assets.cpp'
        fixture.write_text('#include <cassert>\nnamespace Zelda3D { struct CtrRom; }\n' +
            '\n'.join(function(path, name) for path, name in [
                ('zelda3d/model/zelda3d_model.cpp', 'rom'),
                ('zelda3d/hud/zelda3d_hud_tex.cpp', 'Zelda3D_HudTexEnabled'),
                ('zelda3d/hud/zelda3d_hud.cpp', 'hudEnabled'),
                ('zelda3d/texture_pack/texture_pack_runtime.cpp', 'Zelda3D_TexturePackInitialize'),
                ('zelda3d/texture_pack/texture_pack_runtime.cpp', 'Zelda3D_TexturePackRequestEnabled'),
                ('zelda3d/texture_pack/texture_pack_runtime.cpp', 'Zelda3D_TexturePackRequestRescan'),
            ]) + '''
int main() {
    assert(rom() == nullptr && Zelda3D_HudTexEnabled() == 0 && !hudEnabled());
    Zelda3D_TexturePackInitialize();
    Zelda3D_TexturePackRequestEnabled(1);
    Zelda3D_TexturePackRequestRescan();
    assert(rom() == nullptr && Zelda3D_HudTexEnabled() == 0 && !hudEnabled());
}
''')
        executable = work / 'assets'
        run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
             '-DZELDA_FUSION=1', str(fixture), '-o', str(executable)])
        run([str(executable)])

    assets = ROOT / 'Shipwright/libultraship/assets/rml'
    for name in ('fusion_launcher.rml', 'fusion_menu.rml'):
        text = (assets / name).read_text()
        document = ET.fromstring(text)
        assert not any(word in text.lower() for word in ('zelda3d', 'oot3d', 'mm3d', '3ds', 'linkmode'))
        for style in document.findall('./head/link'):
            assert (assets / style.attrib['href']).is_file()
    launcher = ET.parse(assets / 'fusion_launcher.rml')
    assert {node.attrib['action'] for node in launcher.findall('.//button')} == {
        'start_oot', 'start_mm', 'quit'}
    menu = ET.parse(assets / 'fusion_menu.rml')
    assert len(menu.findall('.//tab')) == len(menu.findall('.//pane')) == 1
    assert not menu.findall('.//select-button')
    print('Fusion Original policy: config/request isolation, asset access, menu scaling and UI contracts PASS')


if __name__ == '__main__':
    main()
