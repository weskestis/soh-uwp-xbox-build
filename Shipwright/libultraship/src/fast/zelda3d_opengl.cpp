// Stable C-ABI adapters owned by the SDL2/OpenGL OoT3D renderer.
#ifdef ENABLE_OPENGL

#include "fast/backends/zelda3d_opengl.h"

extern "C" int Zelda3D_Sg_GroupBounds(int modelId, int groupIndex, float* outMin, float* outMax) {
    return Fast::Zelda3DOpenGL::GroupBounds(modelId, groupIndex, outMin, outMax) ? 1 : 0;
}

extern "C" int Zelda3D_GeomScanDump(int* modelIds, float* mins, float* maxs, int capacity) {
    return Fast::Zelda3DOpenGL::GeomScanDump(modelIds, mins, maxs, capacity);
}

#endif
