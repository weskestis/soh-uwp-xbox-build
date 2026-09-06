// Immediate ordered draw pass for OoT3D models in the SDL2/OpenGL profile.
#ifdef ENABLE_OPENGL

#include "zelda3d_opengl_internal.h"

#include "fast/zelda3d_instrumentation.h"
#include "fast/zelda3d_material_overrides.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

using Zelda3DSg::SgUbo;

extern "C" float gZelda3dLightDirWorld[3];
extern "C" int gZelda3dFaceCull;
extern "C" int gZelda3dFaceCullFlip;
extern "C" int gZelda3dFogEnable;
extern "C" float gZelda3dFogColor[3];
extern "C" float gZelda3dFogMul;
extern "C" float gZelda3dFogOffset;
extern "C" int gZelda3dFog3dOn;
extern "C" float gZelda3dFog3d[8];
extern "C" int gZelda3dWorldLit;
extern "C" float gZelda3dAmbient[3];
extern "C" float gZelda3dLight1Col[3];
extern "C" float gZelda3dLight2Dir[3];
extern "C" float gZelda3dLight2Col[3];
extern "C" float gZelda3dAmbientLightCount;
extern "C" int gZelda3dHlGroup;

namespace Fast::Zelda3DOpenGL {
namespace {

constexpr float kWordmarkLightAmbient = 0.18f;

float PreScaleTranslation(float translation, float scale, float bakedTranslation) {
    return scale != 0.0f ? translation / scale : bakedTranslation;
}

uint32_t Quantize(float value) {
    int integer = static_cast<int>(value * 255.0f + 0.5f);
    return static_cast<uint32_t>(std::clamp(integer, 0, 255));
}

GLuint TextureAt(const GlModel& model, int index) {
    return index >= 0 && index < static_cast<int>(model.textures.size()) ? model.textures[index]
                                                                          : State().whiteTexture;
}

void SeedIsolationControls() {
    static bool seeded = false;
    if (seeded)
        return;
    seeded = true;
    if (const char* value = std::getenv("ZELDA3D_SG_DRAWONLY"))
        gZelda3dSgDrawOnly = std::atoi(value);
    if (const char* value = std::getenv("ZELDA3D_SG_DRAWSKIP"))
        gZelda3dSgDrawSkip = std::atoi(value);
    if (const char* value = std::getenv("ZELDA3D_SG_DRAWSKIP_AFTER"))
        gZelda3dSgDrawSkipAfter = std::atoi(value);
    if (const char* value = std::getenv("ZELDA3D_SG_MODELONLY"))
        gZelda3dSgModelOnly = std::atoi(value);
    if (const char* value = std::getenv("ZELDA3D_SG_DRAWLIST"))
        gZelda3dSgDrawList = value[0] != '0';
}

void FillBaseUbo(SgUbo& base, const float* modelProjection, const float* modelView, int lit, int invertY,
                 unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha,
                 float aspectAdjustment, const float* boneMatrices, int boneCount, int sky, float uvOffsetU,
                 float uvOffsetV, const float* lightDirectionOverride, const float* sphereNormalOverride) {
    std::memcpy(base.uMP, modelProjection, sizeof(base.uMP));
    base.uMP[0] *= aspectAdjustment;
    base.uMP[4] *= aspectAdjustment;
    base.uMP[8] *= aspectAdjustment;
    base.uMP[12] *= aspectAdjustment;
    std::memcpy(base.uMV, modelView, sizeof(base.uMV));
    for (int bone = 0; bone < ZELDA3D_GL_MAX_BONES; ++bone) {
        for (int element = 0; element < 16; ++element)
            base.uBones[bone * 16 + element] = element % 5 == 0 ? 1.0f : 0.0f;
    }
    if (boneMatrices != nullptr && boneCount > 0) {
        const int count = std::min(boneCount, ZELDA3D_GL_MAX_BONES);
        for (int bone = 0; bone < count; ++bone) {
            const float* source = boneMatrices + bone * 16;
            float* destination = base.uBones + bone * 16;
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column < 4; ++column)
                    destination[column * 4 + row] = source[row * 4 + column];
        }
    }
    base.uParams[0] = invertY ? -1.0f : 1.0f;
    base.uParams[1] = lit ? 1.0f : 0.0f;
    base.uTintSkin[0] = red / 255.0f;
    base.uTintSkin[1] = green / 255.0f;
    base.uTintSkin[2] = blue / 255.0f;
    base.uTintSkin[3] = boneMatrices != nullptr && boneCount > 0 ? 1.0f : 0.0f;
    if (sphereNormalOverride != nullptr) {
        std::memcpy(base.uSphNrm0, sphereNormalOverride, 3 * sizeof(float));
        std::memcpy(base.uSphNrm1, sphereNormalOverride + 3, 3 * sizeof(float));
        std::memcpy(base.uSphNrm2, sphereNormalOverride + 6, 3 * sizeof(float));
        base.uSphNrm0[3] = 1.0f;
    }
    if (lightDirectionOverride != nullptr) {
        float direction[3] = {
            modelView[0] * lightDirectionOverride[0] + modelView[4] * lightDirectionOverride[1] +
                modelView[8] * lightDirectionOverride[2],
            modelView[1] * lightDirectionOverride[0] + modelView[5] * lightDirectionOverride[1] +
                modelView[9] * lightDirectionOverride[2],
            modelView[2] * lightDirectionOverride[0] + modelView[6] * lightDirectionOverride[1] +
                modelView[10] * lightDirectionOverride[2],
        };
        const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                                       direction[2] * direction[2]);
        if (length > 1e-6f)
            for (float& component : direction)
                component /= length;
        std::memcpy(base.uLightDir, direction, sizeof(direction));
    } else {
        std::memcpy(base.uLightDir, gZelda3dLightDirWorld, 3 * sizeof(float));
    }
    base.uLightDir[3] = sky ? 1.0f : 0.0f;
    std::memcpy(base.uLightDir2, gZelda3dLight2Dir, 3 * sizeof(float));
    base.uSheen[0] = lightDirectionOverride != nullptr ? kWordmarkLightAmbient : 0.0f;
    base.uTex0Xf[0] = 1.0f;
    base.uTex0Xf[1] = 1.0f;
    base.uExtra[0] = alpha / 255.0f;
    base.uExtra[1] = uvOffsetU;
    base.uExtra[2] = uvOffsetV;
    std::memcpy(base.uFog, gZelda3dFogColor, 3 * sizeof(float));
    base.uFog[3] = gZelda3dFogEnable ? 1.0f : 0.0f;
    base.uFog2[0] = gZelda3dFogMul;
    base.uFog2[1] = gZelda3dFogOffset;
    std::memcpy(base.uFog3d0, gZelda3dFog3d, 4 * sizeof(float));
    std::memcpy(base.uFog3d1, gZelda3dFog3d + 4, 4 * sizeof(float));
}

