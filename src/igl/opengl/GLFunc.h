/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/Macros.h>

//
// Note: The below summary explains why we need this.
//
// When using EGL (Android, OpenGL ES on Windows, emscripten, Linux), we must load extensions
// dynamically at runtime. We do not need this for Apple platforms as Apple includes supported
// extensions in its header files.
//
// To simplify use of these extensions, these methods are always available using an igl prefix.
// For EGL platforms, these will be loaded dynamically if available. For WGL, they will be loaded
// dynamically if not provided by GLEW. If provided by GLEW, the method will be called directly. For
// Apple, the method will be called directly if it is available. Whenever the method is not
// available or dynamic load fails, these methods will assert. It is on the caller to ensure that
// methods should be available by doing runtime checks for extensions.

IGL_EXTERN_BEGIN

// Definitions for IGL extension method function pointers.
// These are defined to accommodate platforms where the pointer types are not pre-defined. These
// definitions use a PFNIGL prefix to ensure they don't collide with function pointer types
// defined by other OpenGL loaders. These definitions also omit any extension-specific suffix (e.g.,
// EXT) unless it is needed to disambiguate them.
using PFNIGLBINDBUFFERBASEPROC = void (*)(GLenum target, GLuint index, GLuint buffer);
using PFNIGLBINDBUFFERRANGEPROC =
    void (*)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
using PFNIGLBINDFRAMEBUFFERPROC = void (*)(GLenum target, GLuint framebuffer);
using PFNIGLBINDIMAGETEXTUREPROC = void (*)(GLuint unit,
                                            GLuint texture,
                                            GLint level,
                                            GLboolean layered,
                                            GLint layer,
                                            GLenum access,
                                            GLenum format);
using PFNIGLBINDRENDERBUFFERPROC = void (*)(GLenum target, GLuint renderbuffer);
using PFNIGLBINDVERTEXARRAYPROC = void (*)(GLuint vao);
using PFNIGLBLITFRAMEBUFFERPROC = void (*)(GLint srcX0,
                                           GLint srcY0,
                                           GLint srcX1,
                                           GLint srcY1,
                                           GLint dstX0,
                                           GLint dstY0,
                                           GLint dstX1,
                                           GLint dstY1,
                                           GLbitfield mask,
                                           GLenum filter);
using PFNIGLCHECKFRAMEBUFFERSTATUSPROC = GLenum (*)(GLenum target);
using PFNIGLCLEARDEPTHPROC = void (*)(GLdouble depth);
using PFNIGLCLEARDEPTHFPROC = void (*)(GLfloat depth);
using PFNIGLCOMPRESSEDTEXIMAGE3DPROC = void (*)(GLenum target,
                                                GLint level,
                                                GLenum internalformat,
                                                GLsizei width,
                                                GLsizei height,
                                                GLsizei depth,
                                                GLint border,
                                                GLsizei imageSize,
                                                const GLvoid* data);
using PFNIGLCOMPRESSEDTEXSUBIMAGE3DPROC = void (*)(GLenum target,
                                                   GLint level,
                                                   GLint xoffset,
                                                   GLint yoffset,
                                                   GLint zoffset,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth,
                                                   GLenum format,
                                                   GLsizei imageSize,
                                                   const GLvoid* data);
using PFNIGLCREATEMEMORYOBJECTSPROC = void (*)(GLsizei n, GLuint* memoryObjects);
using PFNIGLDEBUGMESSAGEINSERTPROC = void (*)(GLenum source,
                                              GLenum type,
                                              GLuint id,
                                              GLenum severity,
                                              GLsizei length,
                                              const GLchar* buf);
using PFNIGLDELETEFRAMEBUFFERSPROC = void (*)(GLsizei n, const GLuint* framebuffers);
using PFNIGLDELETEMEMORYOBJECTSPROC = void (*)(GLsizei n, const GLuint* memoryObjects);
using PFNIGLDELETERENDERBUFFERSPROC = void (*)(GLsizei n, const GLuint* renderbuffers);
using PFNIGLDELETESYNCPROC = void (*)(GLsync sync);
using PFNIGLDELETEVERTEXARRAYSPROC = void (*)(GLsizei n, const GLuint* vertexArrays);
using PFNIGLDISCARDFRAMEBUFFERPROC = void (*)(GLenum target,
                                              GLsizei numAttachments,
                                              const GLenum* attachments);
