#include "fast/backends/zelda3d_opengl.h"
#include "fast/zelda3d_instrumentation.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>

#include <cstdio>
#include <cstring>

extern "C" int Zelda3D_Hud_Begin(int*, int*);
extern "C" void Zelda3D_Hud_Draw(int, float, float, float, float, float, float, float, float, unsigned int);
extern "C" void Zelda3D_Hud_End(void);
extern "C" void Zelda3D_Hud_Shutdown(void);

extern "C" {
float gZelda3dLightDirWorld[3] = { 0.0f, 0.0f, -1.0f };
int gZelda3dFaceCull = 1;
int gZelda3dFaceCullFlip = 0;
int gZelda3dFogEnable = 0;
float gZelda3dFogColor[3] = {};
float gZelda3dFogMul = 0.0f;
float gZelda3dFogOffset = 0.0f;
int gZelda3dFog3dOn = 0;
float gZelda3dFog3d[8] = {};
int gZelda3dWorldLit = 0;
float gZelda3dAmbient[3] = { 1.0f, 1.0f, 1.0f };
float gZelda3dLight1Col[3] = { 1.0f, 1.0f, 1.0f };
float gZelda3dLight2Dir[3] = { 0.0f, 0.0f, 1.0f };
float gZelda3dLight2Col[3] = {};
float gZelda3dAmbientLightCount = 1.0f;
int gZelda3dHlGroup = -1;
int gZelda3dTraceModelId = -1;
int gZelda3dStateCheck = -1;
int g_sgDumpModel = -1;
int gZelda3dSgDrawOnly = -1;
int gZelda3dSgDrawSkip = -1;
int gZelda3dSgDrawSkipAfter = -1;
int gZelda3dSgModelOnly = -1;
int gZelda3dSgDrawList = 0;
}

extern "C" int Zelda3D_SgDrawIsolationIncludes(int modelId, int drawIndex) {
    return (gZelda3dSgModelOnly < 0 || modelId == gZelda3dSgModelOnly) &&
           (gZelda3dSgDrawOnly < 0 || drawIndex == gZelda3dSgDrawOnly) && drawIndex != gZelda3dSgDrawSkip &&
           (gZelda3dSgDrawSkipAfter < 0 || drawIndex <= gZelda3dSgDrawSkipAfter);
}

namespace Zelda3DFast {
void ReportProgress() {
}
}

namespace {

Zelda3DGlVtx vertices[3] = {};
Zelda3DGlGroup group = {};
const unsigned char pixels[16] = { 255, 255, 255, 255, 255, 255, 255, 255,
                                   255, 255, 255, 255, 255, 255, 255, 255 };
const Zelda3DGlTex texture = { pixels, 2, 2, 1 };

int Provider(int modelId, const Zelda3DGlGroup** groups, int* groupCount, const Zelda3DGlTex** textures,
             int* textureCount) {
    if (modelId != 1)
        return 0;
    *groups = &group;
    *groupCount = 1;
    *textures = &texture;
    *textureCount = 1;
    return 1;
}

bool InitModel() {
    const float positions[3][3] = { { -0.8f, -0.8f, 0.0f }, { 0.8f, -0.8f, 0.0f }, { 0.0f, 0.8f, 0.0f } };
    for (int i = 0; i < 3; ++i) {
        std::memcpy(vertices[i].pos, positions[i], sizeof(positions[i]));
        vertices[i].nrm[2] = 1.0f;
        vertices[i].weights[0] = 1.0f;
        vertices[i].color[0] = 1.0f;
        vertices[i].color[3] = 1.0f;
    }
    group.verts = vertices;
    group.vertCount = 3;
    group.texIndex = 0;
    group.minFilter = GL_LINEAR;
    group.magFilter = GL_LINEAR;
    group.wrapS = GL_CLAMP_TO_EDGE;
    group.wrapT = GL_CLAMP_TO_EDGE;
    group.depthWrite = 1;
    group.depthTest = 1;
    group.depthFunc = GL_LESS;
    group.alphaFunc = GL_ALWAYS;
    group.meshId = -1;
    group.materialIndex = 0;
    group.hasColor = 1;
    group.matDiffuse[0] = 1.0f;
    group.matDiffuse[3] = 1.0f;
    group.combScaleRGB = 1.0f;
    group.combConstScaleRGB = 1.0f;
    group.uv0Scale[0] = group.uv0Scale[1] = 1.0f;
    group.uv1Scale[0] = group.uv1Scale[1] = 1.0f;
    group.uv2Scale[0] = group.uv2Scale[1] = 1.0f;
    group.coord0Mapping = group.coord1Mapping = group.coord2Mapping = 1;
    return true;
}

bool PixelNear(const unsigned char* pixel, int red, int green, int blue) {
    return pixel[0] >= red && pixel[1] >= green && pixel[2] >= blue;
}

} // namespace

