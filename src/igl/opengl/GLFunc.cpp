/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <igl/opengl/GLFunc.h>

#include <igl/IGL.h>

#if IGL_EGL
#include <EGL/egl.h>
#define IGL_GET_PROC_ADDRESS eglGetProcAddress
#elif IGL_WGL
#include <windows.h>
#include <wingdi.h>
#define IGL_GET_PROC_ADDRESS wglGetProcAddress
#else
#define IGL_GET_PROC_ADDRESS(funcName) nullptr
#endif // IGL_EGL

#define GLEXTENSION_DIRECT_CALL(funcName, funcType, ...) funcName(__VA_ARGS__);
#define GLEXTENSION_DIRECT_CALL_WITH_RETURN(funcName, funcType, returnOnError, ...) \
  return funcName(__VA_ARGS__);

#define GLEXTENSION_LOAD_AND_CALL(funcName, funcType, ...)           \
  static funcType funcAddr = nullptr;                                \
  if (funcAddr == nullptr) {                                         \
    funcAddr = (funcType)IGL_GET_PROC_ADDRESS(#funcName);            \
  }                                                                  \
  if (funcAddr != nullptr) {                                         \
    funcAddr(__VA_ARGS__);                                           \
  } else {                                                           \
    IGL_ASSERT_MSG(0, "Extension function " #funcName " not found"); \
  }
#define GLEXTENSION_LOAD_AND_CALL_WITH_RETURN(funcName, funcType, returnOnError, ...) \
  static funcType funcAddr = nullptr;                                                 \
  if (funcAddr == nullptr) {                                                          \
    funcAddr = (funcType)IGL_GET_PROC_ADDRESS(#funcName);                             \
  }                                                                                   \
  if (funcAddr != nullptr) {                                                          \
    return funcAddr(__VA_ARGS__);                                                     \
  } else {                                                                            \
    IGL_ASSERT_MSG(0, "Extension function " #funcName " not found");                  \
    return returnOnError;                                                             \
  }

#define GLEXTENSION_UNAVAILABLE(funcName, funcType, ...) \
  IGL_ASSERT_MSG(0, "Extension function " #funcName " not found");
#define GLEXTENSION_UNAVAILABLE_WITH_RETURN(funcName, funcType, returnOnError, ...) \
  IGL_ASSERT_MSG(0, "Extension function " #funcName " not found");                  \
  return returnOnError;

#if IGL_EGL || IGL_WGL
#define CAN_LOAD true
#else
#define CAN_LOAD false
#endif // IGL_EGL || IGL_WGL

#if IGL_WGL || IGL_PLATFORM_APPLE
#define CAN_CALL 1
#define CAN_CALL_OPENGL_ES IGL_OPENGL_ES
#define CAN_CALL_OPENGL IGL_OPENGL
#else
#define CAN_CALL 0
#define CAN_CALL_OPENGL_ES 0
#define CAN_CALL_OPENGL 0
#endif // IGL_WGL || IGL_PLATFORM_APPLE

// Special defines for functionality that is always available in OpenGL or OpenGL ES
// Always available means supported from OpenGL 2.0 or OpenGL ES 2.0.
#if CAN_CALL
#define OPENGL_ES_OR_CAN_CALL 1
#define OPENGL_OR_CAN_CALL 1
#else
#define OPENGL_ES_OR_CAN_CALL IGL_OPENGL_ES
#define OPENGL_OR_CAN_CALL IGL_OPENGL
#endif

// Set up redirections with boolean 0 or 1 flags depending on support
// The first 0 or 1 indicates whether the method can be directly called.
// The second 0 or 1 indicates whether the method can be dynamically loaded.
#define GLEXTENSION_1_1(funcName, funcType, ...) \
  GLEXTENSION_DIRECT_CALL(funcName, funcType, __VA_ARGS__)
#define GLEXTENSION_1_0(funcName, funcType, ...) \
  GLEXTENSION_DIRECT_CALL(funcName, funcType, __VA_ARGS__)
#define GLEXTENSION_0_1(funcName, funcType, ...) \
  GLEXTENSION_LOAD_AND_CALL(funcName, funcType, __VA_ARGS__)
#define GLEXTENSION_0_0(funcName, funcType, ...) \
  GLEXTENSION_UNAVAILABLE(funcName, funcType, __VA_ARGS__)

#define GLEXTENSION_WITH_RETURN_1_1(funcName, funcType, returnOnError, ...) \
  GLEXTENSION_DIRECT_CALL_WITH_RETURN(funcName, funcType, returnOnError, __VA_ARGS__)
#define GLEXTENSION_WITH_RETURN_1_0(funcName, funcType, returnOnError, ...) \
  GLEXTENSION_DIRECT_CALL_WITH_RETURN(funcName, funcType, returnOnError, __VA_ARGS__)
#define GLEXTENSION_WITH_RETURN_0_1(funcName, funcType, returnOnError, ...) \
  GLEXTENSION_LOAD_AND_CALL_WITH_RETURN(funcName, funcType, returnOnError, __VA_ARGS__)
#define GLEXTENSION_WITH_RETURN_0_0(funcName, funcType, returnOnError, ...) \
  GLEXTENSION_UNAVAILABLE_WITH_RETURN(funcName, funcType, returnOnError, __VA_ARGS__)

// Expands to GLEXTENSION_A_B where:
//   A is 1 if the function is directly callable (0 otherwise)
//   B is 1 if the function can be dynamically loaded (0 otherwise)
// This in turns expands into GLEXTENSION_DIRECT_CALL, GLEXTENSION_LOAD_AND_CALL or
// GLEXTENSION_UNAVAILABLE, and the arguments are passed along to these.
// This depends on CAN_CALL_funcName being defined as 0 or 1.
#define GLEXTENSION_METHOD_BODY_EXPAND(canCall, canLoad, funcName, funcType, ...) \
  GLEXTENSION_##canCall##_##canLoad(funcName, funcType, __VA_ARGS__)
#define GLEXTENSION_METHOD_BODY_WITH_RETURN_EXPAND(           \
    canCall, canLoad, funcName, funcType, returnOnError, ...) \
  GLEXTENSION_WITH_RETURN_##canCall##_##canLoad(funcName, funcType, returnOnError, __VA_ARGS__)

#if CAN_LOAD
#define GLEXTENSION_METHOD_BODY(canCall, funcName, funcType, ...) \
  GLEXTENSION_METHOD_BODY_EXPAND(canCall, 1, funcName, funcType, __VA_ARGS__)
#define GLEXTENSION_METHOD_BODY_WITH_RETURN(canCall, funcName, funcType, returnOnError, ...) \
  GLEXTENSION_METHOD_BODY_WITH_RETURN_EXPAND(                                                \
      canCall, 1, funcName, funcType, returnOnError, __VA_ARGS__)
#else
#define GLEXTENSION_METHOD_BODY(canCall, funcName, funcType, ...) \
  GLEXTENSION_METHOD_BODY_EXPAND(canCall, 0, funcName, funcType, __VA_ARGS__)
#define GLEXTENSION_METHOD_BODY_WITH_RETURN(canCall, funcName, funcType, returnOnError, ...) \
  GLEXTENSION_METHOD_BODY_WITH_RETURN_EXPAND(                                                \
      canCall, 0, funcName, funcType, returnOnError, __VA_ARGS__)
#endif

IGL_EXTERN_BEGIN

///--------------------------------------
/// MARK: - OpenGL ES / OpenGL

#if IGL_OPENGL
#define CAN_CALL_glClearDepth OPENGL_OR_CAN_CALL
#define CAN_CALL_glMapBuffer CAN_CALL
#else
#define CAN_CALL_glClearDepth 0
#define CAN_CALL_glMapBuffer 0
#endif
#if IGL_OPENGL || defined(GL_ES_VERSION_3_0)
#define CAN_CALL_glDrawBuffers OPENGL_OR_CAN_CALL
#else
#define CAN_CALL_glDrawBuffers 0
#endif
#if IGL_OPENGL || defined(GL_ES_VERSION_3_0)
#define CAN_CALL_glCompressedTexImage3D OPENGL_OR_CAN_CALL
#define CAN_CALL_glCompressedTexSubImage3D OPENGL_OR_CAN_CALL
#define CAN_CALL_glTexImage3D OPENGL_OR_CAN_CALL
#define CAN_CALL_glTexSubImage3D OPENGL_OR_CAN_CALL
#define CAN_CALL_glUnmapBuffer OPENGL_OR_CAN_CALL
#else
#define CAN_CALL_glCompressedTexImage3D 0
#define CAN_CALL_glCompressedTexSubImage3D 0
#define CAN_CALL_glTexImage3D 0
#define CAN_CALL_glTexSubImage3D 0
#define CAN_CALL_glUnmapBuffer 0
#endif
#if defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_3_0)
#define CAN_CALL_glGetStringi CAN_CALL
#else
#define CAN_CALL_glGetStringi 0
#endif
#if defined(GL_VERSION_4_3) || defined(GL_ES_VERSION_3_2)
#define CAN_CALL_glDebugMessageInsert CAN_CALL
#define CAN_CALL_glPopDebugGroup CAN_CALL
#define CAN_CALL_glPushDebugGroup CAN_CALL
#else
#define CAN_CALL_glDebugMessageInsert 0
#define CAN_CALL_glPopDebugGroup 0
#define CAN_CALL_glPushDebugGroup 0
#endif

void iglDebugMessageInsert(GLenum source,
                           GLenum type,
                           GLuint id,
                           GLenum severity,
                           GLsizei length,
                           const GLchar* buf) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDebugMessageInsert,
                          glDebugMessageInsert,
                          PFNIGLDEBUGMESSAGEINSERTPROC,
                          source,
                          type,
                          id,
                          severity,
                          length,
                          buf);
}

void iglClearDepth(GLfloat depth) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glClearDepth, glClearDepth, PFNIGLCLEARDEPTHPROC, depth)
}

void iglCompressedTexImage3D(GLenum target,
                             GLint level,
                             GLenum internalformat,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth,
                             GLint border,
                             GLsizei imageSize,
                             const GLvoid* data) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glCompressedTexImage3D,
                          glCompressedTexImage3D,
                          PFNIGLCOMPRESSEDTEXIMAGE3DPROC,
                          target,
                          level,
                          internalformat,
                          width,
                          height,
                          depth,
                          border,
                          imageSize,
                          data);
}

