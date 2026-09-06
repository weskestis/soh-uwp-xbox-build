// Public model-provider lookup and per-model geometry queries.
#ifndef ZELDA3D_RENDER_MODEL_QUERIES_H
#define ZELDA3D_RENDER_MODEL_QUERIES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void Zelda3D_EnsureModelProvider(void);
/** Validate and identify the user-provided decrypted OoT3D cartridge asset source. */
int Zelda3D_AssetSourceReady(void);
const char* Zelda3D_AssetSourcePath(void);
const char* Zelda3D_AssetSourceError(void);
/** Synchronously validate CPU geometry and decoded textures before suppressing an N64 draw. */
int Zelda3D_ModelReady(int modelId);
int Zelda3D_AutoModelId(const char* zarPath);
float Zelda3D_AutoModelHeight(int modelId);
float Zelda3D_AutoModelMinY(int modelId);
int Zelda3D_AutoModelExtentXZ(int modelId, float* outX, float* outZ);
int Zelda3D_AutoModelSkinned(int modelId);
int Zelda3D_AutoModelAllBlended(int modelId);
int Zelda3D_AutoModelBoneCount(int modelId);
float Zelda3D_AutoModelBoneLenSum(int modelId, int boneCap);
const char* Zelda3D_AutoModelDefaultAnim(int modelId);
int Zelda3D_AutoModelHasCsab(int modelId, const char* base);
const char* Zelda3D_AutoModelZar(int modelId);
void Zelda3D_AutoModelCsabList(int modelId, char* out, int outSize);
unsigned char* Zelda3D_AutoModelReadZarFile(int modelId, const char* suffix, size_t* outSize);
int Zelda3D_FacialFrameTex(int modelId, int materialIndex, int frame);

#ifdef __cplusplus
}
#endif

#endif // ZELDA3D_RENDER_MODEL_QUERIES_H