void FillGroupUbo(SgUbo& ubo, const Zelda3DGlGroup& group, int lit, int forceUnlit, float uvOffsetU,
                  float uvOffsetV, const std::unordered_map<int, Zelda3DMatConstOv>* constantOverrides,
                  const std::unordered_map<int, Zelda3DMatUvOv>* uvOverrides) {
    float groupU = uvOffsetU;
    float groupV = uvOffsetV;
    const Zelda3DMatUvOv* uvOverride = nullptr;
    if (uvOverrides != nullptr) {
        auto found = uvOverrides->find(group.materialIndex);
        if (found != uvOverrides->end()) {
            uvOverride = &found->second;
            groupU += uvOverride->u;
            groupV += uvOverride->v;
        }
    }
    ubo.uExtra[1] = groupU;
    ubo.uExtra[2] = groupV;
    ubo.uParams[2] = group.alphaTest ? group.alphaRef : 0.0f;
    ubo.uParams[3] = group.polygonOffset;
    ubo.uTevCtl[3] = group.alphaTest ? static_cast<float>((group.alphaFunc & 7u) + 1u) : 0.0f;
    ubo.uSheen[3] = static_cast<float>(group.coord0Mapping);
    ubo.uTevCtl[1] = static_cast<float>(group.coord1Mapping);
    ubo.uTex0Xf[0] = group.uv0Scale[0];
    ubo.uTex0Xf[1] = group.uv0Scale[1];
    ubo.uTex0Xf[2] = group.uv0Trans[0];
    ubo.uTex0Xf[3] = group.uv0Trans[1];
    if (gZelda3dFog3dOn && group.fogEnabled)
        ubo.uFog[3] = 2.0f;
    ubo.uExtra[3] = group.vertexLighting ? group.combScaleRGB : 1.0f;
    const bool ambientGroup = group.vertexLighting && gZelda3dWorldLit && !forceUnlit;
    for (int component = 0; component < 3; ++component) {
        ubo.uAmbient[component] = gZelda3dAmbient[component] * group.matAmbient[component];
        ubo.uLitDif1[component] = group.matDiffuse[component] * gZelda3dLight1Col[component];
        ubo.uLitDif2[component] = group.matDiffuse[component] * gZelda3dLight2Col[component];
    }
    ubo.uAmbient[3] = ambientGroup ? (lit ? 1.0f : gZelda3dAmbientLightCount) : 0.0f;
    std::memcpy(ubo.uMatDiffuse, group.matDiffuse, sizeof(ubo.uMatDiffuse));
    ubo.uPrimaryCtl[0] = group.hasColor ? 1.0f : 0.0f;
    ubo.uPrimaryCtl[1] = group.fragmentLighting ? 1.0f : 0.0f;
    ubo.uLitDif1[3] = group.matDiffuse[3];
    ubo.uLitDif2[3] = group.matDiffuse[3];

    int constantIndex = group.combConstIdx & 7;
    if (constantIndex > 5)
        constantIndex = 0;
    std::memcpy(ubo.uMatConst, group.matConstant[constantIndex], 3 * sizeof(float));
    const bool constantBlack = group.matConstant[constantIndex][0] < 1e-4f &&
                               group.matConstant[constantIndex][1] < 1e-4f &&
                               group.matConstant[constantIndex][2] < 1e-4f;
    ubo.uMatConst[3] = group.combUsesConst && !constantBlack ? group.combConstScaleRGB : 0.0f;
    if (constantOverrides != nullptr && group.materialIndex >= 0) {
        auto found = constantOverrides->find(group.materialIndex * 6 + constantIndex);
        if (found != constantOverrides->end() && found->second.constIdx == constantIndex) {
            std::memcpy(ubo.uMatConst, found->second.rgba, 3 * sizeof(float));
            ubo.uMatConst[3] = group.combConstScaleRGB;
        }
    }

    if (group.dualTexMode) {
        ubo.uSheen[1] = static_cast<float>(group.dualTexMode);
        ubo.uSheen[2] = group.dualTexScale2;
        ubo.uTex1Xf[0] = group.uv1Scale[0];
        ubo.uTex1Xf[1] = group.uv1Scale[1];
        ubo.uTex1Xf[2] = uvOverride ? PreScaleTranslation(uvOverride->u, group.uv1Scale[0], group.uv1Trans[0])
                                    : group.uv1Trans[0] + groupU;
        ubo.uTex1Xf[3] = uvOverride ? PreScaleTranslation(uvOverride->v, group.uv1Scale[1], group.uv1Trans[1])
                                    : group.uv1Trans[1] + groupV;
        ubo.uExtra[1] = 0.0f;
        ubo.uExtra[2] = 0.0f;
    }

    if (group.tevGeneric && group.tevStageCount > 0) {
        ubo.uTevCtl[0] = static_cast<float>(group.tevStageCount);
        ubo.uTevCtl[1] = static_cast<float>(group.coord1Mapping);
        ubo.uTevCtl[2] = static_cast<float>(group.coord2Mapping);
        for (int stage = 0; stage < std::min(group.tevStageCount, 6); ++stage) {
            ubo.uTevStages[stage * 4] = group.tevStagePack[stage][0];
            ubo.uTevStages[stage * 4 + 1] = group.tevStagePack[stage][1];
            ubo.uTevStages[stage * 4 + 2] = group.tevStagePack[stage][2];
        }
        float constants[6][4];
        std::memcpy(constants, group.matConstant, sizeof(constants));
        if (constantOverrides != nullptr && group.materialIndex >= 0) {
            for (int slot = 0; slot < 6; ++slot) {
                auto found = constantOverrides->find(group.materialIndex * 6 + slot);
                if (found != constantOverrides->end() && found->second.constIdx == slot)
                    std::memcpy(constants[slot], found->second.rgba, 4 * sizeof(float));
            }
        }
        for (int slot = 0; slot < 6; ++slot) {
            ubo.uTevConst[slot] = Quantize(constants[slot][0]) | (Quantize(constants[slot][1]) << 8) |
                                  (Quantize(constants[slot][2]) << 16) | (Quantize(constants[slot][3]) << 24);
        }
        ubo.uTex1Xf[0] = group.uv1Scale[0];
        ubo.uTex1Xf[1] = group.uv1Scale[1];
        ubo.uTex1Xf[2] = uvOverride ? PreScaleTranslation(uvOverride->u, group.uv1Scale[0], group.uv1Trans[0])
                                    : group.uv1Trans[0];
        ubo.uTex1Xf[3] = uvOverride ? PreScaleTranslation(uvOverride->v, group.uv1Scale[1], group.uv1Trans[1])
                                    : group.uv1Trans[1];
        ubo.uTex2Xf[0] = group.uv2Scale[0];
        ubo.uTex2Xf[1] = group.uv2Scale[1];
        ubo.uTex2Xf[2] = group.uv2Trans[0];
        ubo.uTex2Xf[3] = group.uv2Trans[1];
    }
}