void iglCompressedTexSubImage3D(GLenum target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint zoffset,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLenum format,
                                GLsizei imageSize,
                                const GLvoid* data) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glCompressedTexSubImage3D,
                          glCompressedTexSubImage3D,
                          PFNIGLCOMPRESSEDTEXSUBIMAGE3DPROC,
                          target,
                          level,
                          xoffset,
                          yoffset,
                          zoffset,
                          width,
                          height,
                          depth,
                          format,
                          imageSize,
                          data);
}

void iglDrawBuffers(GLsizei n, const GLenum* bufs) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDrawBuffers, glDrawBuffers, PFNIGLDRAWBUFFERSPROC, n, bufs);
}

const GLubyte* iglGetStringi(GLenum name, GLint index) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(
      CAN_CALL_glGetStringi, glGetStringi, PFNIGLGETSTRINGIPROC, nullptr, name, index);
}

void* iglMapBuffer(GLenum target, GLbitfield access) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(
      CAN_CALL_glMapBuffer, glMapBuffer, PFNIGLMAPBUFFERPROC, nullptr, target, access);
}

void iglPopDebugGroup() {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glPopDebugGroup, glPopDebugGroup, PFNIGLPOPDEBUGGROUPPROC);
}

void iglPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glPushDebugGroup,
                          glPushDebugGroup,
                          PFNIGLPUSHDEBUGGROUPPROC,
                          source,
                          id,
                          length,
                          message);
}

void iglTexImage3D(GLenum target,
                   GLint level,
                   GLint internalformat,
                   GLsizei width,
                   GLsizei height,
                   GLsizei depth,
                   GLint border,
                   GLenum format,
                   GLenum type,
                   const GLvoid* data) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexImage3D,
                          glTexImage3D,
                          PFNIGLTEXIMAGE3DPROC,
                          target,
                          level,
                          internalformat,
                          width,
                          height,
                          depth,
                          border,
                          format,
                          type,
                          data);
}

void iglTexSubImage3D(GLenum target,
                      GLint level,
                      GLint xoffset,
                      GLint yoffset,
                      GLint zoffset,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      GLenum format,
                      GLenum type,
                      const GLvoid* pixels) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexSubImage3D,
                          glTexSubImage3D,
                          PFNIGLTEXSUBIMAGE3DPROC,
                          target,
                          level,
                          xoffset,
                          yoffset,
                          zoffset,
                          width,
                          height,
                          depth,
                          format,
                          type,
                          pixels);
}

void iglUnmapBuffer(GLenum target) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glUnmapBuffer, glUnmapBuffer, PFNIGLUNMAPBUFFERPROC, target);
}

///--------------------------------------
/// MARK: - GL_APPLE_framebuffer_multisample

#if defined(GL_APPLE_framebuffer_multisample)
#define CAN_CALL_glRenderbufferStorageMultisampleAPPLE CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glRenderbufferStorageMultisampleAPPLE 0
#endif

void iglRenderbufferStorageMultisampleAPPLE(GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glRenderbufferStorageMultisampleAPPLE,
                          glRenderbufferStorageMultisampleAPPLE,
                          PFNIGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,
                          target,
                          samples,
                          internalformat,
                          width,
                          height);
}

///--------------------------------------
/// MARK: - GL_APPLE_sync

#if defined(GL_APPLE_sync)
#define CAN_CALL_glDeleteSyncAPPLE CAN_CALL_OPENGL_ES
#define CAN_CALL_glFenceSyncAPPLE CAN_CALL_OPENGL_ES
#define CAN_CALL_glGetSyncivAPPLE CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glDeleteSyncAPPLE 0
#define CAN_CALL_glFenceSyncAPPLE 0
#define CAN_CALL_glGetSyncivAPPLE 0
#endif

void iglDeleteSyncAPPLE(GLsync sync) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glDeleteSyncAPPLE, glDeleteSyncAPPLE, PFNIGLDELETESYNCPROC, sync);
}

GLsync iglFenceSyncAPPLE(GLenum condition, GLbitfield flags) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(
      CAN_CALL_glFenceSyncAPPLE, glFenceSyncAPPLE, PFNIGLFENCESYNCPROC, GL_ZERO, condition, flags);
}

