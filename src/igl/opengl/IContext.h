/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <igl/Common.h>
#include <igl/DeviceFeatures.h>
#include <igl/PlatformDevice.h>
#include <igl/opengl/ComputeCommandAdapter.h>
#include <igl/opengl/DeviceFeatureSet.h>
#include <igl/opengl/GLFunc.h>
#include <igl/opengl/GLIncludes.h>
#include <igl/opengl/RenderCommandAdapter.h>
#include <igl/opengl/UnbindPolicy.h>
#include <igl/opengl/Version.h>
#include <igl/opengl/WithContext.h>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace igl {
class ITexture;
} // namespace igl

namespace igl::opengl {

// We might extend this to other enums presenting API versions on desktops, etc.
// For the time being, we only need to differentiate gles2 and gles3
enum class RenderingAPI { GLES2, GLES3, GL };

///
/// Represents an pure abstract class that encapsulates in it an OpenGL context.
/// Individual types that implement this class are the ones that provide implementation
/// for a concrete OpenGL API implementation.
///
class IContext {
 public:
  virtual void setCurrent() = 0;
  virtual void clearCurrentContext() const = 0;
  virtual bool isCurrentContext() const = 0;
  virtual bool isCurrentSharegroup() const = 0;
  virtual void present(std::shared_ptr<ITexture> surface) const = 0;

  IContext();
  virtual ~IContext();

  void flushDeletionQueue();

 protected:
  bool shouldQueueAPI() const;

