// sherpa-onnx/csrc/ort-dynamic-loader.cc
//
// Copyright (c)  2026  Xiaomi Corporation

#include "sherpa-onnx/csrc/ort-dynamic-loader.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cstdlib>
#include <vector>

#include "onnxruntime_c_api.h"  // NOLINT
#include "sherpa-onnx/csrc/macros.h"

// The provider factory functions below are C symbols directly exported by the
// onnxruntime shared library (they are NOT reachable through the OrtApi
// function-pointer table). When the corresponding execution provider is
// compiled in, session.cc references them, so we must provide definitions that
// forward to the dynamically-loaded onnxruntime. We include the same headers,
// under the same conditions, as session.cc so that our definitions match the
// official declarations exactly.
#if defined(__APPLE__) && (ORT_API_VERSION >= 15) && \
    !defined(SHERPA_ONNX_DISABLE_COREML)
#include "coreml_provider_factory.h"  // NOLINT
#endif

#if __ANDROID_API__ >= 27
#include "nnapi_provider_factory.h"  // NOLINT
#endif

#if defined(_WIN32) && SHERPA_ONNX_ENABLE_DIRECTML == 1
#include "dml_provider_factory.h"  // NOLINT
#endif

namespace sherpa_onnx {

#if defined(_WIN32)
using LibHandle = HMODULE;

static LibHandle PlatformReuseOpen(const char *name) {
  // Return a handle only if the module is already loaded into the process.
  return GetModuleHandleA(name);
}

static LibHandle PlatformOpen(const char *name) { return LoadLibraryA(name); }

static void *PlatformSym(LibHandle h, const char *sym) {
  return reinterpret_cast<void *>(GetProcAddress(h, sym));
}

// Candidate library names, tried in order.
static std::vector<const char *> CandidateLibraryNames() {
  return {"onnxruntime.dll"};
}
#else
using LibHandle = void *;

static LibHandle PlatformReuseOpen(const char *name) {
  // RTLD_NOLOAD returns a handle only if the library is already loaded in the
  // process. This is what lets us share the host application's onnxruntime
  // instance instead of mapping a second copy: if the host already loaded, say,
  // libonnxruntime.so.1, opening that same soname here returns the existing
  // mapping (its reference count is simply incremented).
  return dlopen(name, RTLD_NOLOAD | RTLD_NOW | RTLD_GLOBAL);
}

static LibHandle PlatformOpen(const char *name) {
  return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
}

static void *PlatformSym(LibHandle h, const char *sym) {
  return dlsym(h, sym);
}

static std::vector<const char *> CandidateLibraryNames() {
#if defined(__APPLE__)
  return {"libonnxruntime.dylib", "libonnxruntime.1.dylib"};
#else
  // The Microsoft onnxruntime Linux release ships libonnxruntime.so.1.x.y with
  // an "libonnxruntime.so.1" soname and an unversioned "libonnxruntime.so"
  // symlink; a host that linked it will have "libonnxruntime.so.1" loaded.
  return {"libonnxruntime.so", "libonnxruntime.so.1"};
#endif
}
#endif

static LibHandle LoadOrtLibrary() {
  const char *override_path =
      std::getenv("SHERPA_ONNX_DYNAMIC_ORT_LIBRARY_PATH");

  // When the user explicitly points us at a library, use exactly that (still
  // preferring an already-loaded copy so a single instance is shared).
  std::vector<const char *> names;
  if (override_path && override_path[0] != '\0') {
    names.push_back(override_path);
  } else {
    names = CandidateLibraryNames();
  }

  // Pass 1: reuse an onnxruntime already loaded in the process, if any.
  for (const char *name : names) {
    LibHandle handle = PlatformReuseOpen(name);
    if (handle) {
      SHERPA_ONNX_LOGE(
          "Reusing the onnxruntime already loaded in the current process: "
          "'%s'",
          name);
      return handle;
    }
  }

  // Pass 2: otherwise load it ourselves.
  for (const char *name : names) {
    LibHandle handle = PlatformOpen(name);
    if (handle) {
      SHERPA_ONNX_LOGE("Loaded the onnxruntime shared library '%s'", name);
      return handle;
    }
  }

  SHERPA_ONNX_LOGE(
      "Failed to load the onnxruntime shared library (tried '%s'%s). "
      "Make sure it is available in the loader search path, or set the "
      "environment variable SHERPA_ONNX_DYNAMIC_ORT_LIBRARY_PATH to its full "
      "path.",
      names.front(), names.size() > 1 ? ", ..." : "");
  return nullptr;
}

void *GetOnnxRuntimeLibraryHandle() {
  // Thread-safe, initialized exactly once on first use.
  static LibHandle handle = LoadOrtLibrary();
  return reinterpret_cast<void *>(handle);
}

static void *ResolveSymbol(const char *sym) {
  void *h = GetOnnxRuntimeLibraryHandle();
  if (!h) {
    return nullptr;
  }

  void *p = PlatformSym(reinterpret_cast<LibHandle>(h), sym);
  if (!p) {
    SHERPA_ONNX_LOGE(
        "Symbol '%s' was not found in the loaded onnxruntime library.", sym);
  }
  return p;
}

}  // namespace sherpa_onnx