void iglGetSyncivAPPLE(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetSyncivAPPLE,
                          glGetSyncivAPPLE,
                          PFNIGLGETSYNCIVPROC,
                          sync,
                          pname,
                          bufSize,
                          length,
                          values);
}

///--------------------------------------
/// MARK: - GL_ARB_bindless_texture

#if defined(GL_ARB_bindless_texture)
#define CAN_CALL_glGetTextureHandleARB CAN_CALL_OPENGL
#define CAN_CALL_glMakeTextureHandleResidentARB CAN_CALL_OPENGL
#define CAN_CALL_glMakeTextureHandleNonResidentARB CAN_CALL_OPENGL
#else
#define CAN_CALL_glGetTextureHandleARB 0
#define CAN_CALL_glMakeTextureHandleResidentARB 0
#define CAN_CALL_glMakeTextureHandleNonResidentARB 0
#endif

GLuint64 iglGetTextureHandleARB(GLuint texture) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glGetTextureHandleARB,
                                      glGetTextureHandleARB,
                                      PFNIGLGETTEXTUREHANDLEPROC,
                                      GL_ZERO,
                                      texture);
}

void iglMakeTextureHandleResidentARB(GLuint64 handle) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glMakeTextureHandleResidentARB,
                          glMakeTextureHandleResidentARB,
                          PFNIGLMAKETEXTUREHANDLERESIDENTPROC,
                          handle);
}

void iglMakeTextureHandleNonResidentARB(GLuint64 handle) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glMakeTextureHandleNonResidentARB,
                          glMakeTextureHandleNonResidentARB,
                          PFNIGLMAKETEXTUREHANDLENONRESIDENTPROC,
                          handle);
}

///--------------------------------------
/// MARK: - GL_ARB_compute_shader

#if defined(GL_VERSION_4_3) || defined(GL_ES_VERSION_3_1) || defined(GL_ARB_compute_shader)
#define CAN_CALL_glDispatchCompute CAN_CALL
#else
#define CAN_CALL_glDispatchCompute 0
#endif

void iglDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDispatchCompute,
                          glDispatchCompute,
                          PFNIGLDISPATCHCOMPUTEPROC,
                          num_groups_x,
                          num_groups_y,
                          num_groups_z);
}

///--------------------------------------
/// MARK: - GL_ARB_draw_indirect

#if defined(GL_VERSION_4_0) || defined(GL_ES_VERSION_3_1) || defined(GL_ARB_draw_indirect)
#define CAN_CALL_glDrawElementsIndirect CAN_CALL
#else
#define CAN_CALL_glDrawElementsIndirect 0
#endif

void iglDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid* indirect) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDrawElementsIndirect,
                          glDrawElementsIndirect,
                          PFNIGLDRAWELEMENTSINDIRECTPROC,
                          mode,
                          type,
                          indirect);
}

///--------------------------------------
/// MARK: - GL_ARB_ES2_compatibility

#if IGL_OPENGL_ES || defined(GL_VERSION_4_1) || defined(GL_ARB_ES2_compatibility)
#define CAN_CALL_glClearDepthf OPENGL_ES_OR_CAN_CALL
#else
#define CAN_CALL_glClearDepthf 0
#endif

void iglClearDepthf(GLfloat depth) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glClearDepthf, glClearDepthf, PFNIGLCLEARDEPTHFPROC, depth)
}

///--------------------------------------
/// MARK: - GL_ARB_framebuffer_object

#if defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_3_0) || defined(GL_ARB_framebuffer_object)
#define CAN_CALL_glBlitFramebuffer CAN_CALL
#define CAN_CALL_glFramebufferTextureLayer CAN_CALL
#define CAN_CALL_glRenderbufferStorageMultisample CAN_CALL
#else
#define CAN_CALL_glBlitFramebuffer 0
#define CAN_CALL_glFramebufferTextureLayer 0
#define CAN_CALL_glRenderbufferStorageMultisample 0
#endif

#if defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_2_0) || defined(GL_ARB_framebuffer_object)
#define CAN_CALL_glBindFramebuffer OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glBindRenderbuffer OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glCheckFramebufferStatus OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glDeleteFramebuffers OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glDeleteRenderbuffers OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glFramebufferRenderbuffer OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glFramebufferTexture2D OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glGenerateMipmap OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glGenFramebuffers OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glGenRenderbuffers OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glGetFramebufferAttachmentParameteriv OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glGetRenderbufferParameteriv OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glIsFramebuffer OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glIsRenderbuffer OPENGL_ES_OR_CAN_CALL
#define CAN_CALL_glRenderbufferStorage OPENGL_ES_OR_CAN_CALL
#else
#define CAN_CALL_glBindFramebuffer 0
#define CAN_CALL_glBindRenderbuffer 0
#define CAN_CALL_glBlitFramebuffer 0
#define CAN_CALL_glCheckFramebufferStatus 0
#define CAN_CALL_glDeleteFramebuffers 0
#define CAN_CALL_glDeleteRenderbuffers 0
#define CAN_CALL_glFramebufferRenderbuffer 0
#define CAN_CALL_glFramebufferTexture2D 0
#define CAN_CALL_glGenerateMipmap 0
#define CAN_CALL_glGenFramebuffers 0
#define CAN_CALL_glGenRenderbuffers 0
#define CAN_CALL_glGetFramebufferAttachmentParameteriv 0
#define CAN_CALL_glGetRenderbufferParameteriv 0
#define CAN_CALL_glIsFramebuffer 0
#define CAN_CALL_glIsRenderbuffer 0
#define CAN_CALL_glRenderbufferStorage 0
#endif

void iglBindFramebuffer(GLenum target, GLuint framebuffer) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glBindFramebuffer,
                          glBindFramebuffer,
                          PFNIGLBINDFRAMEBUFFERPROC,
                          target,
                          framebuffer);
}

void iglBindRenderbuffer(GLenum target, GLuint renderbuffer) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glBindRenderbuffer,
                          glBindRenderbuffer,
                          PFNIGLBINDRENDERBUFFERPROC,
                          target,
                          renderbuffer);
}

void iglBlitFramebuffer(GLint srcX0,
                        GLint srcY0,
                        GLint srcX1,
                        GLint srcY1,
                        GLint dstX0,
                        GLint dstY0,
                        GLint dstX1,
                        GLint dstY1,
                        GLbitfield mask,
                        GLenum filter) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glBlitFramebuffer,
                          glBlitFramebuffer,
                          PFNIGLBLITFRAMEBUFFERPROC,
                          srcX0,
                          srcY0,
                          srcX1,
                          srcY1,
                          dstX0,
                          dstY0,
                          dstX1,
                          dstY1,
                          mask,
                          filter);
}

GLenum iglCheckFramebufferStatus(GLenum target) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glCheckFramebufferStatus,
                                      glCheckFramebufferStatus,
                                      PFNIGLCHECKFRAMEBUFFERSTATUSPROC,
                                      GL_FRAMEBUFFER_UNDEFINED,
                                      target);
}

void iglDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDeleteFramebuffers,
                          glDeleteFramebuffers,
                          PFNIGLDELETEFRAMEBUFFERSPROC,
                          n,
                          framebuffers);
}

void iglDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDeleteRenderbuffers,
                          glDeleteRenderbuffers,
                          PFNIGLDELETERENDERBUFFERSPROC,
                          n,
                          renderbuffers);
}

void iglFramebufferRenderbuffer(GLenum target,
                                GLenum attachment,
                                GLenum renderbuffertarget,
                                GLuint renderbuffer) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glFramebufferRenderbuffer,
                          glFramebufferRenderbuffer,
                          PFNIGLFRAMEBUFFERRENDERBUFFERPROC,
                          target,
                          attachment,
                          renderbuffertarget,
                          renderbuffer);
}

void iglFramebufferTexture2D(GLenum target,
                             GLenum attachment,
                             GLenum textarget,
                             GLuint texture,
                             GLint level) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glFramebufferTexture2D,
                          glFramebufferTexture2D,
                          PFNIGLFRAMEBUFFERTEXTURE2DPROC,
                          target,
                          attachment,
                          textarget,
                          texture,
                          level);
}

void iglFramebufferTextureLayer(GLenum target,
                                GLenum attachment,
                                GLuint texture,
                                GLint level,
                                GLint layer) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glFramebufferTextureLayer,
                          glFramebufferTextureLayer,
                          PFNIGLFRAMEBUFFERTEXTURELAYERPROC,
                          target,
                          attachment,
                          texture,
                          level,
                          layer);
}

void iglGenerateMipmap(GLenum target) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glGenerateMipmap, glGenerateMipmap, PFNIGLGENERATEMIPMAPPROC, target);
}

void iglGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glGenFramebuffers, glGenFramebuffers, PFNIGLGENFRAMEBUFFERSPROC, n, framebuffers);
}

void iglGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGenRenderbuffers,
                          glGenRenderbuffers,
                          PFNIGLGENRENDERBUFFERSPROC,
                          n,
                          renderbuffers);
}

void iglGetFramebufferAttachmentParameteriv(GLenum target,
                                            GLenum attachment,
                                            GLenum pname,
                                            GLint* params) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetFramebufferAttachmentParameteriv,
                          glGetFramebufferAttachmentParameteriv,
                          PFNIGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC,
                          target,
                          attachment,
                          pname,
                          params);
}

void iglGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetRenderbufferParameteriv,
                          glGetRenderbufferParameteriv,
                          PFNIGLGETRENDERBUFFERPARAMETERIVPROC,
                          target,
                          pname,
                          params);
}

GLboolean iglIsFramebuffer(GLuint framebuffer) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(
      CAN_CALL_glIsFramebuffer, glIsFramebuffer, PFNIGLISFRAMEBUFFERPROC, GL_FALSE, framebuffer);
}

GLboolean iglIsRenderbuffer(GLuint renderbuffer) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glIsRenderbuffer,
                                      glIsRenderbuffer,
                                      PFNIGLISRENDERBUFFERPROC,
                                      GL_FALSE,
                                      renderbuffer);
}

void iglRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glRenderbufferStorage,
                          glRenderbufferStorage,
                          PFNIGLRENDERBUFFERSTORAGEPROC,
                          target,
                          internalformat,
                          width,
                          height);
}

void iglRenderbufferStorageMultisample(GLenum target,
                                       GLsizei samples,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glRenderbufferStorageMultisample,
                          glRenderbufferStorageMultisample,
                          PFNIGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,
                          target,
                          samples,
                          internalformat,
                          width,
                          height)
}

///--------------------------------------
/// MARK: - GL_ARB_invalidate_subdata

#if defined(GL_VERSION_4_3) || defined(GL_ES_VERSION_3_0) || defined(GL_ARB_invalidate_subdata)
#define CAN_CALL_glInvalidateFramebuffer CAN_CALL
#else
#define CAN_CALL_glInvalidateFramebuffer 0
#endif

void iglInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glInvalidateFramebuffer,
                          glInvalidateFramebuffer,
                          PFNIGLINVALIDATEFRAMEBUFFERPROC,
                          target,
                          numAttachments,
                          attachments);
}

///--------------------------------------
/// MARK: - GL_ARB_map_buffer_range

#if defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_3_0) || defined(GL_ARB_map_buffer_range)
#define CAN_CALL_glMapBufferRange CAN_CALL
#else
#define CAN_CALL_glMapBufferRange 0
#endif

void* iglMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glMapBufferRange,
                                      glMapBufferRange,
                                      PFNIGLMAPBUFFERRANGEPROC,
                                      nullptr,
                                      target,
                                      offset,
                                      length,
                                      access);
}

///--------------------------------------
/// MARK: - GL_ARB_program_interface_query

#if defined(GL_VERSION_4_3) || defined(GL_ES_VERSION_3_1) || defined(GL_ARB_program_interface_query)
#define CAN_CALL_glGetProgramInterfaceiv CAN_CALL
#define CAN_CALL_glGetProgramResourceIndex CAN_CALL
#define CAN_CALL_glGetProgramResourceiv CAN_CALL
#define CAN_CALL_glGetProgramResourceName CAN_CALL
#else
#define CAN_CALL_glGetProgramInterfaceiv 0
#define CAN_CALL_glGetProgramResourceIndex 0
#define CAN_CALL_glGetProgramResourceiv 0
#define CAN_CALL_glGetProgramResourceName 0
#endif

void iglGetProgramInterfaceiv(GLuint program,
                              GLenum programInterface,
                              GLenum pname,
                              GLint* params) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetProgramInterfaceiv,
                          glGetProgramInterfaceiv,
                          PFNIGLGETPROGRAMINTERFACEIVPROC,
                          program,
                          programInterface,
                          pname,
                          params);
}

GLuint iglGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar* name) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glGetProgramResourceIndex,
                                      glGetProgramResourceIndex,
                                      PFNIGLGETPROGRAMRESOURCEINDEXPROC,
                                      GL_INVALID_INDEX,
                                      program,
                                      programInterface,
                                      name);
}

void iglGetProgramResourceiv(GLuint program,
                             GLenum programInterface,
                             GLuint index,
                             GLsizei propCount,
                             const GLenum* props,
                             GLsizei count,
                             GLsizei* length,
                             GLint* params) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetProgramResourceiv,
                          glGetProgramResourceiv,
                          PFNIGLGETPROGRAMRESOURCEIVPROC,
                          program,
                          programInterface,
                          index,
                          propCount,
                          props,
                          count,
                          length,
                          params)
}

void iglGetProgramResourceName(GLuint program,
                               GLenum programInterface,
                               GLuint index,
                               GLsizei bufSize,
                               GLsizei* length,
                               char* name) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetProgramResourceName,
                          glGetProgramResourceName,
                          PFNIGLGETPROGRAMRESOURCENAMEPROC,
                          program,
                          programInterface,
                          index,
                          bufSize,
                          length,
                          name)
}

///--------------------------------------
/// MARK: - GL_ARB_shader_image_load_store

#if defined(GL_VERSION_4_2) || defined(GL_ES_VERSION_3_1) || defined(GL_ARB_shader_image_load_store)
#define CAN_CALL_glBindImageTexture CAN_CALL
#define CAN_CALL_glMemoryBarrier CAN_CALL
#else
#define CAN_CALL_glBindImageTexture 0
#define CAN_CALL_glMemoryBarrier 0
#endif