void CaptureGeometry(int modelId, const GlModel& model, const float* modelView) {
    RendererState& state = State();
    if (!model.hasBounds || state.geometryCurrent.size() >= 4096)
        return;
    GeomRecord record;
    record.modelId = modelId;
    bool first = true;
    for (int corner = 0; corner < 8; ++corner) {
        const float x = corner & 1 ? model.max[0] : model.min[0];
        const float y = corner & 2 ? model.max[1] : model.min[1];
        const float z = corner & 4 ? model.max[2] : model.min[2];
        const float world[3] = {
            modelView[0] * x + modelView[4] * y + modelView[8] * z + modelView[12],
            modelView[1] * x + modelView[5] * y + modelView[9] * z + modelView[13],
            modelView[2] * x + modelView[6] * y + modelView[10] * z + modelView[14],
        };
        for (int axis = 0; axis < 3; ++axis) {
            if (first)
                record.min[axis] = record.max[axis] = world[axis];
            else {
                record.min[axis] = std::min(record.min[axis], world[axis]);
                record.max[axis] = std::max(record.max[axis], world[axis]);
            }
        }
        first = false;
    }
    state.geometryCurrent.push_back(record);
}

void ConfigureVertexInput(const GlModel& model) {
    glBindVertexArray(State().vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, model.vertexBuffer);
    const GLsizei stride = sizeof(Zelda3DGlVtx);
    const size_t offsets[8] = { offsetof(Zelda3DGlVtx, pos),     offsetof(Zelda3DGlVtx, nrm),
                                offsetof(Zelda3DGlVtx, uv),      offsetof(Zelda3DGlVtx, boneIds),
                                offsetof(Zelda3DGlVtx, weights), offsetof(Zelda3DGlVtx, color),
                                offsetof(Zelda3DGlVtx, uv1),     offsetof(Zelda3DGlVtx, uv2) };
    const GLint sizes[8] = { 3, 3, 2, 4, 4, 4, 2, 2 };
    for (GLuint attribute = 0; attribute < 8; ++attribute) {
        glEnableVertexAttribArray(attribute);
        glVertexAttribPointer(attribute, sizes[attribute], GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void*>(offsets[attribute]));
    }
}

} // namespace

