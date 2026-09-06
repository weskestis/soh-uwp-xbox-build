#!/usr/bin/env python3
"""Regression contracts for the CURSOR FPS V3 + OoT3D full-build merge."""

from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
LUS = REPO / "Shipwright" / "libultraship"
SOH = REPO / "Shipwright" / "soh"
ZAPD = REPO / "Shipwright" / "ZAPDTR" / "ZAPD"


def source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class CursorFpsV3Contracts(unittest.TestCase):
    def setUp(self) -> None:
        self.cursor = source(
            LUS / "src" / "fast" / "backends" / "cursor_fps_v3.cpp"
        )

    def test_exact_proxy_timing_motion_and_feedback_constants(self) -> None:
        for contract in (
            "kToggleHoldMs = 350",
            "kToastMs = 1600",
            "kDeadzone = 6500",
            "kMaxPixelsPerSecond = 1150",
            "std::clamp<Uint64>(elapsed, 1, 50)",
            "0x5000, 0x5000, 120",
            "0x2800, 0x2800, 70",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, self.cursor)

    def test_cursor_owns_only_the_proxy_inputs(self) -> None:
        self.assertIn("SDL_GAMEPAD_BUTTON_LEFT_STICK", self.cursor)
        self.assertIn("SDL_GAMEPAD_BUTTON_RIGHT_STICK", self.cursor)
        self.assertIn("SDL_GAMEPAD_BUTTON_SOUTH", self.cursor)
        self.assertIn("SDL_GAMEPAD_AXIS_RIGHTX", self.cursor)
        self.assertIn("SDL_GAMEPAD_AXIS_RIGHTY", self.cursor)
        self.assertIn("QueueMouseButton(south)", self.cursor)
        self.assertIn("return SDL_GetGamepadButton(gamepad, button);", self.cursor)
        self.assertIn("return SDL_GetGamepadAxis(gamepad, axis);", self.cursor)

    def test_sdl_lifecycle_and_event_pump_are_wired(self) -> None:
        backend = source(LUS / "src" / "fast" / "backends" / "gfx_sdl3.cpp")
        for contract in (
            "CursorFpsV3Init(mWnd);",
            "CursorFpsV3ConsumeEvent(event)",
            "CursorFpsV3Tick();",
            "CursorFpsV3Shutdown();",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, backend)
        self.assertLess(backend.index("CursorFpsV3Tick();"), backend.index("SDL_PeepEvents("))

    def test_full_and_compact_menus_are_mutually_exclusive(self) -> None:
        gui = source(LUS / "src" / "fast" / "Fast3dGui.cpp")
        self.assertIn("SDLK_F1", gui)
        self.assertIn("SDL_GAMEPAD_BUTTON_BACK", gui)
        self.assertIn("SDLK_ESCAPE", gui)
        self.assertIn("SDL_GAMEPAD_BUTTON_START", gui)
        self.assertIn("mRml->SetVisible(false);", gui)
        self.assertIn("GetMenu()->Hide();", gui)
        self.assertIn("BlockGameInput(ZELDA3D_FULL_SETTINGS_BLOCK_ID)", gui)