void iglBindImageTexture(GLuint unit,
                         GLuint texture,
                         GLint level,
                         GLboolean layered,
                         GLint layer,
                         GLenum access,
                         GLenum format) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glBindImageTexture,
                          glBindImageTexture,
                          PFNIGLBINDIMAGETEXTUREPROC,
                          unit,
                          texture,
                          level,
                          layered,
                          layer,
                          access,
                          format);
}

void iglMemoryBarrier(GLbitfield barriers) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glMemoryBarrier, glMemoryBarrier, PFNIGLMEMORYBARRIERPROC, barriers);
}

///--------------------------------------
/// MARK: - GL_ARB_sync

#if defined(GL_VERSION_3_2) || defined(GL_ES_VERSION_3_0) || defined(GL_ARB_sync)
#define CAN_CALL_glDeleteSync CAN_CALL
#define CAN_CALL_glFenceSync CAN_CALL
#define CAN_CALL_glGetSynciv CAN_CALL
#else
#define CAN_CALL_glDeleteSync 0
#define CAN_CALL_glFenceSync 0
#define CAN_CALL_glGetSynciv 0
#endif

void iglDeleteSync(GLsync sync) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDeleteSync, glDeleteSync, PFNIGLDELETESYNCPROC, sync);
}

GLsync iglFenceSync(GLenum condition, GLbitfield flags) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(
      CAN_CALL_glFenceSync, glFenceSync, PFNIGLFENCESYNCPROC, GL_ZERO, condition, flags);
}

void iglGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glGetSynciv, glGetSynciv, PFNIGLGETSYNCIVPROC, sync, pname, bufSize, length, values);
}

///--------------------------------------
/// MARK: - GL_ARB_texture_storage

#if defined(GL_VERSION_4_2) || defined(GL_ES_VERSION_3_0) || defined(GL_ARB_texture_storage)
#define CAN_CALL_glTexStorage1D CAN_CALL_OPENGL
#define CAN_CALL_glTexStorage2D CAN_CALL
#if defined(GL_VERSION_2_0) || defined(GL_ES_VERSION_3_0) || defined(GL_OES_texture_3D)
#define CAN_CALL_glTexStorage3D CAN_CALL
#else
#define CAN_CALL_glTexStorage3D 0
#endif
#else
#define CAN_CALL_glTexStorage1D 0
#define CAN_CALL_glTexStorage2D 0
#define CAN_CALL_glTexStorage3D 0
#endif

void iglTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorage1D,
                          glTexStorage1D,
                          PFNIGLTEXSTORAGE1DPROC,
                          target,
                          levels,
                          internalformat,
                          width);
}

void iglTexStorage2D(GLenum target,
                     GLsizei levels,
                     GLenum internalformat,
                     GLsizei width,
                     GLsizei height) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorage2D,
                          glTexStorage2D,
                          PFNIGLTEXSTORAGE2DPROC,
                          target,
                          levels,
                          internalformat,
                          width,
                          height);
}

void iglTexStorage3D(GLenum target,
                     GLsizei levels,
                     GLenum internalformat,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorage3D,
                          glTexStorage3D,
                          PFNIGLTEXSTORAGE3DPROC,
                          target,
                          levels,
                          internalformat,
                          width,
                          height,
                          depth);
}

///--------------------------------------
/// MARK: - GL_ARB_uniform_buffer_object

#if defined(GL_VERSION_3_1) || defined(GL_ES_VERSION_3_0) || defined(GL_ARB_uniform_buffer_object)
#define CAN_CALL_glBindBufferBase CAN_CALL
#define CAN_CALL_glBindBufferRange CAN_CALL
#define CAN_CALL_glGetActiveUniformsiv CAN_CALL
#define CAN_CALL_glGetActiveUniformBlockiv CAN_CALL
#define CAN_CALL_glGetActiveUniformBlockName CAN_CALL
#define CAN_CALL_glGetUniformBlockIndex CAN_CALL
#define CAN_CALL_glUniformBlockBinding CAN_CALL
#else
#define CAN_CALL_glBindBufferBase 0
#define CAN_CALL_glBindBufferRange 0
#define CAN_CALL_glGetActiveUniformsiv 0
#define CAN_CALL_glGetActiveUniformBlockiv 0
#define CAN_CALL_glGetActiveUniformBlockName 0
#define CAN_CALL_glGetUniformBlockIndex 0
#define CAN_CALL_glUniformBlockBinding 0
#endif

void iglBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glBindBufferBase, glBindBufferBase, PFNIGLBINDBUFFERBASEPROC, target, index, buffer);
}

void iglBindBufferRange(GLenum target,
                        GLuint index,
                        GLuint buffer,
                        GLintptr offset,
                        GLsizeiptr size) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glBindBufferRange,
                          glBindBufferRange,
                          PFNIGLBINDBUFFERRANGEPROC,
                          target,
                          index,
                          buffer,
                          offset,
                          size);
}

void iglGetActiveUniformsiv(GLuint program,
                            GLsizei uniformCount,
                            const GLuint* uniformIndices,
                            GLenum pname,
                            GLint* params) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetActiveUniformsiv,
                          glGetActiveUniformsiv,
                          PFNIGLGETACTIVEUNIFORMSIVPROC,
                          program,
                          uniformCount,
                          uniformIndices,
                          pname,
                          params);
}

void iglGetActiveUniformBlockiv(GLuint program, GLuint index, GLenum pname, GLint* params) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetActiveUniformBlockiv,
                          glGetActiveUniformBlockiv,
                          PFNIGLGETACTIVEUNIFORMBLOCKIVPROC,
                          program,
                          index,
                          pname,
                          params);
}

void iglGetActiveUniformBlockName(GLuint program,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLchar* uniformBlockName) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGetActiveUniformBlockName,
                          glGetActiveUniformBlockName,
                          PFNIGLGETACTIVEUNIFORMBLOCKNAMEPROC,
                          program,
                          index,
                          bufSize,
                          length,
                          uniformBlockName);
}

GLuint iglGetUniformBlockIndex(GLuint program, const GLchar* name) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glGetUniformBlockIndex,
                                      glGetUniformBlockIndex,
                                      PFNIGLGETUNIFORMBLOCKINDEXPROC,
                                      GL_INVALID_INDEX,
                                      program,
                                      name);
}

void iglUniformBlockBinding(GLuint pid, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glUniformBlockBinding,
                          glUniformBlockBinding,
                          PFNIGLUNIFORMBLOCKBINDINGPROC,
                          pid,
                          uniformBlockIndex,
                          uniformBlockBinding);
}

///--------------------------------------
/// MARK: - GL_ARB_vertex_array_object

#if defined(GL_VERSION_3_0) || defined(GL_ES_VERSION_3_0) || defined(GL_ARB_vertex_array_object)
#define CAN_CALL_glBindVertexArray CAN_CALL
#define CAN_CALL_glDeleteVertexArrays CAN_CALL
#define CAN_CALL_glGenVertexArrays CAN_CALL
#else
#define CAN_CALL_glBindVertexArray 0
#define CAN_CALL_glDeleteVertexArrays 0
#define CAN_CALL_glGenVertexArrays 0
#endif

void iglBindVertexArray(GLuint vao) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glBindVertexArray, glBindVertexArray, PFNIGLBINDVERTEXARRAYPROC, vao);
}