void SetModelProvider(Zelda3DModelProvider provider) {
    State().provider = provider;
}

void RequestEvictRange(int firstModelId, int endModelId) {
    RendererState& state = State();
    state.evictionFirst = firstModelId;
    state.evictionEnd = endModelId;
    state.evictionPending = true;
}

void BeginPass() {
    RendererState& state = State();
    state.passActive = false;
    state.drawIndex = 0;
    SeedIsolationControls();
    state.geometryLast.swap(state.geometryCurrent);
    state.geometryCurrent.clear();
    ScopedState restore;
    state.passActive = EnsureResources();
    if (state.passActive)
        ApplyPendingEviction();
}

void DrawModel(int modelId, const float* modelProjection, const float* modelView, int lit, int invertY,
               unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha,
               float aspectAdjustment, const float* boneMatrices, int boneCount, unsigned long long visibleMeshMask,
               int sky, float uvOffsetU, float uvOffsetV, const void* materialTextures,
               const void* materialConstants, const void* materialUvs, int forceUnlit,
               const float* lightDirectionOverride, const float* sphereNormalOverride) {
    RendererState& state = State();
    if (!state.passActive || modelProjection == nullptr || modelView == nullptr)
        return;
    ScopedState restore;
    GlModel* model = EnsureModel(modelId);
    if (model == nullptr || model->vertexBuffer == 0)
        return;
    CaptureGeometry(modelId, *model, modelView);
    const auto* textureOverrides = static_cast<const std::unordered_map<int, int>*>(materialTextures);
    const auto* constantOverrides =
        static_cast<const std::unordered_map<int, Zelda3DMatConstOv>*>(materialConstants);
    const auto* uvOverrides = static_cast<const std::unordered_map<int, Zelda3DMatUvOv>*>(materialUvs);

    SgUbo base{};
    FillBaseUbo(base, modelProjection, modelView, lit, invertY, red, green, blue, alpha, aspectAdjustment,
                boneMatrices, boneCount, sky, uvOffsetU, uvOffsetV, lightDirectionOverride, sphereNormalOverride);
    glUseProgram(state.program);
    ConfigureVertexInput(*model);
    glBindBuffer(GL_UNIFORM_BUFFER, state.bonesUbo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, Zelda3DSg::kBonesBytes, base.uBones);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, state.bonesUbo);
    glDisable(GL_POLYGON_OFFSET_FILL);

    if (gZelda3dFaceCull < 0) {
        const char* setting = std::getenv("ZELDA3D_FACECULL");
        gZelda3dFaceCull = setting != nullptr && setting[0] == '0' ? 0 : 1;
    }
    const bool forceBlend = alpha < 255;
    const bool dump = modelId == g_sgDumpModel;
    if (dump) {
        g_sgDumpModel = -1;
        std::fprintf(stderr, "[SG_DUMP] GL model=%d groups=%zu lit=%d alpha=%u bones=%d\n", modelId,
                     model->groups.size(), lit, alpha, boneCount);
    }

    for (size_t groupIndex = 0; groupIndex < model->groups.size(); ++groupIndex) {
        const GlGroup& record = model->groups[groupIndex];
        const Zelda3DGlGroup& group = record.material;
        if (group.cull || (group.meshId >= 0 && group.meshId < 64 &&
                           ((visibleMeshMask >> group.meshId) & 1ull) == 0))
            continue;
        const int drawIndex = state.drawIndex++;
        if (gZelda3dSgDrawList || dump) {
            std::fprintf(stderr,
                         "[Zelda3D_GL] draw=%d model=%d group=%zu material=%d first=%u count=%u tex=%d/%d/%d "
                         "tev=%d stages=%d\n",
                         drawIndex, modelId, groupIndex, group.materialIndex, record.first, record.count, group.texIndex,
                         group.tex1Index, group.tex2Index, group.tevGeneric, group.tevStageCount);
        }
        if (!Zelda3D_SgDrawIsolationIncludes(modelId, drawIndex))
            continue;

        SgUbo ubo = base;
        FillGroupUbo(ubo, group, lit, forceUnlit, uvOffsetU, uvOffsetV, constantOverrides, uvOverrides);
        if (gZelda3dHlGroup >= 0 && static_cast<int>(groupIndex) == gZelda3dHlGroup) {
            ubo.uTintSkin[0] = 1.0f;
            ubo.uTintSkin[1] = 0.0f;
            ubo.uTintSkin[2] = 0.0f;
        }
        if (const char* selected = std::getenv("ZELDA3D_SG_FRAGDBG_DRAW"))
            ubo.uDebug[0] = std::atoi(selected) == drawIndex ? 1.0f : 0.0f;
        glBindBuffer(GL_UNIFORM_BUFFER, state.commonUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, Zelda3DSg::kCommonBytes, &ubo);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, state.commonUbo);

        group.depthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        const GLenum depthFunction = group.depthFunc >= 0x0200 && group.depthFunc <= 0x0207
                                         ? static_cast<GLenum>(group.depthFunc)
                                         : GL_LESS;
        glDepthFunc(depthFunction);
        glDepthMask(group.depthWrite ? GL_TRUE : GL_FALSE);
        if (group.blendEnable) {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(group.blendSrcRGB, group.blendDstRGB, group.blendSrcA, group.blendDstA);
            glBlendEquationSeparate(group.blendEqRGB, group.blendEqA);
            glBlendColor(group.blendColor[0], group.blendColor[1], group.blendColor[2], group.blendColor[3]);
        } else if (forceBlend) {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        } else {
            glDisable(GL_BLEND);
        }
        if (group.faceCull && gZelda3dFaceCull) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(((invertY != 0) ^ (gZelda3dFaceCullFlip != 0)) ? GL_CW : GL_CCW);
        } else {
            glDisable(GL_CULL_FACE);
        }

        int textureIndex = group.texIndex;
        if (textureOverrides != nullptr && group.materialIndex >= 0) {
            auto found = textureOverrides->find(group.materialIndex);
            if (found != textureOverrides->end() && found->second >= 0)
                textureIndex = found->second;
        }
        glActiveTexture(GL_TEXTURE0);
        BindTexture(TextureAt(*model, textureIndex), group.minFilter, group.magFilter, group.wrapS, group.wrapT);
        glActiveTexture(GL_TEXTURE1);
        BindTexture(TextureAt(*model, group.tex1Index), group.min1Filter, group.mag1Filter, group.wrap1S, group.wrap1T);
        glActiveTexture(GL_TEXTURE2);
        BindTexture(TextureAt(*model, group.tex2Index), group.min2Filter, group.mag2Filter, group.wrap2S, group.wrap2T);
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(record.first), static_cast<GLsizei>(record.count));
    }
}

