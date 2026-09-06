// Shader, buffer, texture, model-cache, and eviction ownership for the OoT3D OpenGL renderer.
#ifdef ENABLE_OPENGL

#include "zelda3d_opengl_internal.h"

#include "fast/backends/zelda3d_tev_glsl.h"
#include "fast/zelda3d_sampler.h"
#include "zelda3d_instrumentation_state.h"
#include "zelda3d_sdl3gpu_shaders.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace Fast::Zelda3DOpenGL {
namespace {

void ReplaceAll(std::string& text, const std::string& from, const std::string& to) {
    size_t position = 0;
    while ((position = text.find(from, position)) != std::string::npos) {
        text.replace(position, from.size(), to);
        position += to.size();
    }
}

void AdaptShaderForOpenGL(std::string& source) {
    ReplaceAll(source, "#version 450", "#version 140");
    for (int location = 0; location < 8; ++location) {
        ReplaceAll(source, "layout(location=" + std::to_string(location) + ") ", "");
    }
    ReplaceAll(source, "layout(set=1, binding=0, std140)", "layout(std140)");
    ReplaceAll(source, "layout(set=1, binding=1, std140)", "layout(std140)");
    ReplaceAll(source, "layout(set=3, binding=0, std140)", "layout(std140)");
    ReplaceAll(source, "layout(set=2, binding=0) ", "");
    ReplaceAll(source, "layout(set=2, binding=1) ", "");
    ReplaceAll(source, "layout(set=2, binding=2) ", "");
    // GLSL 1.40 has uniform blocks but not named block instances. Removing the two instance
    // qualifiers keeps the same std140 block names/bytes while exposing their members globally.
    ReplaceAll(source, "} ubo;", "};");
    ReplaceAll(source, "} bones;", "};");
    ReplaceAll(source, "ubo.", "");
    ReplaceAll(source, "bones.", "");
}

GLuint CompileStage(GLenum type, const std::string& source, const char* label) {
    GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) {
        return shader;
    }
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(static_cast<size_t>(std::max(length, 1)));
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    std::fprintf(stderr, "[Zelda3D_GL] %s shader compile failed: %s\n", label, log.data());
    glDeleteShader(shader);
    return 0;
}

GLuint UploadTexture(int width, int height, const unsigned char* rgba, int sourceLevels) {
    if (width <= 0 || height <= 0) {
        width = height = 1;
        rgba = nullptr;
        sourceLevels = 1;
    }
    int fullLevels = 1;
    for (int size = std::max(width, height); size > 1; size >>= 1) {
        ++fullLevels;
    }
    const bool authored = sourceLevels > 1 && rgba != nullptr;
    const int levels = authored ? std::min(sourceLevels, fullLevels) : fullLevels;
    static const unsigned char white[4] = { 255, 255, 255, 255 };

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
    if (authored) {
        size_t offset = 0;
        int levelWidth = width;
        int levelHeight = height;
        for (int level = 0; level < levels; ++level) {
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, levelWidth, levelHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         rgba + offset);
            offset += static_cast<size_t>(levelWidth) * static_cast<size_t>(levelHeight) * 4;
            levelWidth = std::max(levelWidth / 2, 1);
            levelHeight = std::max(levelHeight / 2, 1);
        }
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba ? rgba : white);
        if (levels > 1) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    Zelda3DFast::ReportProgress();
    return texture;
}

void DeleteModel(GlModel& model) {
    if (model.vertexBuffer != 0) {
        glDeleteBuffers(1, &model.vertexBuffer);
    }
    if (!model.textures.empty()) {
        glDeleteTextures(static_cast<GLsizei>(model.textures.size()), model.textures.data());
    }
    model = {};
}

} // namespace

GLuint CompileProgram(const std::string& vertexSource, const std::string& fragmentSource, const char* label) {
    GLuint vertex = CompileStage(GL_VERTEX_SHADER, vertexSource, label);
    GLuint fragment = CompileStage(GL_FRAGMENT_SHADER, fragmentSource, label);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0)
            glDeleteShader(vertex);
        if (fragment != 0)
            glDeleteShader(fragment);
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    static const char* attributes[8] = { "aPos", "aNrm", "aUv", "aBoneId", "aBoneW", "aColor", "aUv1", "aUv2" };
    for (GLuint location = 0; location < 8; ++location) {
        glBindAttribLocation(program, location, attributes[location]);
    }
    glBindFragDataLocation(program, 0, "frag");
    glLinkProgram(program);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glDetachShader(program, vertex);
    glDetachShader(program, fragment);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    if (ok == GL_TRUE) {
        return program;
    }
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(static_cast<size_t>(std::max(length, 1)));
    glGetProgramInfoLog(program, length, nullptr, log.data());
    std::fprintf(stderr, "[Zelda3D_GL] %s program link failed: %s\n", label, log.data());
    glDeleteProgram(program);
    return 0;
}