void iglDeleteVertexArrays(GLsizei n, const GLuint* vertexArrays) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDeleteVertexArrays,
                          glDeleteVertexArrays,
                          PFNIGLDELETEVERTEXARRAYSPROC,
                          n,
                          vertexArrays);
}

void iglGenVertexArrays(GLsizei n, GLuint* vertexArrays) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glGenVertexArrays, glGenVertexArrays, PFNIGLGENVERTEXARRAYSPROC, n, vertexArrays);
}

///--------------------------------------
/// MARK: - GL_EXT_debug_marker

#if defined(GL_EXT_debug_marker)
#define CAN_CALL_glInsertEventMarkerEXT CAN_CALL
#define CAN_CALL_glPopGroupMarkerEXT CAN_CALL
#define CAN_CALL_glPushGroupMarkerEXT CAN_CALL
#else
#define CAN_CALL_glInsertEventMarkerEXT 0
#define CAN_CALL_glPopGroupMarkerEXT 0
#define CAN_CALL_glPushGroupMarkerEXT 0
#endif

void iglInsertEventMarkerEXT(GLenum /*source*/,
                             GLenum /*type*/,
                             GLuint /*id*/,
                             GLenum /*severity*/,
                             GLsizei length,
                             const GLchar* buf) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glInsertEventMarkerEXT,
                          glInsertEventMarkerEXT,
                          PFNIGLINSERTEVENTMARKERPROC,
                          length,
                          buf);
}

void iglPopGroupMarkerEXT() {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glPopGroupMarkerEXT, glPopGroupMarkerEXT, PFNIGLPOPGROUPMARKERPROC);
}

void iglPushGroupMarkerEXT(GLenum /*source*/,
                           GLuint /*id*/,
                           GLsizei length,
                           const GLchar* message) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glPushGroupMarkerEXT,
                          glPushGroupMarkerEXT,
                          PFNIGLPUSHGROUPMARKERPROC,
                          length,
                          message);
}

///--------------------------------------
/// MARK: - GL_EXT_discard_framebuffer

#if defined(GL_EXT_discard_framebuffer)
#define CAN_CALL_glDiscardFramebufferEXT CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glDiscardFramebufferEXT 0
#endif

void iglDiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum* attachments) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDiscardFramebufferEXT,
                          glDiscardFramebufferEXT,
                          PFNIGLDISCARDFRAMEBUFFERPROC,
                          target,
                          numAttachments,
                          attachments);
}

///--------------------------------------
/// MARK: - GL_EXT_draw_buffers

#if defined(GL_EXT_draw_buffers)
#define CAN_CALL_glDrawBuffersEXT CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glDrawBuffersEXT 0
#endif

void iglDrawBuffersEXT(GLsizei n, const GLenum* bufs) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glDrawBuffersEXT, glDrawBuffersEXT, PFNIGLDRAWBUFFERSPROC, n, bufs);
}

///--------------------------------------
/// MARK: - GL_EXT_framebuffer_blit

#if defined(GL_EXT_framebuffer_blit)
#define CAN_CALL_glBlitFramebufferEXT CAN_CALL_OPENGL
#else
#define CAN_CALL_glBlitFramebufferEXT 0
#endif

void iglBlitFramebufferEXT(GLint srcX0,
                           GLint srcY0,
                           GLint srcX1,
                           GLint srcY1,
                           GLint dstX0,
                           GLint dstY0,
                           GLint dstX1,
                           GLint dstY1,
                           GLbitfield mask,
                           GLenum filter) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glBlitFramebufferEXT,
                          glBlitFramebufferEXT,
                          PFNIGLBLITFRAMEBUFFERPROC,
                          srcX0,
                          srcY0,
                          srcX1,
                          srcY1,
                          dstX0,
                          dstY0,
                          dstX1,
                          dstY1,
                          mask,
                          filter);
}

///--------------------------------------
/// MARK: - GL_EXT_map_buffer_range

#if defined(GL_EXT_map_buffer_range)
#define CAN_CALL_glMapBufferRangeEXT CAN_CALL
#else
#define CAN_CALL_glMapBufferRangeEXT 0
#endif

void* iglMapBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glMapBufferRangeEXT,
                                      glMapBufferRangeEXT,
                                      PFNIGLMAPBUFFERRANGEPROC,
                                      nullptr,
                                      target,
                                      offset,
                                      length,
                                      access);
}

///--------------------------------------
/// MARK: - GL_EXT_memory_object

#if defined(GL_EXT_memory_object)
#define CAN_CALL_glCreateMemoryObjectsEXT CAN_CALL
#define CAN_CALL_glDeleteMemoryObjectsEXT CAN_CALL
#define CAN_CALL_glTexStorageMem2DEXT CAN_CALL
#define CAN_CALL_glTexStorageMem3DEXT CAN_CALL
#else
#define CAN_CALL_glCreateMemoryObjectsEXT 0
#define CAN_CALL_glDeleteMemoryObjectsEXT 0
#define CAN_CALL_glTexStorageMem2DEXT 0
#define CAN_CALL_glTexStorageMem3DEXT 0
#endif

void iglCreateMemoryObjectsEXT(GLsizei n, GLuint* memoryObjects) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glCreateMemoryObjectsEXT,
                          glCreateMemoryObjectsEXT,
                          PFNIGLCREATEMEMORYOBJECTSPROC,
                          n,
                          memoryObjects);
}

void iglDeleteMemoryObjectsEXT(GLsizei n, const GLuint* memoryObjects) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDeleteMemoryObjectsEXT,
                          glDeleteMemoryObjectsEXT,
                          PFNIGLDELETEMEMORYOBJECTSPROC,
                          n,
                          memoryObjects);
}

void iglTexStorageMem2DEXT(GLenum target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height,
                           GLuint memory,
                           GLuint64 offset) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorageMem2DEXT,
                          glTexStorageMem2DEXT,
                          PFNIGLTEXSTORAGEMEM2DPROC,
                          target,
                          levels,
                          internalFormat,
                          width,
                          height,
                          memory,
                          offset);
}

void iglTexStorageMem3DEXT(GLenum target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLuint memory,
                           GLuint64 offset) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorageMem3DEXT,
                          glTexStorageMem3DEXT,
                          PFNIGLTEXSTORAGEMEM3DPROC,
                          target,
                          levels,
                          internalFormat,
                          width,
                          height,
                          depth,
                          memory,
                          offset);
}

///--------------------------------------
/// MARK: - GL_EXT_memory_object_fd

#if defined(GL_EXT_memory_object_fd)
#define CAN_CALL_glImportMemoryFdEXT CAN_CALL
#else
#define CAN_CALL_glImportMemoryFdEXT 0
#endif

void iglImportMemoryFdEXT(GLuint memory, GLuint64 size, GLenum handleType, GLint fd) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glImportMemoryFdEXT,
                          glImportMemoryFdEXT,
                          PFNIGLIMPORTMEMORYFDPROC,
                          memory,
                          size,
                          handleType,
                          fd);
}

///--------------------------------------
/// MARK: - GL_EXT_multisampled_render_to_texture

