// Complete save/restore boundary for native OoT3D OpenGL work.
#ifdef ENABLE_OPENGL

#include "zelda3d_opengl_internal.h"

namespace Fast::Zelda3DOpenGL {

RendererState& State() {
    static RendererState state;
    return state;
}

ScopedState::ScopedState() {
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer);
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniformBuffer);
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &uniformBindings[0]);
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 1, &uniformBindings[1]);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
    for (int unit = 0; unit < 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[unit]);
    }
    glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendEquationAlpha);
    glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE, &cullMode);
    glGetIntegerv(GL_FRONT_FACE, &frontFace);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
    glGetFloatv(GL_BLEND_COLOR, blendColor);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygonOffsetFactor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygonOffsetUnits);
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &depthClear);
    blend = glIsEnabled(GL_BLEND);
    cull = glIsEnabled(GL_CULL_FACE);
    depth = glIsEnabled(GL_DEPTH_TEST);
    scissor = glIsEnabled(GL_SCISSOR_TEST);
    polygonOffset = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    glActiveTexture(static_cast<GLenum>(activeTexture));
}

ScopedState::~ScopedState() {
    glBindVertexArray(static_cast<GLuint>(vertexArray));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(elementBuffer));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, static_cast<GLuint>(uniformBindings[0]));
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, static_cast<GLuint>(uniformBindings[1]));
    glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(uniformBuffer));
    for (int unit = 0; unit < 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textures[unit]));
    }
    glActiveTexture(static_cast<GLenum>(activeTexture));
    glUseProgram(static_cast<GLuint>(program));
    blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    cull ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    depth ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    scissor ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
    polygonOffset ? glEnable(GL_POLYGON_OFFSET_FILL) : glDisable(GL_POLYGON_OFFSET_FILL);
    glBlendFuncSeparate(static_cast<GLenum>(blendSrcRgb), static_cast<GLenum>(blendDstRgb),
                        static_cast<GLenum>(blendSrcAlpha), static_cast<GLenum>(blendDstAlpha));
    glBlendEquationSeparate(static_cast<GLenum>(blendEquationRgb), static_cast<GLenum>(blendEquationAlpha));
    glBlendColor(blendColor[0], blendColor[1], blendColor[2], blendColor[3]);
    glDepthFunc(static_cast<GLenum>(depthFunc));
    glDepthMask(depthMask);
    glCullFace(static_cast<GLenum>(cullMode));
    glFrontFace(static_cast<GLenum>(frontFace));
    glPolygonOffset(polygonOffsetFactor, polygonOffsetUnits);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    glClearDepth(depthClear);
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
}

} // namespace Fast::Zelda3DOpenGL

#endif