int main() {
    auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay display = getPlatformDisplay ? getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, nullptr, nullptr)
                                            : eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || eglInitialize(display, nullptr, nullptr) != EGL_TRUE) {
        std::fprintf(stderr, "EGL display init failed: %#x\n", eglGetError());
        return 2;
    }
    eglBindAPI(EGL_OPENGL_API);
    const EGLint configAttributes[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                        EGL_DEPTH_SIZE, 24, EGL_NONE };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (eglChooseConfig(display, configAttributes, &config, 1, &count) != EGL_TRUE || count != 1)
        return 3;
    const EGLint contextAttributes[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
                                         EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
                                         EGL_NONE };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
    const EGLint surfaceAttributes[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
    EGLSurface surface = eglCreatePbufferSurface(display, config, surfaceAttributes);
    if (context == EGL_NO_CONTEXT || surface == EGL_NO_SURFACE ||
        eglMakeCurrent(display, surface, surface, context) != EGL_TRUE) {
        std::fprintf(stderr, "EGL context init failed: %#x\n", eglGetError());
        return 4;
    }
    std::printf("GL=%s\n", glGetString(GL_VERSION));
    InitModel();
    Fast::Zelda3DOpenGL::SetModelProvider(Provider);
    glViewport(0, 0, 64, 64);
    glScissor(0, 0, 64, 64);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLint beforeProgram = 0;
    GLint beforeActiveTexture = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &beforeProgram);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &beforeActiveTexture);
    Fast::Zelda3DOpenGL::BeginPass();
    const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    Fast::Zelda3DOpenGL::DrawModel(1, identity, identity, 0, 0, 255, 255, 255, 255, 1.0f, nullptr, 0,
                                   ~0ull, 0, 0.0f, 0.0f, nullptr, nullptr, nullptr, 0);
    unsigned char modelPixel[4] = {};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, modelPixel);
    GLint afterProgram = -1;
    GLint afterActiveTexture = -1;
    glGetIntegerv(GL_CURRENT_PROGRAM, &afterProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &afterActiveTexture);
    Fast::Zelda3DOpenGL::EndPass();
    if (!PixelNear(modelPixel, 240, 0, 0) || modelPixel[1] > 8 || modelPixel[2] > 8 ||
        beforeProgram != afterProgram || beforeActiveTexture != afterActiveTexture) {
        std::fprintf(stderr, "model/state smoke failed: rgba=%u,%u,%u,%u program=%d/%d active=%#x/%#x\n",
                     modelPixel[0], modelPixel[1], modelPixel[2], modelPixel[3], beforeProgram, afterProgram,
                     beforeActiveTexture, afterActiveTexture);
        return 5;
    }

    int width = 0;
    int height = 0;
    if (!Zelda3D_Hud_Begin(&width, &height) || width != 64 || height != 64)
        return 6;
    Zelda3D_Hud_Draw(0, 0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0x00ff00ffu);
    Zelda3D_Hud_End();
    unsigned char hudPixel[4] = {};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, hudPixel);
    if (!PixelNear(hudPixel, 0, 240, 0) || hudPixel[0] > 8 || hudPixel[2] > 8) {
        std::fprintf(stderr, "HUD smoke failed: rgba=%u,%u,%u,%u\n", hudPixel[0], hudPixel[1], hudPixel[2],
                     hudPixel[3]);
        return 7;
    }

    Zelda3D_Hud_Shutdown();
    Fast::Zelda3DOpenGL::Shutdown();
    const GLenum glError = glGetError();
    if (glError != GL_NO_ERROR) {
        std::fprintf(stderr, "OpenGL smoke left error %#x\n", glError);
        return 8;
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(display, surface);
    eglDestroyContext(display, context);
    eglTerminate(display);
    std::printf("Zelda3D OpenGL model/HUD smoke: PASS\n");
    return 0;
}
