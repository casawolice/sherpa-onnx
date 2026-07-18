[![English](https://img.shields.io/badge/README-English-lightgrey.svg)](./README.md)
[![中文](https://img.shields.io/badge/README-中文-blue.svg)](./README-zh.md)

# sherpa_onnx_ios

这是 [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) 的一个子项目。

一般不需要直接依赖这个包 —— 依赖
[`sherpa_onnx`](https://pub.dev/packages/sherpa_onnx) 即可，它会自动把这个包
作为 iOS 平台实现拉进来。

## 本 fork：不内嵌 ORT、与宿主 App 共用一份 onnxruntime 的瘦身版

本目录下的 `sherpa_onnx.xcframework`（`ios/sherpa_onnx.xcframework`）**不是**
上游官方构建。它是用 `SHERPA_ONNX_IOS_NO_EMBED_ORT=1` 构建的（见仓库根目录的
[`build-ios-shared.sh`](../../build-ios-shared.sh)），也就是说它**不内嵌**自己
那份 onnxruntime。它的 `OrtGetApiBase` 符号（以及 CoreML 的 provider-factory
符号）被留为未定义 ——"运行时动态查找"—— 加载时由宿主 App 已有的那份
onnxruntime 来解析。

### 为什么要这么做

官方 `sherpa_onnx_ios` 会在它的 framework 里内嵌一份静态 onnxruntime
（约 24MB）。如果你的 App 里还用了别的自带 onnxruntime 的插件 —— 比如
[`onnxruntime`](https://pub.dev/packages/onnxruntime) Dart 包（经由 CocoaPods
的 `onnxruntime-c` / `onnxruntime-objc` 引入）—— 那么同一个进程里就会出现
**两份 onnxruntime**，在 iOS 上基本必崩溃。这份瘦身版让 sherpa-onnx 共用宿主 App
已有的那唯一一份 onnxruntime，而不是自己再带一份。

### 在你的 App 里怎么用

在 `pubspec.yaml` 里用 `dependency_overrides`，把 `sherpa_onnx_ios` 依赖指向
本 fork 的这个 commit：

```yaml
dependency_overrides:
  sherpa_onnx_ios:
    git:
      url: https://github.com/casawolice/sherpa-onnx.git
      ref: c5ac5a1361f946893bad9b197a8223b3e316bfad   # 锁定 commit，而非分支
      path: flutter/sherpa_onnx_ios
```

然后：

```bash
flutter pub get
cd ios && pod install
```

前置要求：

- 你的 App 必须已经在运行时提供 onnxruntime —— 通常是依赖了 `onnxruntime`
  这个 Dart 包（或以其他方式引入了 `onnxruntime-c` 这个 CocoaPod）。**不要**
  移除它：这份瘦身版就是靠它来解析符号的。
- 那份 onnxruntime 必须是 **1.27.0 版本**，与本包编译时所用的 ORT 头文件版本
  一致（理论上 ABI 兼容的 ≥ 1.27.0 版本也可以，但目前只验证过 1.27.0）。
- `ios/Podfile` 必须使用 `use_frameworks!`（动态链接），这是 Flutter 的默认
  配置。

`pod install` 之后，建议验证一下装进工程的确实是这份瘦身版、且运行时符号能
正确解析 —— 不崩溃、不报 `symbol not found: _OrtGetApiBase`：

```bash
b=ios/.symlinks/plugins/sherpa_onnx_ios/ios/sherpa_onnx.xcframework/ios-arm64/sherpa_onnx.framework/sherpa_onnx
ls -lh "$b"                                   # 期望几 MB，不是约 24MB
nm -m "$b" | grep OrtGetApiBase                # 期望："(undefined) ... dynamically looked up"
otool -L "$b" | grep onnxruntime || echo ok    # 期望：无 onnxruntime 链接依赖
```

完整使用指南、故障排查、以及符号共享的原理说明，见：
[`docs/flutter-ios-shared-ort.md`](../../docs/flutter-ios-shared-ort.md)。