using PFNIGLDISPATCHCOMPUTEPROC = void (*)(GLuint num_groups_x,
                                           GLuint num_groups_y,
                                           GLuint num_groups_z);
using PFNIGLDRAWBUFFERSPROC = void (*)(GLsizei, const GLenum*);
using PFNIGLDRAWELEMENTSINDIRECTPROC = void (*)(GLenum mode, GLenum type, const GLvoid* indirect);
using PFNIGLFENCESYNCPROC = GLsync (*)(GLenum condition, GLbitfield flags);
using PFNIGLFRAMEBUFFERRENDERBUFFERPROC = void (*)(GLenum target,
                                                   GLenum attachment,
                                                   GLenum renderbuffertarget,
                                                   GLuint renderbuffer);
using PFNIGLFRAMEBUFFERTEXTURE2DPROC =
    void (*)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
using PFNIGLFRAMEBUFFERTEXTURELAYERPROC =
    void (*)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
using PFNIGLFRAMEBUFFERTEXTURE2DMULTISAMPLEPROC = void (*)(GLenum target,
                                                           GLenum attachment,
                                                           GLenum textarget,
                                                           GLuint texture,
                                                           GLint level,
                                                           GLsizei samples);
using PFNIGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWPROC = void (*)(GLenum target,
                                                                  GLenum attachment,
                                                                  GLuint texture,
                                                                  GLint level,
                                                                  GLsizei samples,
                                                                  GLint baseViewIndex,
                                                                  GLsizei numViews);
using PFNIGLFRAMEBUFFERTEXTUREMULTIVIEWPROC = void (*)(GLenum target,
                                                       GLenum attachment,
                                                       GLuint texture,
                                                       GLint level,
                                                       GLint baseViewIndex,
                                                       GLsizei numViews);
using PFNIGLGENERATEMIPMAPPROC = void (*)(GLenum target);
using PFNIGLGENFRAMEBUFFERSPROC = void (*)(GLsizei n, GLuint* framebuffers);
using PFNIGLGENRENDERBUFFERSPROC = void (*)(GLsizei n, GLuint* renderbuffers);
using PFNIGLGENVERTEXARRAYSPROC = void (*)(GLsizei n, GLuint* vertexArrays);
using PFNIGLGETACTIVEUNIFORMSIVPROC = void (*)(GLuint program,
                                               GLsizei uniformCount,
                                               const GLuint* uniformIndices,
                                               GLenum pname,
                                               GLint* params);
using PFNIGLGETACTIVEUNIFORMBLOCKIVPROC = void (*)(GLuint program,
                                                   GLuint index,
                                                   GLenum pname,
                                                   GLint* params);
using PFNIGLGETACTIVEUNIFORMBLOCKNAMEPROC = void (*)(GLuint program,
                                                     GLuint index,
                                                     GLsizei bufSize,
                                                     GLsizei* length,
                                                     GLchar* uniformBlockName);
using PFNIGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC = void (*)(GLenum target,
                                                               GLenum attachment,
                                                               GLenum pname,
                                                               GLint* params);
using PFNIGLGETPROGRAMINTERFACEIVPROC = void (*)(GLuint program,
                                                 GLenum programInterface,
                                                 GLenum pname,
                                                 GLint* params);
using PFNIGLGETPROGRAMRESOURCEINDEXPROC = GLuint (*)(GLuint program,
                                                     GLenum programInterface,
                                                     const GLchar* name);
using PFNIGLGETPROGRAMRESOURCEIVPROC = void (*)(GLuint program,
                                                GLenum programInterface,
                                                GLuint index,
                                                GLsizei propCount,
                                                const GLenum* props,
                                                GLsizei count,
                                                GLsizei* length,
                                                GLint* params);
using PFNIGLGETPROGRAMRESOURCENAMEPROC = void (*)(GLuint program,
                                                  GLenum programInterface,
                                                  GLuint index,
                                                  GLsizei bufSize,
                                                  GLsizei* length,
                                                  char* name);
using PFNIGLGETRENDERBUFFERPARAMETERIVPROC = void (*)(GLenum target, GLenum pname, GLint* params);
using PFNIGLGETSTRINGIPROC = const GLubyte* (*)(GLenum name, GLint index);
using PFNIGLGETSYNCIVPROC =
    void (*)(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values);
