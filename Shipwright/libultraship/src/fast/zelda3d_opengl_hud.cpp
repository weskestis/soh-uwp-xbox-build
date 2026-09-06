// Native OoT HUD quad renderer for the SDL2/OpenGL profile.
#ifdef ENABLE_OPENGL

#include "zelda3d_opengl_internal.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace {

constexpr size_t kMaxQuads = 2048;

struct HudVertex {
    float x;
    float y;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    float u;
    float v;
    uint8_t envRed;
    uint8_t envGreen;
    uint8_t envBlue;
    uint8_t mode;
};

struct HudRun {
    GLuint texture = 0;
    bool repeat = false;
    uint32_t first = 0;
    uint32_t count = 0;
};

struct HudState {
    bool ready = false;
    bool failed = false;
    bool active = false;
    GLuint program = 0;
    GLuint vertexArray = 0;
    GLuint vertexBuffer = 0;
    GLuint whiteTexture = 0;
    GLint viewportUniform = -1;
    int width = 0;
    int height = 0;
    int nextTextureId = 1;
    std::unordered_map<const void*, GLuint> textureCache;
    std::unordered_map<int, GLuint> frameTextures;
    std::vector<HudVertex> vertices;
    std::vector<HudRun> runs;
};

HudState& Hud() {
    static HudState state;
    return state;
}

GLuint UploadTexture(const void* rgba, int width, int height) {
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    if (width <= 0 || height <= 0) {
        width = height = 1;
        rgba = white;
    }
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba ? rgba : white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}

bool EnsureHudResources() {
    HudState& state = Hud();
    if (state.ready)
        return true;
    if (state.failed)
        return false;
    const std::string vertex = R"GLSL(#version 140
in vec2 aPos;
in vec4 aNrm;
in vec2 aUv;
in vec4 aBoneId;
uniform vec2 uViewport;
out vec4 vColor;
out vec2 vUv;
out vec4 vEnvMode;
void main() {
    vec2 ndc = vec2((aPos.x / uViewport.x) * 2.0 - 1.0, 1.0 - (aPos.y / uViewport.y) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aNrm;
    vUv = aUv;
    vEnvMode = aBoneId;
}
)GLSL";
    const std::string fragment = R"GLSL(#version 140
in vec4 vColor;
in vec2 vUv;
in vec4 vEnvMode;
uniform sampler2D uTex;
out vec4 frag;
void main() {
    vec4 texel = texture(uTex, vUv);
    if (vEnvMode.a > 0.75) {
        frag = vec4(mix(vEnvMode.rgb, vColor.rgb, texel.r), vColor.a);
    } else if (vEnvMode.a > 0.25) {
        frag = vec4(mix(vEnvMode.rgb, vColor.rgb, texel.r), texel.a * vColor.a);
    } else {
        frag = texel * vColor;
    }
}
)GLSL";
    state.program = Fast::Zelda3DOpenGL::CompileProgram(vertex, fragment, "HUD");
    if (state.program == 0) {
        state.failed = true;
        return false;
    }
    glUseProgram(state.program);
    glUniform1i(glGetUniformLocation(state.program, "uTex"), 0);
    state.viewportUniform = glGetUniformLocation(state.program, "uViewport");
    glGenVertexArrays(1, &state.vertexArray);
    glGenBuffers(1, &state.vertexBuffer);
    state.whiteTexture = UploadTexture(nullptr, 1, 1);
    state.ready = state.vertexArray != 0 && state.vertexBuffer != 0 && state.whiteTexture != 0 &&
                  state.viewportUniform >= 0;
    state.failed = !state.ready;
    return state.ready;
}