#if defined(GL_EXT_multisampled_render_to_texture)
#define CAN_CALL_glFramebufferTexture2DMultisampleEXT CAN_CALL_OPENGL_ES
#define CAN_CALL_glRenderbufferStorageMultisampleEXT CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glFramebufferTexture2DMultisampleEXT 0
#define CAN_CALL_glRenderbufferStorageMultisampleEXT 0
#endif

void iglFramebufferTexture2DMultisampleEXT(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei samples) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glFramebufferTexture2DMultisampleEXT,
                          glFramebufferTexture2DMultisampleEXT,
                          PFNIGLFRAMEBUFFERTEXTURE2DMULTISAMPLEPROC,
                          target,
                          attachment,
                          textarget,
                          texture,
                          level,
                          samples)
}

void iglRenderbufferStorageMultisampleEXT(GLenum target,
                                          GLsizei samples,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glRenderbufferStorageMultisampleEXT,
                          glRenderbufferStorageMultisampleEXT,
                          PFNIGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,
                          target,
                          samples,
                          internalformat,
                          width,
                          height)
}

///--------------------------------------
/// MARK: - GL_EXT_shader_image_load_store

#if defined(GL_EXT_shader_image_load_store)
#define CAN_CALL_glBindImageTextureEXT CAN_CALL
#define CAN_CALL_glMemoryBarrierEXT CAN_CALL
#else
#define CAN_CALL_glBindImageTextureEXT 0
#define CAN_CALL_glMemoryBarrierEXT 0
#endif

void iglBindImageTextureEXT(GLuint unit,
                            GLuint texture,
                            GLint level,
                            GLboolean layered,
                            GLint layer,
                            GLenum access,
                            GLenum format) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glBindImageTextureEXT,
                          glBindImageTextureEXT,
                          PFNIGLBINDIMAGETEXTUREPROC,
                          unit,
                          texture,
                          level,
                          layered,
                          layer,
                          access,
                          format);
}

void iglMemoryBarrierEXT(GLbitfield barriers) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glMemoryBarrierEXT, glMemoryBarrierEXT, PFNIGLMEMORYBARRIERPROC, barriers);
}

///--------------------------------------
/// MARK: - GL_EXT_texture_storage

#if defined(GL_EXT_texture_storage)
#define CAN_CALL_glTexStorage1DEXT CAN_CALL_OPENGL
#define CAN_CALL_glTexStorage2DEXT CAN_CALL
#if defined(GL_VERSION_2_0) || defined(GL_OES_texture_3D)
#define CAN_CALL_glTexStorage3DEXT CAN_CALL
#else
#define CAN_CALL_glTexStorage3DEXT 0
#endif
#else
#define CAN_CALL_glTexStorage1DEXT 0
#define CAN_CALL_glTexStorage2DEXT 0
#define CAN_CALL_glTexStorage3DEXT 0
#endif

void iglTexStorage1DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorage1DEXT,
                          glTexStorage1DEXT,
                          PFNIGLTEXSTORAGE1DPROC,
                          target,
                          levels,
                          internalformat,
                          width)
}

void iglTexStorage2DEXT(GLenum target,
                        GLsizei levels,
                        GLenum internalformat,
                        GLsizei width,
                        GLsizei height) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorage2DEXT,
                          glTexStorage2DEXT,
                          PFNIGLTEXSTORAGE2DPROC,
                          target,
                          levels,
                          internalformat,
                          width,
                          height)
}

void iglTexStorage3DEXT(GLenum target,
                        GLsizei levels,
                        GLenum internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexStorage3DEXT,
                          glTexStorage3DEXT,
                          PFNIGLTEXSTORAGE3DPROC,
                          target,
                          levels,
                          internalformat,
                          width,
                          height,
                          depth)
}

///--------------------------------------
/// MARK: - GL_IMG_multisampled_render_to_texture

#if defined(GL_IMG_multisampled_render_to_texture)
#define CAN_CALL_glFramebufferTexture2DMultisampleIMG CAN_CALL_OPENGL_ES
#define CAN_CALL_glRenderbufferStorageMultisampleIMG CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glFramebufferTexture2DMultisampleIMG 0
#define CAN_CALL_glRenderbufferStorageMultisampleIMG 0
#endif

void iglFramebufferTexture2DMultisampleIMG(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei samples) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glFramebufferTexture2DMultisampleIMG,
                          glFramebufferTexture2DMultisampleIMG,
                          PFNIGLFRAMEBUFFERTEXTURE2DMULTISAMPLEPROC,
                          target,
                          attachment,
                          textarget,
                          texture,
                          level,
                          samples)
}

void iglRenderbufferStorageMultisampleIMG(GLenum target,
                                          GLsizei samples,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glRenderbufferStorageMultisampleIMG,
                          glRenderbufferStorageMultisampleIMG,
                          PFNIGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,
                          target,
                          samples,
                          internalformat,
                          width,
                          height)
}

///--------------------------------------
/// MARK: - GL_KHR_debug

#if defined(GL_KHR_debug)
#define CAN_CALL_glDebugMessageInsertKHR CAN_CALL
#define CAN_CALL_glPopDebugGroupKHR CAN_CALL
#define CAN_CALL_glPushDebugGroupKHR CAN_CALL
#else
#define CAN_CALL_glDebugMessageInsertKHR 0
#define CAN_CALL_glPopDebugGroupKHR 0
#define CAN_CALL_glPushDebugGroupKHR 0
#endif

#if IGL_OPENGL
#define DebugMessageInsertKHR glDebugMessageInsert
#define PopDebugGroupKHR glPopDebugGroup
#define PushDebugGroupKHR glPushDebugGroup
#else
#define DebugMessageInsertKHR glDebugMessageInsertKHR
#define PopDebugGroupKHR glPopDebugGroupKHR
#define PushDebugGroupKHR glPushDebugGroupKHR
#endif

void iglDebugMessageInsertKHR(GLenum source,
                              GLenum type,
                              GLuint id,
                              GLenum severity,
                              GLsizei length,
                              const GLchar* buf) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDebugMessageInsertKHR,
                          DebugMessageInsertKHR,
                          PFNIGLDEBUGMESSAGEINSERTPROC,
                          source,
                          type,
                          id,
                          severity,
                          length,
                          buf);
}

void iglPopDebugGroupKHR() {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glPopDebugGroupKHR, PopDebugGroupKHR, PFNIGLPOPDEBUGGROUPPROC);
}
void iglPushDebugGroupKHR(GLenum source, GLuint id, GLsizei length, const GLchar* message) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glPushDebugGroupKHR,
                          PushDebugGroupKHR,
                          PFNIGLPUSHDEBUGGROUPPROC,
                          source,
                          id,
                          length,
                          message);
}

///--------------------------------------
/// MARK: - GL_NV_bindless_texture

#if defined(GL_NV_bindless_texture)
#define CAN_CALL_glGetTextureHandleNV CAN_CALL
#define CAN_CALL_glMakeTextureHandleResidentNV CAN_CALL
#define CAN_CALL_glMakeTextureHandleNonResidentNV CAN_CALL
#else
#define CAN_CALL_glGetTextureHandleNV 0
#define CAN_CALL_glMakeTextureHandleResidentNV 0
#define CAN_CALL_glMakeTextureHandleNonResidentNV 0
#endif