bool EnsureResources() {
    RendererState& state = State();
    if (state.resourcesReady)
        return true;
    if (state.resourcesFailed)
        return false;

    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    GLint maxBlock = 0;
    GLint maxAttributes = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxBlock);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttributes);
    if (major < 3 || (major == 3 && minor < 1) || maxBlock < static_cast<GLint>(Zelda3DSg::kBonesBytes) ||
        maxAttributes < 8) {
        std::fprintf(stderr,
                     "[Zelda3D_GL] OpenGL 3.1+, 8 attributes, and a %u-byte uniform block are required "
                     "(got %d.%d, attribs=%d, block=%d)\n",
                     Zelda3DSg::kBonesBytes, major, minor, maxAttributes, maxBlock);
        state.resourcesFailed = true;
        return false;
    }

    static const char* unpack =
        "vec4 zelda3dUnpackUnorm4x8(uint v) { return vec4(float(v & 255u), float((v >> 8) & 255u), "
        "float((v >> 16) & 255u), float((v >> 24) & 255u)) * (1.0 / 255.0); }\n";
    std::string tev = unpack;
    tev += Fast::Zelda3DTev::kGenericFunctions;
    ReplaceAll(tev, "unpackUnorm4x8", "zelda3dUnpackUnorm4x8");
    std::string vertexSource;
    std::string fragmentSource;
    std::string error;
    if (!Fast::Zelda3DSdl3GpuShaders::BuildSources(tev.c_str(), "", "", vertexSource, fragmentSource, error)) {
        std::fprintf(stderr, "[Zelda3D_GL] shader template failed: %s\n", error.c_str());
        state.resourcesFailed = true;
        return false;
    }
    AdaptShaderForOpenGL(vertexSource);
    AdaptShaderForOpenGL(fragmentSource);
    state.program = CompileProgram(vertexSource, fragmentSource, "model");
    if (state.program == 0) {
        state.resourcesFailed = true;
        return false;
    }

    GLuint commonIndex = glGetUniformBlockIndex(state.program, "UBO");
    GLuint bonesIndex = glGetUniformBlockIndex(state.program, "UBOBones");
    if (commonIndex == GL_INVALID_INDEX || bonesIndex == GL_INVALID_INDEX) {
        std::fprintf(stderr, "[Zelda3D_GL] required model uniform blocks were optimized out or missing\n");
        glDeleteProgram(state.program);
        state.program = 0;
        state.resourcesFailed = true;
        return false;
    }
    glUniformBlockBinding(state.program, commonIndex, 0);
    glUniformBlockBinding(state.program, bonesIndex, 1);
    glUseProgram(state.program);
    glUniform1i(glGetUniformLocation(state.program, "uTex"), 0);
    glUniform1i(glGetUniformLocation(state.program, "uTex1"), 1);
    glUniform1i(glGetUniformLocation(state.program, "uTex2"), 2);

    glGenVertexArrays(1, &state.vertexArray);
    glGenBuffers(1, &state.commonUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, state.commonUbo);
    glBufferData(GL_UNIFORM_BUFFER, Zelda3DSg::kCommonBytes, nullptr, GL_STREAM_DRAW);
    glGenBuffers(1, &state.bonesUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, state.bonesUbo);
    glBufferData(GL_UNIFORM_BUFFER, Zelda3DSg::kBonesBytes, nullptr, GL_STREAM_DRAW);
    state.whiteTexture = UploadTexture(1, 1, nullptr, 1);

    state.resourcesReady = state.vertexArray != 0 && state.commonUbo != 0 && state.bonesUbo != 0 &&
                           state.whiteTexture != 0;
    if (!state.resourcesReady) {
        std::fprintf(stderr, "[Zelda3D_GL] failed to allocate required model resources\n");
        state.resourcesFailed = true;
        return false;
    }
    std::fprintf(stderr, "[Zelda3D_GL] renderer ready (%s)\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    return true;
}