class GraphicsModeContracts(unittest.TestCase):
    def test_mode_request_is_committed_only_at_new_play_init(self) -> None:
        runtime = source(SOH / "src" / "zelda3d" / "core" / "zelda3d_runtime.c")
        play = source(SOH / "src" / "code" / "z_play.c")
        self.assertIn("play->nextEntranceIndex = gSaveContext.entranceIndex;", runtime)
        self.assertIn("play->transitionType = TRANS_TYPE_FADE_BLACK;", runtime)
        self.assertIn("void Zelda3D_ApplyPendingGraphicsMode(void)", runtime)
        self.assertIn("Zelda3D_ApplyPendingGraphicsMode();", play)
        self.assertLess(
            play.index("Zelda3D_ApplyPendingGraphicsMode();"),
            play.index("gPlayState = play;"),
        )

    def test_full_soh_menu_and_mode_selector_are_present(self) -> None:
        menu = source(SOH / "soh" / "SohGui" / "SohMenu.cpp")
        settings = source(SOH / "soh" / "SohGui" / "SohMenuSettings.cpp")
        for builder in (
            "AddMenuSettings();",
            "AddMenuEnhancements();",
            "AddMenuRandomizer();",
            "AddMenuNetwork();",
            "AddMenuDevTools();",
        ):
            with self.subTest(builder=builder):
                self.assertIn(builder, menu)
        self.assertIn('AddWidget(path, "Graphics Mode"', settings)
        self.assertIn("Original SoH", settings)
        self.assertIn("Ocarina of Time 3D", settings)
        self.assertIn("Zelda3D_AssetSourceReady()", settings)
        self.assertIn("This is independent of ", settings)
        self.assertIn('"Alternate Assets."', settings)

    def test_renderer_noops_are_visible_not_silently_accepted(self) -> None:
        settings = source(SOH / "soh" / "SohGui" / "SohMenuSettings.cpp")
        enhancements = source(
            SOH / "soh" / "SohGui" / "SohMenuEnhancements.cpp"
        )
        devtools = source(SOH / "soh" / "SohGui" / "SohMenuDevTools.cpp")
        for label in (
            "Anti-aliasing (MSAA)",
            "Enable Vsync",
            "Allow multi-windows",
        ):
            with self.subTest(label=label):
                location = settings.index(label)
                self.assertIn("options->disabled = true", settings[location : location + 900])
        location = enhancements.index("Fix Vanishing Paths")
        self.assertIn(
            "options->disabled = true", enhancements[location : location + 900]
        )
        self.assertIn("Unavailable with the SDL3 GPU renderer", devtools)

    def test_no_io_network_shim_is_disclosed_and_never_autoconnects(self) -> None:
        shim = source(SOH / "soh" / "Network" / "SDLNetShim.h")
        menu = source(SOH / "soh" / "SohGui" / "SohMenuNetwork.cpp")
        globals_cpp = source(SOH / "soh" / "OTRGlobals.cpp")
        self.assertIn("#define SOH_NETWORKING_AVAILABLE 0", shim)
        self.assertIn("#if !SOH_NETWORKING_AVAILABLE", menu)
        self.assertIn("no setting is silently accepted", menu)
        self.assertIn("#if SOH_NETWORKING_AVAILABLE", globals_cpp)


class TexturePackRuntimeContracts(unittest.TestCase):
    def test_original_zip_and_folder_loader_are_both_supported(self) -> None:
        loader = source(REPO / "Shipwright" / "cmb3d" / "asset" / "texpack.cpp")
        header = source(REPO / "Shipwright" / "cmb3d" / "asset" / "texpack.h")
        self.assertIn("zip_open", loader)
        self.assertIn("zip_get_num_entries", loader)
        self.assertIn("recursive_directory_iterator", loader)
        self.assertIn("use_new_hash=true", loader)
        self.assertIn("0004000000033500", loader)
        self.assertIn("void TexPackSetEnabled(bool enabled)", header)

    def test_switch_is_committed_at_scene_boundary_and_observed_in_original_mode(self) -> None:
        runtime = source(
            SOH / "src" / "zelda3d" / "texture_pack" / "texture_pack_runtime.cpp"
        )
        composition = source(SOH / "src" / "zelda3d" / "core" / "zelda3d.c")
        play = source(SOH / "src" / "code" / "z_play.c")
        self.assertIn("play->nextEntranceIndex = gSaveContext.entranceIndex;", runtime)
        self.assertIn("play->transitionType = TRANS_TYPE_FADE_BLACK;", runtime)
        self.assertLess(
            composition.index("Zelda3D_TexturePackProcessRequest(play);"),
            composition.index("if (!Zelda3D_Enabled())"),
        )
        self.assertLess(
            play.index("Zelda3D_TexturePackApplyPending();"),
            play.index("gPlayState = play;"),
        )

    def test_every_pack_dependent_cache_observes_loader_generation(self) -> None:
        model = source(SOH / "src" / "zelda3d" / "model" / "zelda3d_model.cpp")
        atlas = source(
            SOH / "src" / "zelda3d" / "model" / "zelda3d_atlas_cache.cpp"
        )
        hud = source(SOH / "src" / "zelda3d" / "hud" / "zelda3d_hud_tex.cpp")
        self.assertIn("g_loaded.clear();", model)
        self.assertIn("Zelda3D_GL_RequestEvictRange(0, INT_MAX);", model)
        self.assertIn("TexPackGeneration()", atlas)
        self.assertGreaterEqual(hud.count("TexPackGeneration()"), 5)
        self.assertGreaterEqual(hud.count("Zelda3D_HudTexClaim"), 10)

    def test_full_settings_ui_exposes_pack_toggle_status_and_rescan(self) -> None:
        settings = source(SOH / "soh" / "SohGui" / "SohMenuSettings.cpp")
        self.assertIn("Use OoT3D HD Texture Pack", settings)
        self.assertIn("Zelda3D_TexturePackStatus()", settings)
        self.assertIn("Rescan OoT3D Texture Pack Folder", settings)
        self.assertIn("no extraction needed", settings)