GLuint64 iglGetTextureHandleNV(GLuint texture) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(CAN_CALL_glGetTextureHandleNV,
                                      glGetTextureHandleNV,
                                      PFNIGLGETTEXTUREHANDLEPROC,
                                      GL_ZERO,
                                      texture);
}

void iglMakeTextureHandleResidentNV(GLuint64 handle) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glMakeTextureHandleResidentNV,
                          glMakeTextureHandleResidentNV,
                          PFNIGLMAKETEXTUREHANDLERESIDENTPROC,
                          handle);
}

void iglMakeTextureHandleNonResidentNV(GLuint64 handle) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glMakeTextureHandleNonResidentNV,
                          glMakeTextureHandleNonResidentNV,
                          PFNIGLMAKETEXTUREHANDLENONRESIDENTPROC,
                          handle);
}

///--------------------------------------
/// MARK: - GL_OVR_multiview

#if defined(GL_OVR_multiview)
#define CAN_CALL_glFramebufferTextureMultiviewOVR CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glFramebufferTextureMultiviewOVR 0
#endif

void iglFramebufferTextureMultiviewOVR(GLenum target,
                                       GLenum attachment,
                                       GLuint texture,
                                       GLint level,
                                       GLint baseViewIndex,
                                       GLsizei numViews) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glFramebufferTextureMultiviewOVR,
                          glFramebufferTextureMultiviewOVR,
                          PFNIGLFRAMEBUFFERTEXTUREMULTIVIEWPROC,
                          target,
                          attachment,
                          texture,
                          level,
                          baseViewIndex,
                          numViews)
}

///--------------------------------------
/// MARK: - GL_OVR_multiview_multisampled_render_to_texture

#if defined(GL_OVR_multiview_multisampled_render_to_texture)
#define CAN_CALL_glFramebufferTextureMultisampleMultiviewOVR CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glFramebufferTextureMultisampleMultiviewOVR 0
#endif

void iglFramebufferTextureMultisampleMultiviewOVR(GLenum target,
                                                  GLenum attachment,
                                                  GLuint texture,
                                                  GLint level,
                                                  GLsizei samples,
                                                  GLint baseViewIndex,
                                                  GLsizei numViews) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glFramebufferTextureMultisampleMultiviewOVR,
                          glFramebufferTextureMultisampleMultiviewOVR,
                          PFNIGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWPROC,
                          target,
                          attachment,
                          texture,
                          level,
                          samples,
                          baseViewIndex,
                          numViews)
}

///--------------------------------------
/// MARK: - GL_OES_mapbuffer

#if defined(GL_OES_mapbuffer)
#define CAN_CALL_glMapBufferOES CAN_CALL_OPENGL_ES
#define CAN_CALL_glUnmapBufferOES CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glMapBufferOES 0
#define CAN_CALL_glUnmapBufferOES 0
#endif

void* iglMapBufferOES(GLenum target, GLbitfield access) {
  GLEXTENSION_METHOD_BODY_WITH_RETURN(
      CAN_CALL_glMapBufferOES, glMapBufferOES, PFNIGLMAPBUFFERPROC, nullptr, target, access);
}

void iglUnmapBufferOES(GLenum target) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glUnmapBufferOES, glUnmapBufferOES, PFNIGLUNMAPBUFFERPROC, target);
}

///--------------------------------------
/// MARK: - GL_OES_texture_3D

#if defined(GL_OES_texture_3D)
#define CAN_CALL_glCompressedTexImage3DOES OPENGL_OR_CAN_CALL
#define CAN_CALL_glCompressedTexSubImage3DOES OPENGL_OR_CAN_CALL
#define CAN_CALL_glTexImage3DOES CAN_CALL_OPENGL_ES
#define CAN_CALL_glTexSubImage3DOES CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glCompressedTexImage3DOES 0
#define CAN_CALL_glCompressedTexSubImage3DOES 0
#define CAN_CALL_glTexImage3DOES 0
#define CAN_CALL_glTexSubImage3DOES 0
#endif

void iglCompressedTexImage3DOES(GLenum target,
                                GLint level,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLint border,
                                GLsizei imageSize,
                                const GLvoid* data) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glCompressedTexImage3DOES,
                          glCompressedTexImage3DOES,
                          PFNIGLCOMPRESSEDTEXIMAGE3DPROC,
                          target,
                          level,
                          internalformat,
                          width,
                          height,
                          depth,
                          border,
                          imageSize,
                          data);
}

void iglCompressedTexSubImage3DOES(GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLenum format,
                                   GLsizei imageSize,
                                   const GLvoid* data) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glCompressedTexSubImage3DOES,
                          glCompressedTexSubImage3DOES,
                          PFNIGLCOMPRESSEDTEXSUBIMAGE3DPROC,
                          target,
                          level,
                          xoffset,
                          yoffset,
                          zoffset,
                          width,
                          height,
                          depth,
                          format,
                          imageSize,
                          data);
}

void iglTexImage3DOES(GLenum target,
                      GLint level,
                      GLint internalformat,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      GLint border,
                      GLenum format,
                      GLenum type,
                      const GLvoid* data) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexImage3DOES,
                          glTexImage3DOES,
                          PFNIGLTEXIMAGE3DPROC,
                          target,
                          level,
                          internalformat,
                          width,
                          height,
                          depth,
                          border,
                          format,
                          type,
                          data);
}

void iglTexSubImage3DOES(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint zoffset,
                         GLsizei width,
                         GLsizei height,
                         GLsizei depth,
                         GLenum format,
                         GLenum type,
                         const GLvoid* pixels) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glTexSubImage3DOES,
                          glTexSubImage3DOES,
                          PFNIGLTEXSUBIMAGE3DPROC,
                          target,
                          level,
                          xoffset,
                          yoffset,
                          zoffset,
                          width,
                          height,
                          depth,
                          format,
                          type,
                          pixels);
}

///--------------------------------------
/// MARK: - GL_OES_vertex_array_object

#if defined(GL_OES_vertex_array_object)
#define CAN_CALL_glBindVertexArrayOES CAN_CALL_OPENGL_ES
#define CAN_CALL_glDeleteVertexArraysOES CAN_CALL_OPENGL_ES
#define CAN_CALL_glGenVertexArraysOES CAN_CALL_OPENGL_ES
#else
#define CAN_CALL_glBindVertexArrayOES 0
#define CAN_CALL_glDeleteVertexArraysOES 0
#define CAN_CALL_glGenVertexArraysOES 0
#endif

void iglBindVertexArrayOES(GLuint vao) {
  GLEXTENSION_METHOD_BODY(
      CAN_CALL_glBindVertexArrayOES, glBindVertexArrayOES, PFNIGLBINDVERTEXARRAYPROC, vao);
}

void iglDeleteVertexArraysOES(GLsizei n, const GLuint* vertexArrays) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glDeleteVertexArraysOES,
                          glDeleteVertexArraysOES,
                          PFNIGLDELETEVERTEXARRAYSPROC,
                          n,
                          vertexArrays);
}

void iglGenVertexArraysOES(GLsizei n, GLuint* vertexArrays) {
  GLEXTENSION_METHOD_BODY(CAN_CALL_glGenVertexArraysOES,
                          glGenVertexArraysOES,
                          PFNIGLGENVERTEXARRAYSPROC,
                          n,
                          vertexArrays);
}

IGL_EXTERN_END