using PFNIGLGETTEXTUREHANDLEPROC = GLuint64 (*)(GLuint texture);
using PFNIGLGETUNIFORMBLOCKINDEXPROC = GLuint (*)(GLuint program, const GLchar* name);
using PFNIGLIMPORTMEMORYFDPROC = void (*)(GLuint memory,
                                          GLuint64 size,
                                          GLenum handleType,
                                          GLint fd);
using PFNIGLINSERTEVENTMARKERPROC = void (*)(GLsizei length, const GLchar* marker);
using PFNIGLINVALIDATEFRAMEBUFFERPROC = void (*)(GLenum target,
                                                 GLsizei numAttachments,
                                                 const GLenum* attachments);
using PFNIGLISFRAMEBUFFERPROC = GLboolean (*)(GLuint framebuffer);
using PFNIGLISRENDERBUFFERPROC = GLboolean (*)(GLuint renderbuffer);
using PFNIGLMAKETEXTUREHANDLERESIDENTPROC = void (*)(GLuint64 handle);
using PFNIGLMAKETEXTUREHANDLENONRESIDENTPROC = void (*)(GLuint64 handle);
using PFNIGLMAPBUFFERPROC = void* (*)(GLenum target, GLbitfield access);
using PFNIGLMAPBUFFERRANGEPROC = void* (*)(GLenum target,
                                           GLintptr offset,
                                           GLsizeiptr length,
                                           GLbitfield access);
using PFNIGLMEMORYBARRIERPROC = void (*)(GLbitfield barriers);
using PFNIGLPOPDEBUGGROUPPROC = void (*)();
using PFNIGLPOPGROUPMARKERPROC = void (*)();
using PFNIGLPUSHDEBUGGROUPPROC = void (*)(GLenum source,
                                          GLuint id,
                                          GLsizei length,
                                          const GLchar* message);
using PFNIGLPUSHGROUPMARKERPROC = void (*)(GLsizei length, const GLchar* marker);
using PFNIGLRENDERBUFFERSTORAGEPROC = void (*)(GLenum target,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height);
using PFNIGLRENDERBUFFERSTORAGEMULTISAMPLEPROC =
    void (*)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
using PFNIGLTEXIMAGE3DPROC = void (*)(GLenum target,
                                      GLint level,
                                      GLint internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLint border,
                                      GLenum format,
                                      GLenum type,
                                      const GLvoid* pixels);
using PFNIGLTEXSUBIMAGE3DPROC = void (*)(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLint zoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLenum format,
                                         GLenum type,
                                         const GLvoid* data);
using PFNIGLTEXSTORAGE1DPROC = void (*)(GLenum target,
                                        GLsizei levels,
                                        GLenum internalformat,
                                        GLsizei width);
using PFNIGLTEXSTORAGE2DPROC =
    void (*)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
using PFNIGLTEXSTORAGE3DPROC = void (*)(GLenum target,
                                        GLsizei levels,
                                        GLenum internalformat,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth);
using PFNIGLTEXSTORAGEMEM2DPROC = void (*)(GLenum target,
                                           GLsizei levels,
                                           GLenum internalFormat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLuint memory,
                                           GLuint64 offset);
using PFNIGLTEXSTORAGEMEM3DPROC = void (*)(GLenum target,
                                           GLsizei levels,
                                           GLenum internalFormat,
                                           GLsizei width,
                                           GLsizei height,
                                           GLsizei depth,
                                           GLuint memory,
                                           GLuint64 offset);
using PFNIGLUNIFORMBLOCKBINDINGPROC = void (*)(GLuint pid,
                                               GLuint uniformBlockIndex,
                                               GLuint uniformBlockBinding);
using PFNIGLUNMAPBUFFERPROC = void (*)(GLenum target);

///--------------------------------------
/// MARK: - OpenGL ES / OpenGL

// NOTE: Public IGL signature of clearDepth altered to match clearDepthf.
void iglClearDepth(GLfloat depth);
void iglCompressedTexImage3D(GLenum target,
                             GLint level,
                             GLenum internalformat,
                             GLsizei width,
                             GLsizei height,
                             GLsizei depth,
                             GLint border,
                             GLsizei imageSize,
                             const GLvoid* data);
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
                                const GLvoid* data);