 public:
  ///--------------------------------------
  /// MARK: - GL APIs
  void activeTexture(GLenum texture);
  void attachShader(GLuint program, GLuint shader);
  void bindAttribLocation(GLuint program, GLuint index, const GLchar* name);
  void bindBuffer(GLenum target, GLuint buffer);
  void bindBufferBase(GLenum target, GLuint index, GLuint buffer);
  void bindBufferRange(GLenum target,
                       GLuint index,
                       GLuint buffer,
                       GLintptr offset,
                       GLsizeiptr size);
  void bindFramebuffer(GLenum target, GLuint framebuffer);
  void bindRenderbuffer(GLenum target, GLuint renderbuffer);
  void bindTexture(GLenum target, GLuint texture);
  void bindImageTexture(GLuint unit,
                        GLuint texture,
                        GLint level,
                        GLboolean layered,
                        GLint layer,
                        GLenum access,
                        GLenum format);
  void bindVertexArray(GLuint vao);
  void blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
  void blendEquation(GLenum mode);
  void blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
  virtual void blendFunc(GLenum sfactor, GLenum dfactor);
  void blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
  void blitFramebuffer(GLint srcX0,
                       GLint srcY0,
                       GLint srcX1,
                       GLint srcY1,
                       GLint dstX0,
                       GLint dstY0,
                       GLint dstX1,
                       GLint dstY1,
                       GLbitfield mask,
                       GLenum filter);
  void bufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
  void bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);
  virtual GLenum checkFramebufferStatus(GLenum target);
  void clear(GLbitfield mask);
  void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
  void clearDepthf(GLfloat depth);
  void clearStencil(GLint s);
  void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
  void compileShader(GLuint shader);
  void compressedTexImage1D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLint border,
                            GLsizei imageSize,
                            const GLvoid* data);
  void compressedTexImage2D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            GLsizei imageSize,
                            const GLvoid* data);
  void compressedTexImage3D(GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth,
                            GLint border,
                            GLsizei imageSize,
                            const GLvoid* data);
  void compressedTexSubImage1D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLsizei width,
                               GLenum format,
                               GLsizei imageSize,
                               const GLvoid* data);
  void compressedTexSubImage2D(GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLsizei imageSize,
                               const GLvoid* data);
  void compressedTexSubImage3D(GLenum target,
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
  void copyTexImage2D(GLenum target,
                      GLint level,
                      GLenum internalFormat,
                      GLint x,
                      GLint y,
                      GLsizei width,
                      GLsizei height,
                      GLint border);
  void copyTexSubImage2D(GLenum target,
                         GLint level,
                         GLint xoffset,
                         GLint yoffset,
                         GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height);
  void createMemoryObjects(GLsizei n, GLuint* objects);
  GLuint createProgram();
  GLuint createShader(GLenum shaderType);
  virtual void cullFace(GLint mode);
  void debugMessageInsert(GLenum source,
                          GLenum type,
                          GLuint id,
                          GLenum severity,
                          GLsizei length,
                          const GLchar* buf);
  void deleteBuffers(GLsizei n, const GLuint* buffers);
  void deleteFramebuffers(GLsizei n, const GLuint* framebuffers);
  void deleteMemoryObjects(GLsizei n, const GLuint* objects);
  void deleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
  void deleteVertexArrays(GLsizei n, const GLuint* vertexArrays);
  void deleteProgram(GLuint program);
  void deleteShader(GLuint shaderId);
  void deleteSync(GLsync sync);
  void deleteTextures(const std::vector<GLuint>& textures);
  void depthFunc(GLenum func);
  void depthMask(GLboolean flag);
  void depthRangef(GLfloat n, GLfloat f);
  void detachShader(GLuint program, GLuint shader);
  virtual void disable(GLenum cap);
  void disableVertexAttribArray(GLuint index);
  void drawArrays(GLenum mode, GLint first, GLsizei count);
  void drawBuffers(GLsizei n, GLenum* buffers);
  void drawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);
  void drawElementsIndirect(GLenum mode, GLenum type, const GLvoid* indirect);
  virtual void enable(GLenum cap);
  void enableVertexAttribArray(GLuint index);
  GLsync fenceSync(GLenum condition, GLbitfield flags);
  void finish();
  void flush();
  void framebufferRenderbuffer(GLenum target,
                               GLenum attachment,
                               GLenum renderbuffertarget,
                               GLuint renderbuffer);
  void framebufferTexture2D(GLenum target,
                            GLenum attachment,
                            GLenum textarget,
                            GLuint texture,
                            GLint level);
  void framebufferTexture2DMultisample(GLenum target,
                                       GLenum attachment,
                                       GLenum textarget,
                                       GLuint texture,
                                       GLint level,
                                       GLsizei samples);
  void framebufferTextureLayer(GLenum target,
                               GLenum attachment,
                               GLuint texture,
                               GLint level,
                               GLint layer);
  virtual void frontFace(GLenum mode);
  virtual void polygonFillMode(GLenum mode);
  void generateMipmap(GLenum target);
  void genBuffers(GLsizei n, GLuint* buffers);
  void genFramebuffers(GLsizei n, GLuint* framebuffers);
  void genRenderbuffers(GLsizei n, GLuint* renderbuffers);
  void genTextures(GLsizei n, GLuint* textures);
  void genVertexArrays(GLsizei n, GLuint* vertexArrays);
  void getActiveAttrib(GLuint program,
                       GLuint index,
                       GLsizei bufsize,
                       GLsizei* length,
                       GLint* size,
                       GLenum* type,
                       GLchar* name) const;
  void getActiveUniform(GLuint program,
                        GLuint index,
                        GLsizei bufsize,
                        GLsizei* length,
                        GLint* size,
                        GLenum* type,
                        GLchar* name) const;

  void getActiveUniformsiv(GLuint program,
                           GLsizei uniformCount,
                           const GLuint* uniformIndices,
                           GLenum pname,
                           GLint* params) const;
  void getActiveUniformBlockiv(GLuint program, GLuint index, GLenum pname, GLint* params) const;
  void getActiveUniformBlockName(GLuint program,
                                 GLuint index,
                                 GLsizei bufSize,
                                 GLsizei* length,
                                 GLchar* uniformBlockName) const;
  void getAttachedShaders(GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) const;
  GLint getAttribLocation(GLuint program, const GLchar* name) const;
  void getBooleanv(GLenum pname, GLboolean* params) const;
  void getBufferParameteriv(GLenum target, GLenum pname, GLint* params) const;
  virtual GLenum getError() const;
  void getFloatv(GLenum pname, GLfloat* params) const;
  void getFramebufferAttachmentParameteriv(GLenum target,
                                           GLenum attachment,
                                           GLenum pname,
                                           GLint* params) const;
  void getIntegerv(GLenum pname, GLint* params) const;
  void getProgramiv(GLuint program, GLenum pname, GLint* params) const;
  void getProgramInterfaceiv(GLuint program,
                             GLenum programInterface,
                             GLenum pname,
                             GLint* params) const;
  void getProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog) const;
  void getProgramResourceiv(GLuint program,
                            GLenum programInterface,
                            GLuint index,
                            GLsizei propCount,
                            const GLenum* props,
                            GLsizei count,
                            GLsizei* length,
                            GLint* params) const;
  GLuint getProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar* name) const;
  void getProgramResourceName(GLuint program,
                              GLenum programInterface,
                              GLuint index,
                              GLsizei bufSize,
                              GLsizei* length,
                              char* name) const;
  void getRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) const;
  void getShaderiv(GLuint shader, GLenum pname, GLint* params) const;
  void getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog) const;
  void getShaderPrecisionFormat(GLenum shadertype,
                                GLenum precisiontype,
                                GLint* range,
                                GLint* precision) const;
  void getShaderSource(GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source) const;
  virtual const GLubyte* getString(GLenum name) const;
  virtual const GLubyte* getStringi(GLenum name, GLint index) const;
  void getSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values) const;
  void getTexParameterfv(GLenum target, GLenum pname, GLfloat* params) const;
  void getTexParameteriv(GLenum target, GLenum pname, GLint* params) const;
  void getUniformfv(GLuint program, GLint location, GLfloat* params) const;
  void getUniformiv(GLuint program, GLint location, GLint* params) const;
  GLuint getUniformBlockIndex(GLuint program, const GLchar* name) const;
  GLint getUniformLocation(GLuint program, const GLchar* name) const;
  void getVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) const;
  void getVertexAttribiv(GLuint index, GLenum pname, GLint* params) const;
  void hint(GLenum target, GLenum mode);
  void importMemoryFd(GLuint memory, GLuint64 size, GLenum handleType, GLint fd);
  void invalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum* attachments);
  GLboolean isBuffer(GLuint buffer);
  GLboolean isEnabled(GLenum cap);
  GLboolean isFramebuffer(GLuint framebuffer);
  GLboolean isProgram(GLuint program);
  GLboolean isRenderbuffer(GLuint renderbuffer);
  GLboolean isShader(GLuint shader);
  GLboolean isTexture(GLuint texture);
  void lineWidth(GLfloat width);
  void linkProgram(GLuint program);
  void* mapBuffer(GLenum target, GLbitfield access);
  void* mapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
  void pixelStorei(GLenum pname, GLint param);
  void polygonOffset(GLfloat factor, GLfloat units);
  void popDebugGroup();
  void pushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar* message);
  void readPixels(GLint x,
                  GLint y,
                  GLsizei width,
                  GLsizei height,
                  GLenum format,
                  GLenum type,
                  GLvoid* pixels);
  void releaseShaderCompiler();
  void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
  void renderbufferStorageMultisample(GLenum target,
                                      GLsizei samples,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height);
  void framebufferTextureMultiview(GLenum target,
                                   GLenum attachment,
                                   GLuint texture,
                                   GLint level,
                                   GLint baseViewIndex,
                                   GLsizei numViews);
  void framebufferTextureMultisampleMultiview(GLenum target,
                                              GLenum attachment,
                                              GLuint texture,
                                              GLint level,
                                              GLsizei samples,
                                              GLint baseViewIndex,
                                              GLsizei numViews);
  void sampleCoverage(GLfloat value, GLboolean invert);
  void scissor(GLint x, GLint y, GLsizei width, GLsizei height);
  virtual void setEnabled(bool shouldEnable, GLenum cap);
  void shaderBinary(GLsizei n,
                    const GLuint* shaders,
                    GLenum binaryformat,
                    const GLvoid* binary,
                    GLsizei length);
  void shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
  void stencilFunc(GLenum func, GLint ref, GLuint mask);
  void stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
  void stencilMask(GLuint mask);
  void stencilMaskSeparate(GLenum face, GLuint mask);
  void stencilOp(GLenum fail, GLenum zfail, GLenum zpass);
  void stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
  void texStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width);
  void texStorage2D(GLenum target,
                    GLsizei levels,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height);
  void texStorage3D(GLenum target,
                    GLsizei levels,
                    GLenum internalformat,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth);
  void texStorageMem2D(GLenum target,
                       GLsizei levels,
                       GLenum internalFormat,
                       GLsizei width,
                       GLsizei height,
                       GLuint memory,
                       GLuint64 offset);
  void texStorageMem3D(GLenum target,
                       GLsizei levels,
                       GLenum internalFormat,
                       GLsizei width,
                       GLsizei height,
                       GLsizei depth,
                       GLuint memory,
                       GLuint64 offset);
  void texImage1D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const GLvoid* data);
  void texSubImage1D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLsizei width,
                     GLenum format,
                     GLenum type,
                     const GLvoid* pixels);
  void texImage2D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const GLvoid* data);

  void texImage3D(GLenum target,
                  GLint level,
                  GLint internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLsizei depth,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const GLvoid* data);

  void texParameterf(GLenum target, GLenum pname, GLfloat param);
  void texParameterfv(GLenum target, GLenum pname, const GLfloat* params);
  void texParameteri(GLenum target, GLenum pname, GLint param);
  void texParameteriv(GLenum target, GLenum pname, const GLint* params);
  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const GLvoid* pixels);
  void texSubImage3D(GLenum target,
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
  void uniform1f(GLint location, GLfloat x);
  void uniform1fv(GLint location, GLsizei count, const GLfloat* v);
  void uniform1i(GLint location, GLint x);
  void uniform1iv(GLint location, GLsizei count, const GLint* v);
  void uniform2f(GLint location, GLfloat x, GLfloat y);
  void uniform2fv(GLint location, GLsizei count, const GLfloat* v);
  void uniform2i(GLint location, GLint x, GLint y);
  void uniform2iv(GLint location, GLsizei count, const GLint* v);
  void uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z);
  void uniform3fv(GLint location, GLsizei count, const GLfloat* v);
  void uniform3i(GLint location, GLint x, GLint y, GLint z);
  void uniform3iv(GLint location, GLsizei count, const GLint* v);
  void uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
  void uniform4fv(GLint location, GLsizei count, const GLfloat* v);
  void uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w);
  void uniform4iv(GLint location, GLsizei count, const GLint* v);
  void uniformBlockBinding(GLuint pid, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
  void uniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
  void uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
  void uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
  void unmapBuffer(GLenum target);
  void useProgram(GLuint program);
  void validateProgram(GLuint program);
  void vertexAttrib1f(GLuint indx, GLfloat x);
  void vertexAttrib1fv(GLuint indx, const GLfloat* values);
  void vertexAttrib2f(GLuint indx, GLfloat x, GLfloat y);
  void vertexAttrib2fv(GLuint indx, const GLfloat* values);
  void vertexAttrib3f(GLuint indx, GLfloat x, GLfloat y, GLfloat z);
  void vertexAttrib3fv(GLuint indx, const GLfloat* values);
  void vertexAttrib4f(GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
  void vertexAttrib4fv(GLuint indx, const GLfloat* values);
  void vertexAttribPointer(GLuint indx,
                           GLint size,
                           GLenum type,
                           GLboolean normalized,
                           GLsizei stride,
                           const GLvoid* ptr);
  void viewport(GLint x, GLint y, GLsizei width, GLsizei height);

  void dispatchCompute(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
  void memoryBarrier(GLbitfield barriers);
  GLuint64 getTextureHandle(GLuint texture);
  void makeTextureHandleResident(GLuint64 handle);
  void makeTextureHandleNonResident(GLuint64 handle);

  /** Returns current `callCounter_` value. Exposed for testing only. */
  unsigned int getCallCount() const;

  unsigned int getCurrentDrawCount() const;

  // Utility functions
  [[nodiscard]] const DeviceFeatureSet& deviceFeatures() const;
  /// Calls bindBuffer(target, 0) or enqueues to run when deletion queue is flushed
  void unbindBuffer(GLenum target);

  // Log the next N frames
  void apiLogNextNDraws(const unsigned int n);

  // Log everything between start() and end()
  void apiLogStart();
  void apiLogEnd();

  inline bool isDestructionAllowed() const {
    return lockCount_ == 0;
  }

  void resetCounters();

  /** Manual reference counting.
   * In some cases, mostly for performance reasons, we hold unprotected references to the IContext.
   * When doing so, use the functions below to signal such references so we can at least throw an
   * error when those references become invalid.
   */
  // @fb-only
  bool addRef();
  bool releaseRef();

  // @fb-only
  /**
   * The goal here is to try to check whether 'this' is a valid object and not a zombie. Ideally,
   * this should be handled elsewhere, but until we solve that more difficult problem we can at
   * least provide reasonable error messages to users.
   *
   * The idea is to set a specific chunk of memory within the object to a known valid value
   * in the constructor and clear it in the destructor. Invoking this method on a valid object
   * always returns true, and invoking this method on a zombie pointer will simply check that
   * memory offset from the base pointer and most likely return false, unless that memory happens
   * to match our not-so-secret pattern.
   */
  bool isLikelyValidObject() const {
    return zombieGuard_ == kNotAZombie;
  }

 public:
  UnbindPolicy getUnbindPolicy() const;

  /** Sets unbind policy for *subsequent* scopes/render passes.
   *
   * For example, only new instances of RenderCommandEncoder will honor the new unbind policy.
   * Previous instances, on the other hand, use the policy that was in place when they were
   * created. Similarly, the Device's unbind policy will not change until the next beginScope().
   */
  void setUnbindPolicy(UnbindPolicy newValue);

  /** Enables or Disables calling getError() after GL call
   * This check is enabled by defaults in debug mode, and this function can be used to turn it off.
   * In release mode, the option is hard coded to false and this function has no effect.
   */
  void enableAutomaticErrorCheck(bool enable);

  // Manages an adapter pool as recreating this every frame causes unwanted memory allocations.
  // @fb-only
  // @fb-only
  auto& getAdapterPool() {
    return renderAdapterPool_;
  }

  auto& getComputeAdapterPool() {
    return computeAdapterPool_;
  }

  // Called to check if the last OGL call resulted in an error.
  GLenum checkForErrors(const char* callerName, size_t lineNum) const;
  Result getLastError() const;

 protected:
  static std::unordered_map<void*, IContext*>& getExistingContexts();
  static void registerContext(void* glContext, IContext* context);
  static void unregisterContext(void* glContext);
  void initialize(Result* result = nullptr);

 private:
  bool alwaysCheckError_ = false; // TRUE to check error after each OGL call
  mutable GLenum lastError_ = GL_NO_ERROR;
  mutable unsigned int callCounter_ = 0;
  unsigned int drawCallCount_ = 0;
  int lockCount_ = 0; // used by DestructionGuard
  int refCount_ = 0; // used by addRef/releaseRef

  // API Logging
  unsigned int apiLogDrawsLeft_ = 0;
  bool apiLogEnabled_ = false;

  PFNIGLBINDIMAGETEXTUREPROC bindImageTexturerProc_ = nullptr;
  PFNIGLBINDVERTEXARRAYPROC bindVertexArrayProc_ = nullptr;
  PFNIGLBLITFRAMEBUFFERPROC blitFramebufferProc_ = nullptr;
  PFNIGLCLEARDEPTHFPROC clearDepthfProc_ = nullptr;
  PFNIGLCOMPRESSEDTEXIMAGE3DPROC compressedTexImage3DProc_ = nullptr;
  PFNIGLCOMPRESSEDTEXSUBIMAGE3DPROC compressedTexSubImage3DProc_ = nullptr;
  PFNIGLDEBUGMESSAGEINSERTPROC debugMessageInsertProc_ = nullptr;
  PFNIGLDELETESYNCPROC deleteSyncProc_ = nullptr;
  PFNIGLDELETEVERTEXARRAYSPROC deleteVertexArraysProc_ = nullptr;
  PFNIGLDRAWBUFFERSPROC drawBuffersProc_ = nullptr;
  PFNIGLFENCESYNCPROC fenceSyncProc_ = nullptr;
  PFNIGLFRAMEBUFFERTEXTURE2DMULTISAMPLEPROC framebufferTexture2DMultisampleProc_ = nullptr;
  PFNIGLINVALIDATEFRAMEBUFFERPROC invalidateFramebufferProc_ = nullptr;
  PFNIGLGENVERTEXARRAYSPROC genVertexArraysProc_ = nullptr;
  mutable PFNIGLGETSYNCIVPROC getSyncivProc_ = nullptr;
  PFNIGLGETTEXTUREHANDLEPROC getTextureHandleProc_ = nullptr;
  PFNIGLMAKETEXTUREHANDLERESIDENTPROC makeTextureHandleResidentProc_ = nullptr;
  PFNIGLMAKETEXTUREHANDLENONRESIDENTPROC makeTextureHandleNonResidentProc_ = nullptr;
  PFNIGLMAPBUFFERPROC mapBufferProc_ = nullptr;
  PFNIGLMAPBUFFERRANGEPROC mapBufferRangeProc_ = nullptr;
  PFNIGLMEMORYBARRIERPROC memoryBarrierProc_ = nullptr;
  PFNIGLPOPDEBUGGROUPPROC popDebugGroupProc_ = nullptr;
  PFNIGLPUSHDEBUGGROUPPROC pushDebugGroupProc_ = nullptr;
  PFNIGLRENDERBUFFERSTORAGEMULTISAMPLEPROC renderbufferStorageMultisampleProc_ = nullptr;
  PFNIGLTEXIMAGE3DPROC texImage3DProc_ = nullptr;
  PFNIGLTEXSTORAGE1DPROC texStorage1DProc_ = nullptr;
  PFNIGLTEXSTORAGE2DPROC texStorage2DProc_ = nullptr;
  PFNIGLTEXSTORAGE3DPROC texStorage3DProc_ = nullptr;
  PFNIGLTEXSUBIMAGE3DPROC texSubImage3DProc_ = nullptr;
  PFNIGLUNMAPBUFFERPROC unmapBufferProc_ = nullptr;

  /// Responsible for holding onto operations queued for deletion when not in context.
  /// All operations to non-scratch queues are suyncronized by one mutex
  struct SynchronizedDeletionQueues {
   public:
    void flushDeletionQueue(IContext& context);

    void queueDeleteBuffers(GLsizei n, const GLuint* buffers);
    void queueUnbindBuffer(GLenum target);
    void queueDeleteFramebuffers(GLsizei n, const GLuint* framebuffers);
    void queueDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers);
    void queueDeleteVertexArrays(GLsizei n, const GLuint* vertexArrays);
    void queueDeleteProgram(GLuint program);
    void queueDeleteShader(GLuint shaderId);
    void queueDeleteTextures(const std::vector<GLuint>& textures);

   private:
    /// This is called by flushDeletionQueue to swap fooQueue w/ scratchFooQueue
    void swapScratchDeletionQueues();

    // These are swapped with the main queues then read to perform operations. They can be read from
    // flushDeletionQueue w/o synchronization.
    std::vector<GLuint> scratchBuffersQueue_;
    std::unordered_set<GLenum> scratchUnbindBuffersQueue_;
    std::vector<GLuint> scratchFramebuffersQueue_;
    std::vector<GLuint> scratchRenderbuffersQueue_;
    std::vector<GLuint> scratchVertexArraysQueue_;
    std::vector<GLuint> scratchProgramQueue_;
    std::vector<GLuint> scratchShaderQueue_;
    std::vector<GLuint> scratchTexturesQueue_;

    // Guards all the non-scratch queues.
    std::mutex deletionQueueMutex_;

    // Resources queued for deletion
    std::vector<GLuint> buffersQueue_;
    // These are the targets for the buffers that we enqueued for deletion that we need to unbind
    std::unordered_set<GLenum> unbindBuffersQueue_;
    std::vector<GLuint> framebuffersQueue_;
    std::vector<GLuint> renderbuffersQueue_;
    std::vector<GLuint> vertexArraysQueue_;
    std::vector<GLuint> programQueue_;
    std::vector<GLuint> shaderQueue_;
    std::vector<GLuint> texturesQueue_;
  };

  SynchronizedDeletionQueues deletionQueues_;

  UnbindPolicy unbindPolicy_ = UnbindPolicy::Default;

  void getGLMajorAndMinorVersions(GLint& majorVersion, GLint& minorVersion) const;
  void getGLMajorAndMinorVersions_(GLint& majorVersion, GLint& minorVersion) const;
  friend class DestructionGuard;
  std::vector<std::unique_ptr<RenderCommandAdapter>> renderAdapterPool_;
  std::vector<std::unique_ptr<ComputeCommandAdapter>> computeAdapterPool_;

  DeviceFeatureSet deviceFeatureSet_;

  // For framebufferTexture2DMultisample
  GLint maxSamples_ = -1;

  static constexpr uint64_t kNotAZombie = 0xdeadc0def3315badLL;
  uint64_t zombieGuard_ = kNotAZombie;
};

} // namespace igl::opengl
