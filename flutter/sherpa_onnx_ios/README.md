[![English](https://img.shields.io/badge/README-English-blue.svg)](./README.md)
[![中文](https://img.shields.io/badge/README-中文-lightgrey.svg)](./README-zh.md)

# sherpa_onnx_ios

This is a sub project of [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx).

You are not expected to use this package directly — depend on
[`sherpa_onnx`](https://pub.dev/packages/sherpa_onnx) instead, which pulls this
one in automatically as its iOS platform implementation.

## This fork: a slim build that shares the host app's onnxruntime

The `sherpa_onnx.xcframework` in this directory (`ios/sherpa_onnx.xcframework`)
is **not** the stock upstream build. It was built with
`SHERPA_ONNX_IOS_NO_EMBED_ORT=1` (see [`build-ios-shared.sh`](../../build-ios-shared.sh)
at the repo root), which means it does **not** embed its own copy of
onnxruntime. Its `OrtGetApiBase` symbol (and CoreML's provider-factory symbol)
are left undefined — "dynamically looked up" — and get resolved at load time
against whatever onnxruntime the host app already provides.

### Why

The stock `sherpa_onnx_ios` embeds a static onnxruntime (~24MB) inside its
framework. If your app also uses another plugin that brings its own
onnxruntime — e.g. the [`onnxruntime`](https://pub.dev/packages/onnxruntime)
Dart package, via CocoaPods `onnxruntime-c` / `onnxruntime-objc` — you end up
with **two onnxruntime copies in the same process**, which reliably crashes on
iOS. This slim build makes sherpa-onnx share the single onnxruntime the host
app already has, instead of shipping a second one.

### How to use it in your app

Point your app's `sherpa_onnx_ios` dependency at this fork+commit via
`dependency_overrides` in `pubspec.yaml`:

```yaml
dependency_overrides:
  sherpa_onnx_ios:
    git:
      url: https://github.com/casawolice/sherpa-onnx.git
      ref: c5ac5a1361f946893bad9b197a8223b3e316bfad   # pin a commit, not a branch
      path: flutter/sherpa_onnx_ios
```

Then:

```bash
flutter pub get
cd ios && pod install
```

Requirements:

- Your app must already provide onnxruntime at runtime — normally by
  depending on the `onnxruntime` Dart package (or otherwise pulling in the
  `onnxruntime-c` CocoaPod). Do **not** remove it: it's the ORT this slim
  build resolves against.
- That onnxruntime must be **version 1.27.0**, matching the ORT headers this
  package was compiled against (ABI-compatible versions ≥ 1.27.0 should also
  work, but 1.27.0 is what's verified).
- `ios/Podfile` must use `use_frameworks!` (dynamic linking), which is
  Flutter's default.

Verify after `pod install` that the framework installed is really this slim
build and that the symbol resolves correctly at runtime — no crash, no
`symbol not found: _OrtGetApiBase`:

```bash
b=ios/.symlinks/plugins/sherpa_onnx_ios/ios/sherpa_onnx.xcframework/ios-arm64/sherpa_onnx.framework/sherpa_onnx
ls -lh "$b"                                   # a few MB, not ~24MB
nm -m "$b" | grep OrtGetApiBase                # "(undefined) ... dynamically looked up"
otool -L "$b" | grep onnxruntime || echo ok    # no onnxruntime link dependency
```

Full usage guide, troubleshooting, and how the symbol sharing works:
[`docs/flutter-ios-shared-ort.en.md`](../../docs/flutter-ios-shared-ort.en.md)
(中文版: [`docs/flutter-ios-shared-ort.md`](../../docs/flutter-ios-shared-ort.md)).