void EndPass() {
    RendererState& state = State();
    state.passActive = false;
    if (gZelda3dSgDrawOnly >= state.drawIndex && state.drawIndex > 0)
        std::fprintf(stderr, "[Zelda3D_GL] DRAWONLY=%d but frame emitted only %d group(s)\n", gZelda3dSgDrawOnly,
                     state.drawIndex);
    if (gZelda3dSgDrawSkip >= state.drawIndex && state.drawIndex > 0)
        std::fprintf(stderr, "[Zelda3D_GL] DRAWSKIP=%d but frame emitted only %d group(s)\n", gZelda3dSgDrawSkip,
                     state.drawIndex);
    if (gZelda3dSgDrawList && state.drawIndex > 0) {
        std::fprintf(stderr, "[Zelda3D_GL] draw list end: %d group(s)\n", state.drawIndex);
        gZelda3dSgDrawList = 0;
    }
    state.drawIndex = 0;
}

void ClearOverlayDepth() {
    if (!State().passActive)
        return;
    ScopedState restore;
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
}

bool GroupBounds(int modelId, int groupIndex, float* outMin, float* outMax) {
    auto found = State().models.find(modelId);
    if (found == State().models.end() || !found->second.uploaded || groupIndex < 0 ||
        groupIndex >= static_cast<int>(found->second.groups.size()))
        return false;
    const GlGroup& group = found->second.groups[static_cast<size_t>(groupIndex)];
    if (!group.hasBounds)
        return false;
    for (int axis = 0; axis < 3; ++axis) {
        if (outMin != nullptr)
            outMin[axis] = group.min[axis];
        if (outMax != nullptr)
            outMax[axis] = group.max[axis];
    }
    return true;
}

int GeomScanDump(int* modelIds, float* mins, float* maxs, int capacity) {
    const int count = std::min(capacity, static_cast<int>(State().geometryLast.size()));
    for (int index = 0; index < count; ++index) {
        if (modelIds != nullptr)
            modelIds[index] = State().geometryLast[index].modelId;
        for (int axis = 0; axis < 3; ++axis) {
            if (mins != nullptr)
                mins[index * 3 + axis] = State().geometryLast[index].min[axis];
            if (maxs != nullptr)
                maxs[index * 3 + axis] = State().geometryLast[index].max[axis];
        }
    }
    return count;
}

void Shutdown() {
    if (!State().resourcesReady && !State().resourcesFailed)
        return;
    ScopedState restore;
    DestroyResources();
}

} // namespace Fast::Zelda3DOpenGL

#endif