void FlushHud() {
    HudState& state = Hud();
    if (!state.active || state.vertices.empty() || state.runs.empty())
        return;
    Fast::Zelda3DOpenGL::ScopedState restore;
    glUseProgram(state.program);
    glUniform2f(state.viewportUniform, static_cast<float>(state.width), static_cast<float>(state.height));
    glBindVertexArray(state.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, state.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(state.vertices.size() * sizeof(HudVertex)),
                 state.vertices.data(), GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex),
                          reinterpret_cast<const void*>(offsetof(HudVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(HudVertex),
                          reinterpret_cast<const void*>(offsetof(HudVertex, red)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(HudVertex),
                          reinterpret_cast<const void*>(offsetof(HudVertex, u)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(HudVertex),
                          reinterpret_cast<const void*>(offsetof(HudVertex, envRed)));
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glActiveTexture(GL_TEXTURE0);
    for (const HudRun& run : state.runs) {
        glBindTexture(GL_TEXTURE_2D, run.texture != 0 ? run.texture : state.whiteTexture);
        const GLint wrap = run.repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(run.first), static_cast<GLsizei>(run.count));
    }
    state.vertices.clear();
    state.runs.clear();
}

void DrawHud(int textureId, float x, float y, float width, float height, float u0, float v0, float u1, float v1,
             unsigned int tintRgba, unsigned int envRgb, int mode) {
    HudState& state = Hud();
    if (!state.active || state.vertices.size() + 6 > kMaxQuads * 6)
        return;
    GLuint texture = state.whiteTexture;
    auto found = state.frameTextures.find(textureId);
    if (textureId != 0 && found != state.frameTextures.end())
        texture = found->second;
    const uint8_t red = static_cast<uint8_t>((tintRgba >> 24) & 0xff);
    const uint8_t green = static_cast<uint8_t>((tintRgba >> 16) & 0xff);
    const uint8_t blue = static_cast<uint8_t>((tintRgba >> 8) & 0xff);
    const uint8_t alpha = static_cast<uint8_t>(tintRgba & 0xff);
    const uint8_t envRed = static_cast<uint8_t>((envRgb >> 16) & 0xff);
    const uint8_t envGreen = static_cast<uint8_t>((envRgb >> 8) & 0xff);
    const uint8_t envBlue = static_cast<uint8_t>(envRgb & 0xff);
    const uint8_t encodedMode = static_cast<uint8_t>(mode >= 2 ? 255 : (mode == 1 ? 128 : 0));
    const float x1 = x + width;
    const float y1 = y + height;
    const HudVertex quad[6] = {
        { x, y, red, green, blue, alpha, u0, v0, envRed, envGreen, envBlue, encodedMode },
        { x1, y, red, green, blue, alpha, u1, v0, envRed, envGreen, envBlue, encodedMode },
        { x, y1, red, green, blue, alpha, u0, v1, envRed, envGreen, envBlue, encodedMode },
        { x1, y, red, green, blue, alpha, u1, v0, envRed, envGreen, envBlue, encodedMode },
        { x1, y1, red, green, blue, alpha, u1, v1, envRed, envGreen, envBlue, encodedMode },
        { x, y1, red, green, blue, alpha, u0, v1, envRed, envGreen, envBlue, encodedMode },
    };
    const uint32_t first = static_cast<uint32_t>(state.vertices.size());
    state.vertices.insert(state.vertices.end(), quad, quad + 6);
    const bool repeat = u0 < 0.0f || v0 < 0.0f || u1 > 1.0f || v1 > 1.0f;
    if (!state.runs.empty() && state.runs.back().texture == texture && state.runs.back().repeat == repeat &&
        state.runs.back().first + state.runs.back().count == first) {
        state.runs.back().count += 6;
    } else {
        state.runs.push_back({ texture, repeat, first, 6 });
    }
}

} // namespace

extern "C" {

int Zelda3D_Hud_Available(void) {
    return glGetString(GL_VERSION) != nullptr ? 1 : 0;
}

int Zelda3D_Hud_Begin(int* outWidth, int* outHeight) {
    HudState& state = Hud();
    state.active = false;
    Fast::Zelda3DOpenGL::ScopedState restore;
    if (!EnsureHudResources())
        return 0;
    GLint viewport[4] = {};
    glGetIntegerv(GL_VIEWPORT, viewport);
    state.width = viewport[2];
    state.height = viewport[3];
    if (state.width <= 0 || state.height <= 0)
        return 0;
    state.vertices.clear();
    state.runs.clear();
    state.active = true;
    if (outWidth != nullptr)
        *outWidth = state.width;
    if (outHeight != nullptr)
        *outHeight = state.height;
    return 1;
}

int Zelda3D_Hud_Tex(const void* key, const void* rgba, int width, int height) {
    HudState& state = Hud();
    if (!state.active || key == nullptr)
        return 0;
    GLuint texture = 0;
    auto cached = state.textureCache.find(key);
    if (cached != state.textureCache.end()) {
        texture = cached->second;
    } else {
        Fast::Zelda3DOpenGL::ScopedState restore;
        texture = UploadTexture(rgba, width, height);
        state.textureCache[key] = texture;
    }
    const int id = state.nextTextureId++;
    state.frameTextures[id] = texture;
    return id;
}

void Zelda3D_Hud_Draw(int texture, float x, float y, float width, float height, float u0, float v0, float u1,
                      float v1, unsigned int tintRgba) {
    DrawHud(texture, x, y, width, height, u0, v0, u1, v1, tintRgba, 0, 0);
}

void Zelda3D_Hud_DrawEnv(int texture, float x, float y, float width, float height, float u0, float v0, float u1,
                         float v1, unsigned int tintRgba, unsigned int envRgb, int mode) {
    DrawHud(texture, x, y, width, height, u0, v0, u1, v1, tintRgba, envRgb, mode);
}

void Zelda3D_Hud_Flush(void) {
    FlushHud();
}

void Zelda3D_Hud_End(void) {
    HudState& state = Hud();
    FlushHud();
    state.active = false;
    state.frameTextures.clear();
    state.nextTextureId = 1;
}

void Zelda3D_Hud_Shutdown(void) {
    HudState& state = Hud();
    if (!state.ready && !state.failed)
        return;
    Fast::Zelda3DOpenGL::ScopedState restore;
    for (const auto& entry : state.textureCache) {
        GLuint texture = entry.second;
        glDeleteTextures(1, &texture);
    }
    if (state.whiteTexture != 0)
        glDeleteTextures(1, &state.whiteTexture);
    if (state.vertexBuffer != 0)
        glDeleteBuffers(1, &state.vertexBuffer);
    if (state.vertexArray != 0)
        glDeleteVertexArrays(1, &state.vertexArray);
    if (state.program != 0)
        glDeleteProgram(state.program);
    state = {};
}

} // extern "C"

#endif