void iglDebugMessageInsert(GLenum source,
                           GLenum type,
                           GLuint id,
                           GLenum severity,
                           GLsizei length,
                           const GLchar* buf);
void iglDrawBuffers(GLsizei n, const GLenum* bufs);
const GLubyte* iglGetStringi(GLenum name, GLint index);
void* iglMapBuffer(GLenum target, GLbitfield access);
void iglPopDebugGroup();
void iglPushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message);
void iglTexImage3D(GLenum target,
                   GLint level,
                   GLint internalformat,
                   GLsizei width,
                   GLsizei height,
                   GLsizei depth,
                   GLint border,
                   GLenum format,
                   GLenum type,
                   const GLvoid* data);
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
                      const GLvoid* pixels);
void iglUnmapBuffer(GLenum target);

///--------------------------------------
/// MARK: - GL_APPLE_framebuffer_multisample

void iglRenderbufferStorageMultisampleAPPLE(GLenum target,
                                            GLsizei samples,
                                            GLenum internalformat,
                                            GLsizei width,
                                            GLsizei height);
///--------------------------------------
/// MARK: - GL_APPLE_sync

void iglDeleteSyncAPPLE(GLsync sync);
GLsync iglFenceSyncAPPLE(GLenum condition, GLbitfield flags);
void iglGetSyncivAPPLE(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values);

///--------------------------------------
/// MARK: - GL_ARB_bindless_texture

GLuint64 iglGetTextureHandleARB(GLuint texture);
void iglMakeTextureHandleResidentARB(GLuint64 handle);
void iglMakeTextureHandleNonResidentARB(GLuint64 handle);

///--------------------------------------
/// MARK: - GL_ARB_compute_shader

void iglDispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);

///--------------------------------------
/// MARK: - GL_ARB_draw_indirect

void iglDrawElementsIndirect(GLenum mode, GLenum type, const GLvoid* indirect);

///--------------------------------------
/// MARK: - GL_ARB_ES2_compatibility

void iglClearDepthf(GLfloat depth);

///--------------------------------------
/// MARK: - GL_ARB_framebuffer_object

void iglBindFramebuffer(GLenum target, GLuint framebuffer);
void iglBindRenderbuffer(GLenum target, GLuint renderbuffer);
void iglBlitFramebuffer(GLint srcX0,
                        GLint srcY0,
                        GLint srcX1,
                        GLint srcY1,
                        GLint dstX0,
                        GLint dstY0,
                        GLint dstX1,
                        GLint dstY1,
                        GLbitfield mask,
                        GLenum filter);
GLenum iglCheckFramebufferStatus(GLenum target);
void iglDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
void iglDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
void iglFramebufferRenderbuffer(GLenum target,
                                GLenum attachment,
                                GLenum renderbuffertarget,
                                GLuint renderbuffer);
void iglFramebufferTexture2D(GLenum target,
                             GLenum attachment,
                             GLenum textarget,
                             GLuint texture,
                             GLint level);
void iglFramebufferTextureLayer(GLenum target,
                                GLenum attachment,
                                GLuint texture,
                                GLint level,
                                GLint layer);
void iglGenerateMipmap(GLenum target);
void iglGenFramebuffers(GLsizei n, GLuint* framebuffers);
void iglGenRenderbuffers(GLsizei n, GLuint* renderbuffers);
void iglGetFramebufferAttachmentParameteriv(GLenum target,
                                            GLenum attachment,
                                            GLenum pname,
                                            GLint* params);
void iglGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params);
GLboolean iglIsFramebuffer(GLuint framebuffer);
GLboolean iglIsRenderbuffer(GLuint renderbuffer);
void iglRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void iglRenderbufferStorageMultisample(GLenum target,
                                       GLsizei samples,
                                       GLenum internalformat,
                                       GLsizei width,
                                       GLsizei height);

///--------------------------------------
/// MARK: - GL_ARB_invalidate_subdata

void iglInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments);

///--------------------------------------
/// MARK: - GL_ARB_map_buffer_range

void* iglMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);

///--------------------------------------
/// MARK: - GL_ARB_program_interface_query

void iglGetProgramInterfaceiv(GLuint program, GLenum programInterface, GLenum pname, GLint* params);
GLuint iglGetProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar* name);
void iglGetProgramResourceiv(GLuint program,
                             GLenum programInterface,
                             GLuint index,
                             GLsizei propCount,
                             const GLenum* props,
                             GLsizei count,
                             GLsizei* length,
                             GLint* params);
