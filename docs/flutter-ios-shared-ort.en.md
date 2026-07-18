[![English](https://img.shields.io/badge/doc-English-blue.svg)](./flutter-ios-shared-ort.en.md)
[![中文](https://img.shields.io/badge/doc-中文-lightgrey.svg)](./flutter-ios-shared-ort.md)

# Flutter iOS: sharing a single onnxruntime with the host app (slim sherpa_onnx)

## What this is

The official `sherpa_onnx` iOS plugin embeds a **static onnxruntime** inside
its `sherpa_onnx.framework` (~24MB). If your Flutter app also uses **another
plugin that goes through onnxruntime** — e.g. the `onnxruntime` Dart package,
which pulls in its own ORT via `onnxruntime-objc` / `onnxruntime-c` — you end
up with **two onnxruntime copies in the same process**, which reliably
crashes on iOS.

This slim `sherpa_onnx.xcframework` is built with
`SHERPA_ONNX_IOS_NO_EMBED_ORT=1`: sherpa **no longer embeds ORT** and leaves
`OrtGetApiBase` (and friends) undefined — "dynamically looked up" — resolved
at runtime against the single `onnxruntime-c` your app already has. The whole
app ends up with exactly one ORT, and the conflict disappears.

- Size: each slice is roughly **3–5MB** (vs. ~24MB with ORT embedded).
- Requirement: **the versions must match** — sherpa and `onnxruntime-c` both
  need to be **1.27.0** (ABI-compatible).
- iOS only. Other platforms are unaffected.

## When you need this

| Your situation | Do you need this package? |
|---|---|
| App only uses sherpa_onnx, no other onnxruntime plugin | No, use the official package |
| App uses sherpa_onnx **and** `onnxruntime` (or another ORT-based plugin), and crashes on iOS | **Yes, use this package** |

## Usage

### 1. Point your app at the slim iOS plugin (recommended: git dependency)

The slim `sherpa_onnx.xcframework` is committed directly in this repo under
[`flutter/sherpa_onnx_ios/ios/`](../flutter/sherpa_onnx_ios/ios/) — no Release
download or local build needed. Override the dependency in your app's
`pubspec.yaml` with a git source:

```yaml
dependency_overrides:
  sherpa_onnx_ios:
    git:
      url: https://github.com/casawolice/sherpa-onnx.git
      ref: c5ac5a1361f946893bad9b197a8223b3e316bfad   # pin a commit, not a branch
      path: flutter/sherpa_onnx_ios
```

Pin `ref` to a specific commit hash (not `master`), so anyone who clones your
app gets the exact same xcframework from `flutter pub get`, with no drift
from later fork updates. To upgrade, find a newer commit at
[casawolice/sherpa-onnx](https://github.com/casawolice/sherpa-onnx/commits/master)
and update `ref` manually.

### 1b. Alternative: local path / build it yourself

If you're iterating on the sherpa source locally, build it yourself and use a
`path:` override instead:

```bash
git clone https://github.com/casawolice/sherpa-onnx
cd sherpa-onnx
SHERPA_ONNX_IOS_NO_EMBED_ORT=1 ./build-ios-shared.sh
# the output is installed automatically into
# flutter/sherpa_onnx_ios/ios/sherpa_onnx.xcframework
```

```yaml
dependency_overrides:
  sherpa_onnx_ios:
    path: /absolute/path/to/sherpa-onnx/flutter/sherpa_onnx_ios
```

(You can also download `sherpa_onnx-ios-flutter.xcframework.zip` from this
repo's **Releases**, unzip it, and drop it into a local copy of
`sherpa_onnx_ios`'s `ios/` directory, then point `path:` at that — same
effect.)

### 2. Keep the single onnxruntime your app already has

Don't remove your `onnxruntime` Dart package — its `onnxruntime-c 1.27.0` is
the one and only ORT for the whole app, and sherpa will share it. Make sure
`ios/Podfile` uses `use_frameworks!` (dynamic framework linking, which is how
`onnxruntime-c` ends up shared):

```ruby
platform :ios, '15.1'
target 'Runner' do
  use_frameworks!
  flutter_install_all_ios_pods File.dirname(File.realpath(__FILE__))
end
```

### 3. Install and run

```bash
cd your_app
flutter pub get
cd ios && pod install && cd ..
flutter run          # simulator or a real device
```

## Verifying the slim framework is correct

```bash
b=flutter/sherpa_onnx_ios/ios/sherpa_onnx.xcframework/ios-arm64/sherpa_onnx.framework/sherpa_onnx
ls -lh "$b"                                   # expect a few MB, not ~24MB
nm -m "$b" | grep OrtGetApiBase               # expect: (undefined) ... dynamically looked up
otool -L "$b" | grep onnxruntime || echo ok   # expect: no onnxruntime dependency
```

## FAQ

- **Crashes with `symbol not found: _OrtGetApiBase`**: your app has no ORT to
  resolve against. Make sure the `onnxruntime` Dart package is still a
  dependency, that `pod install` pulled in `onnxruntime-c`, and that the
  `Podfile` uses `use_frameworks!` (dynamic linking).
- **Version mismatch**: sherpa's header version must match `onnxruntime-c`'s
  (this package uses 1.27.0). Keep both in sync when upgrading either side.
- **Android / desktop**: this specific "share one ORT" trick is iOS-only.
  Android/desktop have a separate `SHERPA_ONNX_ENABLE_DYNAMIC_ORT_LOADING`
  (dlopen) mode — see the root README.

## How it works

onnxruntime's C API is exposed through a **function-pointer table** returned
by `OrtGetApiBase()`. As long as the process has exactly one `OrtGetApiBase`
and its implementation, sherpa and your own inference code get back the same
table and the same global state — that's what "sharing one ORT" means. The
slim build leaves that symbol undefined and lets the host's `onnxruntime-c`
provide it, which is exactly how this is achieved.
