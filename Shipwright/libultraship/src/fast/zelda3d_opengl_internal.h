// Private contracts between the focused OoT3D OpenGL renderer owners.
#pragma once

#ifdef ENABLE_OPENGL

#include "fast/backends/zelda3d_opengl.h"
#include "fast/zelda3d_model_types.h"
#include "fast/zelda3d_sg_ubo.h"

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__APPLE__)
#include <GL/glew.h>
#elif defined(USE_OPENGLES)
#include <GLES3/gl3.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Fast::Zelda3DOpenGL {

struct GlGroup {
    Zelda3DGlGroup material{};
    uint32_t first = 0;
    uint32_t count = 0;
    bool hasBounds = false;
    float min[3] = {};
    float max[3] = {};
};

struct GlModel {
    bool uploaded = false;
    bool failed = false;
    GLuint vertexBuffer = 0;
    std::vector<GlGroup> groups;
    std::vector<GLuint> textures;
    bool hasBounds = false;
    float min[3] = {};
    float max[3] = {};
};

struct GeomRecord {
    int modelId = 0;
    float min[3] = {};
    float max[3] = {};
};

struct RendererState {
    Zelda3DModelProvider provider = nullptr;
    std::unordered_map<int, GlModel> models;
    std::vector<GeomRecord> geometryCurrent;
    std::vector<GeomRecord> geometryLast;
    GLuint program = 0;
    GLuint vertexArray = 0;
    GLuint commonUbo = 0;
    GLuint bonesUbo = 0;
    GLuint whiteTexture = 0;
    bool resourcesReady = false;
    bool resourcesFailed = false;
    bool passActive = false;
    bool evictionPending = false;
    int evictionFirst = 0;
    int evictionEnd = 0;
    int drawIndex = 0;
};

// OpenGL state not represented in Fast3D's cache must be restored exactly after every native draw.
class ScopedState {
  public:
    ScopedState();
    ~ScopedState();
    ScopedState(const ScopedState&) = delete;
    ScopedState& operator=(const ScopedState&) = delete;

  private:
    GLint vertexArray = 0;
    GLint program = 0;
    GLint arrayBuffer = 0;
    GLint elementBuffer = 0;
    GLint uniformBuffer = 0;
    GLint uniformBindings[2] = {};
    GLint activeTexture = GL_TEXTURE0;
    GLint textures[3] = {};
    GLint blendSrcRgb = GL_ONE;
    GLint blendDstRgb = GL_ZERO;
    GLint blendSrcAlpha = GL_ONE;
    GLint blendDstAlpha = GL_ZERO;
    GLint blendEquationRgb = GL_FUNC_ADD;
    GLint blendEquationAlpha = GL_FUNC_ADD;
    GLint depthFunc = GL_LESS;
    GLint cullMode = GL_BACK;
    GLint frontFace = GL_CCW;
    GLint viewport[4] = {};
    GLint scissorBox[4] = {};
    GLint unpackAlignment = 4;
    GLfloat blendColor[4] = {};
    GLfloat polygonOffsetFactor = 0.0f;
    GLfloat polygonOffsetUnits = 0.0f;
    GLdouble depthClear = 1.0;
    GLboolean blend = GL_FALSE;
    GLboolean cull = GL_FALSE;
    GLboolean depth = GL_FALSE;
    GLboolean scissor = GL_FALSE;
    GLboolean polygonOffset = GL_FALSE;
    GLboolean depthMask = GL_TRUE;
    GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
};

RendererState& State();
bool EnsureResources();
GlModel* EnsureModel(int modelId);
void ApplyPendingEviction();
void DestroyResources();
GLuint CompileProgram(const std::string& vertexSource, const std::string& fragmentSource, const char* label);
GLenum NormalizeWrap(unsigned wrap);
void BindTexture(GLuint texture, unsigned minFilter, unsigned magFilter, unsigned wrapS, unsigned wrapT);

} // namespace Fast::Zelda3DOpenGL

#endif