void iglGetProgramResourceName(GLuint program,
                               GLenum programInterface,
                               GLuint index,
                               GLsizei bufSize,
                               GLsizei* length,
                               char* name);

///--------------------------------------
/// MARK: - GL_ARB_shader_image_load_store

void iglBindImageTexture(GLuint unit,
                         GLuint texture,
                         GLint level,
                         GLboolean layered,
                         GLint layer,
                         GLenum access,
                         GLenum format);
void iglMemoryBarrier(GLbitfield barriers);

///--------------------------------------
/// MARK: - GL_ARB_sync

void iglDeleteSync(GLsync sync);
GLsync iglFenceSync(GLenum condition, GLbitfield flags);
void iglGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values);

///--------------------------------------
/// MARK: - GL_ARB_texture_storage

void iglTexStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
void iglTexStorage2D(GLenum target,
                     GLsizei levels,
                     GLenum internalformat,
                     GLsizei width,
                     GLsizei height);
void iglTexStorage3D(GLenum target,
                     GLsizei levels,
                     GLenum internalformat,
                     GLsizei width,
                     GLsizei height,
                     GLsizei depth);

///--------------------------------------
/// MARK: - GL_ARB_uniform_buffer_object

void iglBindBufferBase(GLenum target, GLuint index, GLuint buffer);
void iglBindBufferRange(GLenum target,
                        GLuint index,
                        GLuint buffer,
                        GLintptr offset,
                        GLsizeiptr size);
void iglGetActiveUniformsiv(GLuint program,
                            GLsizei uniformCount,
                            const GLuint* uniformIndices,
                            GLenum pname,
                            GLint* params);
void iglGetActiveUniformBlockiv(GLuint program, GLuint index, GLenum pname, GLint* params);
void iglGetActiveUniformBlockName(GLuint program,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei* length,
                                  GLchar* uniformBlockName);
GLuint iglGetUniformBlockIndex(GLuint program, const GLchar* name);
void iglUniformBlockBinding(GLuint pid, GLuint uniformBlockIndex, GLuint uniformBlockBinding);

///--------------------------------------
/// MARK: - GL_ARB_vertex_array_object

void iglBindVertexArray(GLuint vao);
void iglDeleteVertexArrays(GLsizei n, const GLuint* vertexArrays);
void iglGenVertexArrays(GLsizei n, GLuint* vertexArrays);

///--------------------------------------
/// MARK: - GL_EXT_debug_marker

// NOTE: Public IGL signature altered to match GL_KHR_debug.
// Additional params from GL_KHR_debug not used by GL_EXT_debug_marker are ignored.

void iglInsertEventMarkerEXT(GLenum source,
                             GLenum type,
                             GLuint id,
                             GLenum severity,
                             GLsizei length,
                             const GLchar* buf);
void iglPopGroupMarkerEXT();
void iglPushGroupMarkerEXT(GLenum source, GLuint id, GLsizei length, const GLchar* message);

///--------------------------------------
/// MARK: - GL_EXT_discard_framebuffer

void iglDiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum* attachments);

///--------------------------------------
/// MARK: - GL_EXT_draw_buffers

void iglDrawBuffersEXT(GLsizei n, const GLenum* bufs);

///--------------------------------------
/// MARK: - GL_EXT_framebuffer_blit

void iglBlitFramebufferEXT(GLint srcX0,
                           GLint srcY0,
                           GLint srcX1,
                           GLint srcY1,
                           GLint dstX0,
                           GLint dstY0,
                           GLint dstX1,
                           GLint dstY1,
                           GLbitfield mask,
                           GLenum filter);

///--------------------------------------
/// MARK: - GL_EXT_map_buffer_range

void* iglMapBufferRangeEXT(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);

///--------------------------------------
/// MARK: - GL_EXT_memory_object

void iglCreateMemoryObjectsEXT(GLsizei n, GLuint* memoryObjects);
void iglDeleteMemoryObjectsEXT(GLsizei n, const GLuint* memoryObjects);
void iglTexStorageMem2DEXT(GLenum target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height,
                           GLuint memory,
                           GLuint64 offset);
void iglTexStorageMem3DEXT(GLenum target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth,
                           GLuint memory,
                           GLuint64 offset);