extern "C" {

// This is the single C entry point of onnxruntime. The header-only C++ wrapper
// (onnxruntime_cxx_api.h) calls it once to obtain the OrtApi function-pointer
// table via OrtGetApiBase()->GetApi(ORT_API_VERSION). By defining it here and
// forwarding to the dynamically-loaded library, every Ort::GetApi()-based call
// site keeps working without any change.
ORT_EXPORT const OrtApiBase *ORT_API_CALL OrtGetApiBase(void) NO_EXCEPTION {
  using Fn = const OrtApiBase *(ORT_API_CALL *)(void);
  static Fn fn =
      reinterpret_cast<Fn>(sherpa_onnx::ResolveSymbol("OrtGetApiBase"));
  if (!fn) {
    SHERPA_ONNX_LOGE(
        "Fatal: OrtGetApiBase could not be resolved from any onnxruntime "
        "library. sherpa-onnx cannot run without onnxruntime.");
    return nullptr;
  }
  return fn();
}

#if __ANDROID_API__ >= 27
OrtStatus *ORT_API_CALL OrtSessionOptionsAppendExecutionProvider_Nnapi(
    OrtSessionOptions *options, uint32_t nnapi_flags) NO_EXCEPTION {
  using Fn = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, uint32_t);
  static Fn fn = reinterpret_cast<Fn>(sherpa_onnx::ResolveSymbol(
      "OrtSessionOptionsAppendExecutionProvider_Nnapi"));
  if (!fn) {
    return nullptr;
  }
  return fn(options, nnapi_flags);
}
#endif

#if defined(__APPLE__) && (ORT_API_VERSION >= 15) && \
    !defined(SHERPA_ONNX_DISABLE_COREML)
OrtStatus *ORT_API_CALL OrtSessionOptionsAppendExecutionProvider_CoreML(
    OrtSessionOptions *options, uint32_t coreml_flags) NO_EXCEPTION {
  using Fn = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, uint32_t);
  static Fn fn = reinterpret_cast<Fn>(sherpa_onnx::ResolveSymbol(
      "OrtSessionOptionsAppendExecutionProvider_CoreML"));
  if (!fn) {
    return nullptr;
  }
  return fn(options, coreml_flags);
}
#endif

#if defined(_WIN32) && SHERPA_ONNX_ENABLE_DIRECTML == 1
OrtStatus *ORT_API_CALL OrtSessionOptionsAppendExecutionProvider_DML(
    OrtSessionOptions *options, int device_id) NO_EXCEPTION {
  using Fn = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, int);
  static Fn fn = reinterpret_cast<Fn>(sherpa_onnx::ResolveSymbol(
      "OrtSessionOptionsAppendExecutionProvider_DML"));
  if (!fn) {
    return nullptr;
  }
  return fn(options, device_id);
}
#endif

}  // extern "C"