GlModel* EnsureModel(int modelId) {
    RendererState& state = State();
    GlModel& model = state.models[modelId];
    if (model.uploaded)
        return &model;
    if (model.failed)
        return nullptr;
    const Zelda3DGlGroup* sourceGroups = nullptr;
    const Zelda3DGlTex* sourceTextures = nullptr;
    int groupCount = 0;
    int textureCount = 0;
    if (state.provider == nullptr ||
        state.provider(modelId, &sourceGroups, &groupCount, &sourceTextures, &textureCount) == 0 || groupCount <= 0 ||
        sourceGroups == nullptr || textureCount < 0 || (textureCount > 0 && sourceTextures == nullptr)) {
        std::fprintf(stderr, "[Zelda3D_GL] model %d unavailable from provider\n", modelId);
        model.failed = true;
        return nullptr;
    }

    std::vector<Zelda3DGlVtx> vertices;
    for (int groupIndex = 0; groupIndex < groupCount; ++groupIndex) {
        const Zelda3DGlGroup& source = sourceGroups[groupIndex];
        if (source.vertCount < 0 || (source.vertCount > 0 && source.verts == nullptr)) {
            std::fprintf(stderr, "[Zelda3D_GL] model %d group %d has invalid vertices\n", modelId, groupIndex);
            model.failed = true;
            return nullptr;
        }
        GlGroup group;
        group.material = source;
        group.material.verts = nullptr;
        group.first = static_cast<uint32_t>(vertices.size());
        group.count = static_cast<uint32_t>(source.vertCount);
        if (source.vertCount > 0) {
            for (int axis = 0; axis < 3; ++axis)
                group.min[axis] = group.max[axis] = source.verts[0].pos[axis];
            for (int vertexIndex = 1; vertexIndex < source.vertCount; ++vertexIndex) {
                for (int axis = 0; axis < 3; ++axis) {
                    group.min[axis] = std::min(group.min[axis], source.verts[vertexIndex].pos[axis]);
                    group.max[axis] = std::max(group.max[axis], source.verts[vertexIndex].pos[axis]);
                }
            }
            group.hasBounds = true;
            vertices.insert(vertices.end(), source.verts, source.verts + source.vertCount);
        }
        model.groups.push_back(group);
    }
    if (!vertices.empty()) {
        for (int axis = 0; axis < 3; ++axis)
            model.min[axis] = model.max[axis] = vertices[0].pos[axis];
        for (const Zelda3DGlVtx& vertex : vertices) {
            for (int axis = 0; axis < 3; ++axis) {
                model.min[axis] = std::min(model.min[axis], vertex.pos[axis]);
                model.max[axis] = std::max(model.max[axis], vertex.pos[axis]);
            }
        }
        model.hasBounds = true;
        glGenBuffers(1, &model.vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, model.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Zelda3DGlVtx)), vertices.data(),
                     GL_STATIC_DRAW);
    }
    for (int textureIndex = 0; textureIndex < textureCount; ++textureIndex) {
        model.textures.push_back(UploadTexture(sourceTextures[textureIndex].w, sourceTextures[textureIndex].h,
                                               sourceTextures[textureIndex].rgba, sourceTextures[textureIndex].levels));
    }
    model.uploaded = model.vertexBuffer != 0;
    model.failed = !model.uploaded;
    if (model.uploaded) {
        std::fprintf(stderr, "[Zelda3D_GL] uploaded model %d: %d groups, %d textures, %zu vertices\n", modelId,
                     groupCount, textureCount, vertices.size());
        Zelda3DFast::ReportProgress();
        return &model;
    }
    return nullptr;
}

GLenum NormalizeWrap(unsigned wrap) {
    switch (wrap) {
        case 0x8370:
            return GL_MIRRORED_REPEAT;
        case 0x2900:
        case 0x812F:
            return GL_CLAMP_TO_EDGE;
        default:
            return GL_REPEAT;
    }
}

void BindTexture(GLuint texture, unsigned minFilter, unsigned magFilter, unsigned wrapS, unsigned wrapT) {
    glBindTexture(GL_TEXTURE_2D, texture != 0 ? texture : State().whiteTexture);
    const Zelda3DSamplerFilter filter = ResolveZelda3DSamplerFilter(minFilter, magFilter);
    GLint glMin = filter.minification == Zelda3DTextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
    if (filter.mipmap == Zelda3DMipmapFilter::Nearest)
        glMin = filter.minification == Zelda3DTextureFilter::Nearest ? GL_NEAREST_MIPMAP_NEAREST
                                                                     : GL_LINEAR_MIPMAP_NEAREST;
    else if (filter.mipmap == Zelda3DMipmapFilter::Linear)
        glMin = filter.minification == Zelda3DTextureFilter::Nearest ? GL_NEAREST_MIPMAP_LINEAR
                                                                     : GL_LINEAR_MIPMAP_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glMin);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    filter.magnification == Zelda3DTextureFilter::Nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, NormalizeWrap(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, NormalizeWrap(wrapT));
}

void ApplyPendingEviction() {
    RendererState& state = State();
    if (!state.evictionPending)
        return;
    state.evictionPending = false;
    for (auto it = state.models.begin(); it != state.models.end();) {
        if (it->first >= state.evictionFirst && it->first < state.evictionEnd) {
            DeleteModel(it->second);
            it = state.models.erase(it);
        } else {
            ++it;
        }
    }
}

void DestroyResources() {
    RendererState& state = State();
    for (auto& entry : state.models)
        DeleteModel(entry.second);
    state.models.clear();
    if (state.whiteTexture != 0)
        glDeleteTextures(1, &state.whiteTexture);
    if (state.commonUbo != 0)
        glDeleteBuffers(1, &state.commonUbo);
    if (state.bonesUbo != 0)
        glDeleteBuffers(1, &state.bonesUbo);
    if (state.vertexArray != 0)
        glDeleteVertexArrays(1, &state.vertexArray);
    if (state.program != 0)
        glDeleteProgram(state.program);
    Zelda3DModelProvider provider = state.provider;
    state = {};
    state.provider = provider;
}

} // namespace Fast::Zelda3DOpenGL

#endif