class DualRomAutoExtractionContracts(unittest.TestCase):
    def setUp(self) -> None:
        self.auto_extract = source(
            SOH / "soh" / "host" / "rom_auto_extraction.cpp"
        )

    def test_normal_and_master_quest_archives_are_tracked_independently(self) -> None:
        self.assertIn('ArchiveExists("oot.o2r")', self.auto_extract)
        self.assertIn('ArchiveExists("oot-mq.o2r")', self.auto_extract)
        self.assertIn(
            'const char* archiveName = isMasterQuest ? "oot-mq.o2r" : "oot.o2r";',
            self.auto_extract,
        )
        self.assertNotIn("VanillaArchiveExists", self.auto_extract)

    def test_existing_archives_are_preserved_and_missing_counterpart_is_scanned(self) -> None:
        self.assertIn("if (normalReady && masterQuestReady)", self.auto_extract)
        self.assertIn("if (archiveReady)", self.auto_extract)
        self.assertIn("already exists; preserving it", self.auto_extract)
        self.assertIn("continue;", self.auto_extract)
        self.assertIn("return normalReady || masterQuestReady;", self.auto_extract)


class ExtractionDeterminismContracts(unittest.TestCase):
    def test_zapd_discovers_inputs_in_a_stable_order(self) -> None:
        directory = source(ZAPD / "Utils" / "Directory.h")
        self.assertIn("#include <algorithm>", directory)
        self.assertIn("std::sort(lst.begin(), lst.end());", directory)

    def test_exported_struct_fields_never_serialize_heap_residue(self) -> None:
        skin = source(ZAPD / "OtherStructs" / "SkinLimbStructs.h")
        mesh_header = source(ZAPD / "ZRoom" / "Commands" / "SetMesh.h")
        mesh_source = source(ZAPD / "ZRoom" / "Commands" / "SetMesh.cpp")
        for contract in (
            "uint16_t index = 0;",
            "uint16_t vtxCount = 0;",
            "uint16_t totalVtxCount = 0;",
            "segptr_t limbModifications = 0;",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, skin)
        self.assertIn("uint8_t data = 0;", mesh_header)
        self.assertIn("uint8_t meshHeaderType = 0;", mesh_header)
        self.assertIn("data = cmdArg1;", mesh_source)


class AssetFallbackContracts(unittest.TestCase):
    def test_owner_extracted_romfs_is_a_first_class_asset_source(self) -> None:
        ctr_header = source(REPO / "Shipwright" / "cmb3d" / "asset" / "ctr_rom.h")
        ctr_source = source(REPO / "Shipwright" / "cmb3d" / "asset" / "ctr_rom.cpp")
        model = source(SOH / "src" / "zelda3d" / "model" / "zelda3d_model.cpp")
        self.assertIn("hostPath", ctr_header)
        self.assertIn("std::filesystem::is_directory", ctr_source)
        self.assertIn('getenv("ZELDA3D_OOT3D_ROMFS")', model)
        self.assertLess(
            model.index('getenv("ZELDA3D_OOT3D_ROMFS")'),
            model.index('getenv("ZELDA3D_OOT3D_ROM")'),
        )
        for anchor in (
            '"/kankyo/BlueSky.zar"',
            '"/actor/zelda_link_boy_new.zar"',
            '"/scene/spot00.zar"',
        ):
            with self.subTest(anchor=anchor):
                self.assertIn(anchor, model)

    def test_critical_replacements_preflight_model_or_animation(self) -> None:
        required = {
            "player/player_draw.cpp": "Zelda3D_AnimReady",
            "render/room_render.cpp": "Zelda3D_ModelReady",
            "render/sky_render.cpp": "Zelda3D_SkyLayerReady",
            "render/celestial_render.cpp": "Zelda3D_ModelReady",
            "behaviors/actor/boss_fd.cpp": "Zelda3D_AnimReady",
            "behaviors/actor/boss_fd2.cpp": "Zelda3D_AnimReady",
            "behaviors/actor/en_butte.cpp": "Zelda3D_AnimReady",
            "behaviors/actor/en_elf.cpp": "Zelda3D_AnimReady",
            "behaviors/actor/en_fish.cpp": "Zelda3D_AnimReady",
            "behaviors/title/title_logo.cpp": "Zelda3D_AnimReady",
        }
        root = SOH / "src" / "zelda3d"
        for relative, contract in required.items():
            with self.subTest(path=relative, contract=contract):
                self.assertIn(contract, source(root / relative))


if __name__ == "__main__":
    unittest.main()
