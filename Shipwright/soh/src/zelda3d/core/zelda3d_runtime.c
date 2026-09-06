#include "zelda3d_runtime.h"

#include <libultraship/bridge.h>
#include <stdlib.h>

#include "soh/cvar_prefixes.h"

#define ZELDA3D_GRAPHICS_MODE_CVAR CVAR_SETTING("GraphicsMode")

int gZelda3dEnabled = -1;

static int sGraphicsModeInitialized = 0;
static int sRequestedGraphicsMode = -1;
static int sPendingGraphicsMode = -1;
static int sModeReloadQueued = 0;

extern PlayState* gPlayState;

static int Zelda3D_NormalizeGraphicsMode(int mode) {
    return mode == ZELDA3D_GRAPHICS_OOT3D ? ZELDA3D_GRAPHICS_OOT3D : ZELDA3D_GRAPHICS_ORIGINAL;
}

static int Zelda3D_DefaultGraphicsMode(void) {
    // The developer harness has historically used SOH3D=1/0. Keep that useful as the default for
    // an unset config, but never make it a permanent override: the packaged build must stay
    // toggleable from its Settings menu.
    const char* value = getenv("SOH3D");
    if (value != NULL) {
        return value[0] == '0' ? ZELDA3D_GRAPHICS_ORIGINAL : ZELDA3D_GRAPHICS_OOT3D;
    }
    return ZELDA3D_GRAPHICS_ORIGINAL;
}

static int Zelda3D_ConfiguredGraphicsMode(void) {
    return Zelda3D_NormalizeGraphicsMode(
        CVarGetInteger(ZELDA3D_GRAPHICS_MODE_CVAR, Zelda3D_DefaultGraphicsMode()));
}

int Zelda3D_Enabled(void) {
    if (gZelda3dEnabled < 0) {
        gZelda3dEnabled = Zelda3D_DefaultGraphicsMode();
    }
    return gZelda3dEnabled;
}

void Zelda3D_GraphicsModeInitialize(void) {
    const int configured = Zelda3D_ConfiguredGraphicsMode();
    gZelda3dEnabled = configured;
    sRequestedGraphicsMode = configured;
    sPendingGraphicsMode = -1;
    sModeReloadQueued = 0;
    sGraphicsModeInitialized = 1;
}

void Zelda3D_GraphicsModeResetRunState(void) {
    gZelda3dEnabled = -1;
    sGraphicsModeInitialized = 0;
    sRequestedGraphicsMode = -1;
    sPendingGraphicsMode = -1;
    sModeReloadQueued = 0;
}

void Zelda3D_RequestGraphicsMode(int mode) {
    const int normalized = Zelda3D_NormalizeGraphicsMode(mode);

    if (!sGraphicsModeInitialized) {
        // InitOTR normally seeds this after loading the config. This path also makes the API safe
        // for a menu/preset callback that runs before the first gameplay state exists.
        Zelda3D_GraphicsModeInitialize();
    }

    CVarSetInteger(ZELDA3D_GRAPHICS_MODE_CVAR, normalized);
    sRequestedGraphicsMode = normalized;

    if (gPlayState == NULL) {
        gZelda3dEnabled = normalized;
        sPendingGraphicsMode = -1;
        sModeReloadQueued = 0;
        return;
    }

    if (normalized == gZelda3dEnabled) {
        sPendingGraphicsMode = -1;
        return;
    }

    sPendingGraphicsMode = normalized;
}

void Zelda3D_ProcessGraphicsModeRequest(PlayState* play) {
    if (!sGraphicsModeInitialized) {
        return;
    }

    // Console edits and presets do not execute the menu widget callback, so treat the persisted
    // CVar as the requested state every frame as well.
    const int configured = Zelda3D_ConfiguredGraphicsMode();
    if (configured != sRequestedGraphicsMode) {
        Zelda3D_RequestGraphicsMode(configured);
    }

    if (sPendingGraphicsMode < 0 || play == NULL || sModeReloadQueued) {
        return;
    }

    if (sPendingGraphicsMode == gZelda3dEnabled) {
        sPendingGraphicsMode = -1;
        return;
    }

    // If another transition is already underway, leave the request pending. Play_Init will commit
    // it for the destination scene, which avoids starting a competing transition.
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF) {
        return;
    }

    play->nextEntranceIndex = gSaveContext.entranceIndex;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    sModeReloadQueued = 1;
}

void Zelda3D_ApplyPendingGraphicsMode(void) {
    if (!sGraphicsModeInitialized) {
        return;
    }

    // Catch a CVar changed from the title screen or by a preset before gameplay was created.
    const int configured = Zelda3D_ConfiguredGraphicsMode();
    if (configured != sRequestedGraphicsMode) {
        sRequestedGraphicsMode = configured;
        sPendingGraphicsMode = configured;
    }

    if (sPendingGraphicsMode >= 0) {
        gZelda3dEnabled = sPendingGraphicsMode;
    }
    sPendingGraphicsMode = -1;
    sModeReloadQueued = 0;
}

int Zelda3D_GetRequestedGraphicsMode(void) {
    if (!sGraphicsModeInitialized) {
        return Zelda3D_Enabled();
    }
    return sRequestedGraphicsMode;
}

int Zelda3D_IsGraphicsModeSwitchPending(void) {
    return sPendingGraphicsMode >= 0;
}
