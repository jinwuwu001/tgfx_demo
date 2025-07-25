/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making tgfx available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      https://opensource.org/licenses/BSD-3-Clause
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "platform/android/SurfaceTexture.h"
#include <chrono>
#include "HandlerThread.h"
#include "JNIUtil.h"
#include "core/utils/Log.h"
#include "gpu/DefaultTexture.h"
#include "gpu/opengl/GLTextureSampler.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

namespace tgfx {
static Global<jclass> SurfaceTextureClass;
static jmethodID SurfaceTexture_Constructor_singleBufferMode;
static jmethodID SurfaceTexture_Constructor_texName;
static jmethodID SurfaceTexture_setOnFrameAvailableListenerHandler;
static jmethodID SurfaceTexture_setOnFrameAvailableListener;
static jfieldID SurfaceTexture_mEventHandler;
static jmethodID SurfaceTexture_updateTexImage;
static jmethodID SurfaceTexture_attachToGLContext;
static jmethodID SurfaceTexture_detachFromGLContext;
static jmethodID SurfaceTexture_getTransformMatrix;
static jmethodID SurfaceTexture_release;
static Global<jclass> SurfaceClass;
static jmethodID Surface_Constructor;
static jmethodID Surface_release;
static Global<jclass> HandlerClass;
static jmethodID Handler_Constructor;
static Global<jclass> EventHandlerClass;
static jmethodID EventHandler_Constructor;

void SurfaceTexture::JNIInit(JNIEnv* env) {
  SurfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
  SurfaceTexture_Constructor_singleBufferMode =
      env->GetMethodID(SurfaceTextureClass.get(), "<init>", "(Z)V");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SurfaceTexture_Constructor_texName =
        env->GetMethodID(SurfaceTextureClass.get(), "<init>", "(I)V");
  }
  SurfaceTexture_setOnFrameAvailableListenerHandler = env->GetMethodID(
      SurfaceTextureClass.get(), "setOnFrameAvailableListener",
      "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;Landroid/os/Handler;)V");
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    SurfaceTexture_setOnFrameAvailableListener =
        env->GetMethodID(SurfaceTextureClass.get(), "setOnFrameAvailableListener",
                         "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V");
    SurfaceTexture_mEventHandler =
        env->GetFieldID(SurfaceTextureClass.get(), "mEventHandler",
                        "Landroid/graphics/SurfaceTexture$EventHandler;");
    EventHandlerClass = env->FindClass("android/graphics/SurfaceTexture$EventHandler");
    EventHandler_Constructor =
        env->GetMethodID(EventHandlerClass.get(), "<init>",
                         "(Landroid/graphics/SurfaceTexture;Landroid/os/Looper;)V");
  } else {
    HandlerClass = env->FindClass("android/os/Handler");
    Handler_Constructor = env->GetMethodID(HandlerClass.get(), "<init>", "(Landroid/os/Looper;)V");
  }
  SurfaceTexture_updateTexImage =
      env->GetMethodID(SurfaceTextureClass.get(), "updateTexImage", "()V");
  SurfaceTexture_attachToGLContext =
      env->GetMethodID(SurfaceTextureClass.get(), "attachToGLContext", "(I)V");
  SurfaceTexture_detachFromGLContext =
      env->GetMethodID(SurfaceTextureClass.get(), "detachFromGLContext", "()V");
  SurfaceTexture_getTransformMatrix =
      env->GetMethodID(SurfaceTextureClass.get(), "getTransformMatrix", "([F)V");
  SurfaceTexture_release = env->GetMethodID(SurfaceTextureClass.get(), "release", "()V");
  SurfaceClass = env->FindClass("android/view/Surface");
  Surface_Constructor =
      env->GetMethodID(SurfaceClass.get(), "<init>", "(Landroid/graphics/SurfaceTexture;)V");
  Surface_release = env->GetMethodID(SurfaceClass.get(), "release", "()V");
}

static std::mutex threadLocker = {};
static std::shared_ptr<HandlerThread> handlerThread;

static jobject GetGlobalLooper() {
  std::lock_guard<std::mutex> autoLock(threadLocker);
  if (handlerThread == nullptr) {
    handlerThread = HandlerThread::Make();
  }
  if (handlerThread == nullptr) {
    return nullptr;
  }
  return handlerThread->getLooper();
}

static void SetOnFrameAvailableListener(JNIEnv* env, jobject surfaceTexture, jobject listener,
                                        jobject looper) {
  if (SurfaceTexture_setOnFrameAvailableListenerHandler != nullptr) {
    auto handler = env->NewObject(HandlerClass.get(), Handler_Constructor, looper);
    env->CallVoidMethod(surfaceTexture, SurfaceTexture_setOnFrameAvailableListenerHandler, listener,
                        handler);
  } else {
    env->CallVoidMethod(surfaceTexture, SurfaceTexture_setOnFrameAvailableListener, listener);
    auto handler =
        env->NewObject(EventHandlerClass.get(), EventHandler_Constructor, surfaceTexture, looper);
    env->SetObjectField(surfaceTexture, SurfaceTexture_mEventHandler, handler);
  }
}