///--------------------------------------
/// MARK: - GL_EXT_memory_object_fd

void iglImportMemoryFdEXT(GLuint memory, GLuint64 size, GLenum handleType, GLint fd);

///--------------------------------------
/// MARK: - GL_EXT_multisampled_render_to_texture

void iglFramebufferTexture2DMultisampleEXT(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei samples);
void iglRenderbufferStorageMultisampleEXT(GLenum target,
                                          GLsizei samples,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height);

///--------------------------------------
/// MARK: - GL_EXT_shader_image_load_store

void iglBindImageTextureEXT(GLuint unit,
                            GLuint texture,
                            GLint level,
                            GLboolean layered,
                            GLint layer,
                            GLenum access,
                            GLenum format);
void iglMemoryBarrierEXT(GLbitfield barriers);

///--------------------------------------
/// MARK: - GL_EXT_texture_storage

void iglTexStorage1DEXT(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
void iglTexStorage2DEXT(GLenum target,
                        GLsizei levels,
                        GLenum internalformat,
                        GLsizei width,
                        GLsizei height);
void iglTexStorage3DEXT(GLenum target,
                        GLsizei levels,
                        GLenum internalformat,
                        GLsizei width,
                        GLsizei height,
                        GLsizei depth);

///--------------------------------------
/// MARK: - GL_IMG_multisampled_render_to_texture

void iglFramebufferTexture2DMultisampleIMG(GLenum target,
                                           GLenum attachment,
                                           GLenum textarget,
                                           GLuint texture,
                                           GLint level,
                                           GLsizei samples);
void iglRenderbufferStorageMultisampleIMG(GLenum target,
                                          GLsizei samples,
                                          GLenum internalformat,
                                          GLsizei width,
                                          GLsizei height);

///--------------------------------------
/// MARK: - GL_KHR_debug

void iglDebugMessageInsertKHR(GLenum source,
                              GLenum type,
                              GLuint id,
                              GLenum severity,
                              GLsizei length,
                              const GLchar* buf);
void iglPopDebugGroupKHR();
void iglPushDebugGroupKHR(GLenum source, GLuint id, GLsizei length, const GLchar* message);

///--------------------------------------
/// MARK: - GL_NV_bindless_texture

GLuint64 iglGetTextureHandleNV(GLuint texture);
void iglMakeTextureHandleResidentNV(GLuint64 handle);
void iglMakeTextureHandleNonResidentNV(GLuint64 handle);

///--------------------------------------
/// MARK: - GL_OVR_multiview

void iglFramebufferTextureMultiviewOVR(GLenum target,
                                       GLenum attachment,
                                       GLuint texture,
                                       GLint level,
                                       GLint baseViewIndex,
                                       GLsizei numViews);

///--------------------------------------
/// MARK: - GL_OVR_multiview_multisampled_render_to_texture

void iglFramebufferTextureMultisampleMultiviewOVR(GLenum target,
                                                  GLenum attachment,
                                                  GLuint texture,
                                                  GLint level,
                                                  GLsizei samples,
                                                  GLint baseViewIndex,
                                                  GLsizei numViews);
///--------------------------------------
/// MARK: - GL_OES_mapbuffer

void* iglMapBufferOES(GLenum target, GLbitfield access);
void iglUnmapBufferOES(GLenum target);

///--------------------------------------
/// MARK: - GL_OES_texture_3D

void iglCompressedTexImage3DOES(GLenum target,
                                GLint level,
                                GLenum internalformat,
                                GLsizei width,
                                GLsizei height,
                                GLsizei depth,
                                GLint border,
                                GLsizei imageSize,
                                const GLvoid* data);
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
                                   const GLvoid* data);
void iglTexImage3DOES(GLenum target,
                      GLint level,
                      GLint internalformat,
                      GLsizei width,
                      GLsizei height,
                      GLsizei depth,
                      GLint border,
                      GLenum format,
                      GLenum type,
                      const GLvoid* data);
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
                         const GLvoid* pixels);

///--------------------------------------
/// MARK: - GL_OES_vertex_array_object

void iglBindVertexArrayOES(GLuint vao);
void iglDeleteVertexArraysOES(GLsizei n, const GLuint* vertexArrays);
void iglGenVertexArraysOES(GLsizei n, GLuint* vertexArrays);

IGL_EXTERN_END
