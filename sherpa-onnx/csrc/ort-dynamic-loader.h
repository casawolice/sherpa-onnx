// sherpa-onnx/csrc/ort-dynamic-loader.h
//
// Copyright (c)  2026  Xiaomi Corporation

#ifndef SHERPA_ONNX_CSRC_ORT_DYNAMIC_LOADER_H_
#define SHERPA_ONNX_CSRC_ORT_DYNAMIC_LOADER_H_

// This file is only compiled/used when SHERPA_ONNX_ENABLE_DYNAMIC_ORT_LOADING
// is turned ON.
//
// In that mode, sherpa-onnx does NOT link against libonnxruntime at build
// time. Instead, the onnxruntime shared library is loaded at runtime with
// dlopen()/LoadLibrary() and the required C-exported entry points
// (OrtGetApiBase and the platform-specific provider factory functions) are
// resolved with dlsym()/GetProcAddress().
//
// The main benefit is that sherpa-onnx can be embedded into a host application
// that already ships/loads its own onnxruntime, so that a single onnxruntime
// instance is shared instead of two conflicting copies being linked into the
// same process.
//
// The library that gets loaded can be overridden with the environment variable
//   SHERPA_ONNX_DYNAMIC_ORT_LIBRARY_PATH
// which may be either a bare library name (resolved through the usual loader
// search path) or an absolute path. When unset, a platform-default name such
// as libonnxruntime.so / libonnxruntime.dylib / onnxruntime.dll is used.

namespace sherpa_onnx {

// Returns a handle (dlopen/LoadLibrary handle) to the onnxruntime shared
// library. The library is loaded lazily on the first call.
//
// It first tries to reuse an onnxruntime that the host process has already
// loaded (so that a single instance is shared); if none is loaded yet, it
// loads one itself.
//
// Returns nullptr if the library cannot be located/loaded (an error message is
// logged in that case).
void *GetOnnxRuntimeLibraryHandle();

}  // namespace sherpa_onnx

#endif  // SHERPA_ONNX_CSRC_ORT_DYNAMIC_LOADER_H_