std::shared_ptr<SurfaceTexture> SurfaceTexture::Make(int width, int height, jobject listener) {
  if (width <= 0 || height <= 0 || listener == nullptr) {
    return nullptr;
  }
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  jobject stObject;
  if (SurfaceTexture_Constructor_singleBufferMode != nullptr) {
    stObject = env->NewObject(SurfaceTextureClass.get(),
                              SurfaceTexture_Constructor_singleBufferMode, JNI_FALSE);
  } else {
    stObject = env->NewObject(SurfaceTextureClass.get(), SurfaceTexture_Constructor_texName, 0);
    if (stObject != nullptr) {
      env->CallVoidMethod(stObject, SurfaceTexture_detachFromGLContext);
    }
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOGE("SurfaceImageReader::MakeFrom(): failed to create a new SurfaceTexture!");
    return nullptr;
  }
  auto looper = GetGlobalLooper();
  if (looper == nullptr) {
    return nullptr;
  }
  SetOnFrameAvailableListener(env, stObject, listener, looper);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOGE("SurfaceImageReader::MakeFrom(): failed to set the OnFrameAvailableListener!");
    return nullptr;
  }
  return std::shared_ptr<SurfaceTexture>(new SurfaceTexture(width, height, env, stObject));
}

SurfaceTexture::SurfaceTexture(int width, int height, JNIEnv* env, jobject st)
    : ImageStream(width, height) {
  surfaceTexture = st;
  surface = env->NewObject(SurfaceClass.get(), Surface_Constructor, st);
}

SurfaceTexture::~SurfaceTexture() {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return;
  }
  env->CallVoidMethod(surface.get(), Surface_release);
  env->CallVoidMethod(surfaceTexture.get(), SurfaceTexture_release);
}

jobject SurfaceTexture::getInputSurface() const {
  return surface.get();
}

void SurfaceTexture::notifyFrameAvailable() {
  // Note: If there is a pending frame available already, SurfaceTexture will not dispatch any new
  // frame-available event until you have called the SurfaceTexture.updateTexImage().
  std::lock_guard<std::mutex> autoLock(locker);
  frameAvailable = true;
  condition.notify_all();
}

static ISize ComputeTextureSize(float matrix[16], int width, int height) {
  Size size = {static_cast<float>(width), static_cast<float>(height)};
  auto scaleX = fabsf(matrix[0]);
  if (scaleX > 0) {
    size.width = size.width / (scaleX + matrix[12] * 2);
  }
  float scaleY = fabsf(matrix[5]);
  if (scaleY > 0) {
    size.height = size.height / (scaleY + (matrix[13] - scaleY) * 2);
  }
  return size.toRound();
}

std::shared_ptr<Texture> SurfaceTexture::onMakeTexture(Context* context, bool) {
  auto sampler = makeTextureSampler(context);
  if (sampler == nullptr) {
    return nullptr;
  }
  auto textureSize = updateTexImage();
  if (textureSize.isEmpty()) {
    sampler->releaseGPU(context);
    return nullptr;
  }
  return Resource::AddToCache(
      context, new DefaultTexture(std::move(sampler), textureSize.width, textureSize.height));
}

bool SurfaceTexture::onUpdateTexture(std::shared_ptr<Texture>) {
  auto size = updateTexImage();
  return !size.isEmpty();
}

std::unique_ptr<TextureSampler> SurfaceTexture::makeTextureSampler(Context* context) {
  std::lock_guard<std::mutex> autoLock(locker);
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  auto gl = GLFunctions::Get(context);
  unsigned samplerID = 0;
  gl->genTextures(1, &samplerID);
  if (samplerID == 0) {
    return nullptr;
  }
  env->CallVoidMethod(surfaceTexture.get(), SurfaceTexture_attachToGLContext, samplerID);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    gl->deleteTextures(1, &samplerID);
    LOGE("NativeImageReader::makeTexture(): failed to attached to a SurfaceTexture!");
    return nullptr;
  }
  return std::make_unique<GLTextureSampler>(samplerID, GL_TEXTURE_EXTERNAL_OES,
                                            PixelFormat::RGBA_8888);
}

ISize SurfaceTexture::updateTexImage() {
  std::unique_lock<std::mutex> autoLock(locker);
  if (!frameAvailable) {
    static const auto TIMEOUT = std::chrono::seconds(1);
    auto status = condition.wait_for(autoLock, TIMEOUT);
    if (status == std::cv_status::timeout) {
      LOGE("NativeImageReader::onUpdateTexture(): timeout when waiting for the frame available!");
      return {};
    }
  }
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return {};
  }
  frameAvailable = false;
  env->CallVoidMethod(surfaceTexture.get(), SurfaceTexture_updateTexImage);
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LOGE("NativeImageReader::onUpdateTexture(): failed to updateTexImage!");
    return {};
  }
  auto floatArray = env->NewFloatArray(16);
  env->CallVoidMethod(surfaceTexture.get(), SurfaceTexture_getTransformMatrix, floatArray);
  auto matrix = env->GetFloatArrayElements(floatArray, nullptr);
  auto textureSize = ComputeTextureSize(matrix, width(), height());
  env->ReleaseFloatArrayElements(floatArray, matrix, 0);
  return textureSize;
}

}  // namespace tgfx
