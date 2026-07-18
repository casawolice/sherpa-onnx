# Flutter iOS：与宿主 App 共用一份 onnxruntime（瘦身版 sherpa_onnx）

## 这是什么

官方 `sherpa_onnx` 的 iOS 插件把一份 **onnxruntime 静态库内嵌**进了它的
`sherpa_onnx.framework`（约 24MB）。如果你的 Flutter App 里**还用了别的走
onnxruntime 的插件**（例如 `onnxruntime` Dart 包，它经由 `onnxruntime-objc /
onnxruntime-c` 引入自己的一份 ORT），同一个进程里就会出现**两份 onnxruntime**，
在 iOS 上通常直接 crash。

本瘦身版 `sherpa_onnx.xcframework` 用 `SHERPA_ONNX_IOS_NO_EMBED_ORT=1` 构建：
sherpa **不再内嵌 ORT**，把 `OrtGetApiBase` 等符号留为未定义
（`dynamically looked up`），运行时由 App 里那唯一一份 `onnxruntime-c` 解析。
于是全 App 只有一份 ORT，冲突消失。

- 体积：每个 slice 约 **3–5MB**（对比内嵌版 24MB）。
- 前提：**版本必须一致** —— sherpa 与 `onnxruntime-c` 都用 **1.27.0**（ABI 兼容）。
- 仅 iOS。其它平台不受影响。

## 适用判断

| 你的情况 | 是否需要这个包 |
|---|---|
| App 里只有 sherpa_onnx，没有别的 onnxruntime 插件 | 不需要，用官方包即可 |
| App 里同时有 sherpa_onnx **和** `onnxruntime`（或其它带 ORT 的插件），iOS 崩溃 | **需要本包** |

## 用法

### 1. 让 App 使用瘦身版 iOS 插件（推荐：git 依赖）

瘦身版 `sherpa_onnx.xcframework` 已直接提交在本仓库
[`flutter/sherpa_onnx_ios/ios/`](../flutter/sherpa_onnx_ios/ios/) 下，无需下载
Release 或本地构建，在 App 的 `pubspec.yaml` 里直接用 git 依赖覆盖即可：

```yaml
dependency_overrides:
  sherpa_onnx_ios:
    git:
      url: https://github.com/casawolice/sherpa-onnx.git
      ref: c5ac5a1361f946893bad9b197a8223b3e316bfad   # 锁定 commit，而非分支
      path: flutter/sherpa_onnx_ios
```

`ref` 建议锁定到具体 commit hash（而非 `master`），保证任何人 clone 你的 App 后
`flutter pub get` 拿到的都是同一份 xcframework，不随 fork 更新漂移。要升级时，
去 [casawolice/sherpa-onnx](https://github.com/casawolice/sherpa-onnx/commits/master)
找到新的 commit hash，手动更新 `ref`。

### 1b. 备选：本地路径 / 自行构建

如果你需要修改 sherpa 源码后立刻联调，可以本地构建并用 `path:` 覆盖：

```bash
git clone https://github.com/casawolice/sherpa-onnx
cd sherpa-onnx
SHERPA_ONNX_IOS_NO_EMBED_ORT=1 ./build-ios-shared.sh
# 产物会自动装进 flutter/sherpa_onnx_ios/ios/sherpa_onnx.xcframework
```

```yaml
dependency_overrides:
  sherpa_onnx_ios:
    path: /absolute/path/to/sherpa-onnx/flutter/sherpa_onnx_ios
```

（也可以从本仓库 **Releases** 下载 `sherpa_onnx-ios-flutter.xcframework.zip`
解压后手动替换到某份本地 `sherpa_onnx_ios` 插件的 `ios/` 目录下，再用 `path:`
指向它，效果等同。）

### 2. 保留 App 里那唯一一份 onnxruntime

不要动你的 `onnxruntime` Dart 包 —— 它带的 `onnxruntime-c 1.27.0` 就是全 App
唯一的 ORT，sherpa 会共用它。确认 `ios/Podfile` 是 `use_frameworks!`（动态
framework 链接，`onnxruntime-c` 因此以动态 framework 形式被共享）：

```ruby
platform :ios, '15.1'
target 'Runner' do
  use_frameworks!
  flutter_install_all_ios_pods File.dirname(File.realpath(__FILE__))
end
```

### 3. 安装并运行

```bash
cd your_app
flutter pub get
cd ios && pod install && cd ..
flutter run          # 模拟器或真机
```

## 验证瘦身 framework 是否正确

```bash
b=flutter/sherpa_onnx_ios/ios/sherpa_onnx.xcframework/ios-arm64/sherpa_onnx.framework/sherpa_onnx
ls -lh "$b"                                   # 期望几 MB，不是 24MB
nm -m "$b" | grep OrtGetApiBase               # 期望: (undefined) ... dynamically looked up
otool -L "$b" | grep onnxruntime || echo ok   # 期望: 无 onnxruntime 依赖
```

## 常见问题

- **运行报 `symbol not found: _OrtGetApiBase`**：说明 App 里没有可解析的 ORT。
  确认 `onnxruntime` Dart 包仍在依赖里、`pod install` 已拉入 `onnxruntime-c`，
  且 `Podfile` 用的是 `use_frameworks!`（动态链接）。
- **版本不一致**：sherpa 头文件版本必须与 `onnxruntime-c` 的一致（本包为 1.27.0）。
  升级一方时另一方要同步。
- **Android / 桌面**：本改造只针对 iOS 的“共用一份 ORT”。Android/桌面另有
  `SHERPA_ONNX_ENABLE_DYNAMIC_ORT_LOADING`（dlopen）模式，见仓库根 README。

## 原理

onnxruntime 的 C API 通过 `OrtGetApiBase()` 返回的**函数指针表**暴露。只要进程里
只有一份 `OrtGetApiBase` 及其实现，sherpa 和你的推理代码拿到的就是同一张表、同一套
全局状态 —— 这就是“共用一份 ORT”。瘦身版把该符号留未定义、交给宿主的
`onnxruntime-c` 提供，正是实现这一点。
