// Zelda3D process and per-run lifecycle.
#ifndef ZELDA3D_CORE_RUNTIME_H
#define ZELDA3D_CORE_RUNTIME_H

#include "global.h"

#ifdef __cplusplus
extern "C" {
#endif

int Zelda3D_Enabled(void);
extern int gZelda3dEnabled;

typedef enum {
    ZELDA3D_GRAPHICS_ORIGINAL = 0,
    ZELDA3D_GRAPHICS_OOT3D = 1,
} Zelda3DGraphicsMode;

// Graphics-mode switching is deliberately transactional. A request made while PlayState is live
// queues a same-entrance fade; the mode itself is committed by Play_Init, after the old scene and
// collision have been destroyed and before the replacement scene is initialized.
void Zelda3D_GraphicsModeInitialize(void);
void Zelda3D_GraphicsModeResetRunState(void);
void Zelda3D_RequestGraphicsMode(int mode);
void Zelda3D_ProcessGraphicsModeRequest(PlayState* play);
void Zelda3D_ApplyPendingGraphicsMode(void);
int Zelda3D_GetRequestedGraphicsMode(void);
int Zelda3D_IsGraphicsModeSwitchPending(void);

void Zelda3D_FrameBegin(void);
void Zelda3D_FrameEndUpdate(PlayState* play);
void Zelda3D_RegisterHostHooks(void);
void Zelda3D_CoreRunBegin(void);
int Zelda3D_CoreRunEnd(void);

typedef struct {
    unsigned int epoch;
} Zelda3DOnce;

int Zelda3D_Once(Zelda3DOnce* once);

#ifdef __cplusplus
}
#endif

#endif // ZELDA3D_CORE_RUNTIME_H
