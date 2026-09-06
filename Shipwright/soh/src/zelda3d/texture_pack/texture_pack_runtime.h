// Optional OoT3D HD texture-pack discovery and transactional runtime switching.
#ifndef ZELDA3D_TEXTURE_PACK_RUNTIME_H
#define ZELDA3D_TEXTURE_PACK_RUNTIME_H

#include <stdint.h>

typedef struct PlayState PlayState;

#ifdef __cplusplus
extern "C" {
#endif

// Called after the CVar store is loaded. Discovers either an original Citra/Azahar ZIP or an
// extracted pack in the app-data texture-packs directory and validates it before use.
void Zelda3D_TexturePackInitialize(void);
void Zelda3D_TexturePackResetRunState(void);

// Requests are transactional while the OoT3D renderer owns a live scene: the current entrance
// fades out and Play_Init commits the change after the old model/collision state has been destroyed.
void Zelda3D_TexturePackRequestEnabled(int enabled);
void Zelda3D_TexturePackRequestRescan(void);
void Zelda3D_TexturePackProcessRequest(PlayState* play);
void Zelda3D_TexturePackApplyPending(void);

int Zelda3D_TexturePackAvailable(void);
int Zelda3D_TexturePackActive(void);
int Zelda3D_TexturePackRequestedEnabled(void);
int Zelda3D_TexturePackSwitchPending(void);
int Zelda3D_TexturePackExternalOverride(void);
int Zelda3D_TexturePackIsArchive(void);
uint64_t Zelda3D_TexturePackIndexedCount(void);
const char* Zelda3D_TexturePackInstallDirectory(void);
const char* Zelda3D_TexturePackSource(void);
const char* Zelda3D_TexturePackName(void);
const char* Zelda3D_TexturePackVersion(void);
const char* Zelda3D_TexturePackError(void);
const char* Zelda3D_TexturePackStatus(void);

#ifdef __cplusplus
}
#endif

#endif // ZELDA3D_TEXTURE_PACK_RUNTIME_H
