// Native OoT3D model renderer entry points for the SDL2/OpenGL profile.
#pragma once

#ifdef ENABLE_OPENGL

#include "fast/zelda3d_model_provider.h"

namespace Fast::Zelda3DOpenGL {

void SetModelProvider(Zelda3DModelProvider provider);
void RequestEvictRange(int firstModelId, int endModelId);
void BeginPass();
void DrawModel(int modelId, const float* modelProjection, const float* modelView, int lit, int invertY,
               unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha,
               float aspectAdjustment, const float* boneMatrices, int boneCount, unsigned long long visibleMeshMask,
               int sky, float uvOffsetU, float uvOffsetV, const void* materialTextures,
               const void* materialConstants, const void* materialUvs, int forceUnlit,
               const float* lightDirectionOverride = nullptr, const float* sphereNormalOverride = nullptr);
void EndPass();
void ClearOverlayDepth();
bool GroupBounds(int modelId, int groupIndex, float* outMin, float* outMax);
int GeomScanDump(int* modelIds, float* mins, float* maxs, int capacity);
void Shutdown();

} // namespace Fast::Zelda3DOpenGL

#endif
