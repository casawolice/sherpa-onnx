<div align="center">

[![English](https://img.shields.io/badge/README-English-lightgrey.svg)](./README.md)
[![中文](https://img.shields.io/badge/README-中文-blue.svg)](./README-zh.md)

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/k2-fsa/sherpa-onnx)

</div>

> 本文件是 [README.md](./README.md) 的中文版本，内容与英文版保持一致。若两者出现不一致，
> 请以英文 [README.md](./README.md) 为准。本 fork 相较上游新增了「[共享宿主的 ONNX
> Runtime（动态加载）](#共享宿主的-onnx-runtime动态加载)」能力。

 ### 支持的功能

| 语音识别 | [语音合成][tts-url] | [音源分离][ss-url] |
|------------------|------------------|-------------------|
|   ✔️              |         ✔️        |       ✔️           |

| 说话人识别 | [说话人分离][sd-url] | 说话人确认 |
|----------------------|-------------------- |------------------------|
|   ✔️                  |         ✔️           |            ✔️           |

| [口语语种识别][slid-url] | [音频事件标注][at-url] | [语音活动检测][vad-url] |
|--------------------------------|---------------|--------------------------|
|                 ✔️              |          ✔️    |                ✔️         |

| [关键词检测][kws-url] | [添加标点][punct-url] | [语音增强][se-url] |
|------------------|-----------------|--------------------|
|     ✔️            |       ✔️         |      ✔️             |


### 支持的平台

| 架构 | Android | iOS     | Windows    | macOS | linux | HarmonyOS |
|------------|---------|---------|------------|-------|-------|-----------|
|   x64      |  ✔️      |         |   ✔️      | ✔️    |  ✔️    |   ✔️   |
|   x86      |  ✔️      |         |   ✔️      |       |        |        |
|   arm64    |  ✔️      | ✔️      |   ✔️      | ✔️    |  ✔️    |   ✔️   |
|   arm32    |  ✔️      |         |           |       |  ✔️    |   ✔️   |
|   riscv64  |          |         |           |       |  ✔️    |        |

### 支持的编程语言

| 1. C++ | 2. C  | 3. Python | 4. JavaScript |
|--------|-------|-----------|---------------|
|   ✔️    | ✔️     | ✔️         |    ✔️          |

|5. Java | 6. C# | 7. Kotlin | 8. Swift |
|--------|-------|-----------|----------|
| ✔️      |  ✔️    | ✔️         |  ✔️       |

| 9. Go | 10. Dart | 11. Rust | 12. Pascal |
|-------|----------|----------|------------|
| ✔️     |  ✔️       |   ✔️      |    ✔️       |


同时支持 WebAssembly。

### 支持的 NPU

| [1. Rockchip NPU (RKNN)][rknpu-doc] | [2. Qualcomm NPU (QNN)][qnn-doc]  | [3. Ascend NPU][ascend-doc] |
|-------------------------------------|-----------------------------------|-----------------------------|
|     ✔️                              |                  ✔️               |     ✔️                      |

| [4. Axera NPU][axera-npu] |
|---------------------------|
|     ✔️                    |

[加入我们的 Discord](https://discord.gg/fJdxzg2VbG)


## 共享宿主的 ONNX Runtime（动态加载）

默认情况下，sherpa-onnx 在**编译期**链接 `libonnxruntime`，并把它打包进产物。本 fork
新增了一个可选模式：改为在**运行时**通过 `dlopen` / `LoadLibrary` 加载 ONNX Runtime
（下称 ORT）。这样就能把 sherpa-onnx 嵌入到一个**本身已经自带 ORT** 的宿主程序里，
让两者**共用同一份 ORT 实例**——避免在同一进程里链接两份 ORT 导致的符号冲突 / 版本冲突。

编译期只需要 ORT 的头文件；最终产物 `libsherpa-onnx-*` 对 `libonnxruntime`
**没有链接期依赖**。

### 如何开启

打开 CMake 选项（默认 `OFF`，即普通构建行为完全不变）：

```bash
cmake -DSHERPA_ONNX_ENABLE_DYNAMIC_ORT_LOADING=ON /path/to/sherpa-onnx
```

开启后：

- sherpa-onnx 不再链接 `libonnxruntime`，也不会把它打包进安装目录 / wheel / AAR 等。
- 运行时由 sherpa-onnx 定义 `OrtGetApiBase`（ORT 唯一的 C 入口）以及
  NNAPI / CoreML / DirectML 的 provider 工厂符号，转发到它加载的那份 ORT。
  现有的模型 / 解码代码**一行都不用改**。

### 运行时如何定位 ORT 库

1. **`SHERPA_ONNX_DYNAMIC_ORT_LIBRARY_PATH`**——若设置了该环境变量（可以是库名，
   也可以是绝对路径），就使用它指定的库。
2. 否则按平台尝试默认名：
   `libonnxruntime.so` / `libonnxruntime.so.1`（Linux/Android）、
   `libonnxruntime.dylib` / `libonnxruntime.1.dylib`（macOS）、
   `onnxruntime.dll`（Windows）。

两种情况下，如果宿主进程**已经加载**了同名库，都会复用已存在的实例
（`RTLD_NOLOAD`），从而做到共用一份 ORT、而不是重复映射两份。如果最终找不到任何可用
的 ORT，会打印清晰的错误日志、ORT 调用优雅失败，而不是直接崩溃。

> 版本提示：宿主提供的 ORT 必须与 sherpa-onnx 编译时使用的头文件**ABI 兼容**，
> 即其 `ORT_API_VERSION` 应**大于等于**编译时的版本。

### 示例（Linux）

```bash
# 以动态加载模式构建 sherpa-onnx
cmake -DSHERPA_ONNX_ENABLE_DYNAMIC_ORT_LOADING=ON -B build
cmake --build build

# 产物的 NEEDED 依赖里不再有 libonnxruntime：
ldd build/lib/libsherpa-onnx-c-api.so | grep onnxruntime   # -> 无输出

# 指向宿主提供的 ORT（或直接依赖默认搜索路径）：
export SHERPA_ONNX_DYNAMIC_ORT_LIBRARY_PATH=/opt/host-app/lib/libonnxruntime.so.1
./your-host-app     # 此时 sherpa-onnx 与宿主共用同一份 ORT 实例
```


## 简介

本仓库支持在**本地**（离线）运行以下功能：

  - 语音转文字（即 ASR），流式与非流式均支持
  - 文字转语音（即 TTS）
  - 说话人分离（Speaker diarization）
  - 说话人识别（Speaker identification）
  - 说话人确认（Speaker verification）
  - 口语语种识别
  - 音频事件标注（Audio tagging）
  - 语音活动检测（VAD，例如 [silero-vad][silero-vad]）
  - 语音增强（例如 [gtcrn][gtcrn]、[DPDFNet](https://github.com/ceva-ip/DPDFNet)）
  - 关键词检测
  - 音源分离（例如 [spleeter][spleeter]、[UVR][UVR]）

支持以下平台与操作系统：

  - x86、``x86_64``、32 位 ARM、64 位 ARM（arm64、aarch64）、RISC-V（riscv64）、**RK NPU**、**Ascend NPU**
  - Linux、macOS、Windows、openKylin
  - Android、WearOS
  - iOS
  - HarmonyOS
  - NodeJS
  - WebAssembly
  - [NVIDIA Jetson Orin NX][NVIDIA Jetson Orin NX]（支持在 CPU 和 GPU 上运行）
  - [NVIDIA Jetson Nano B01][NVIDIA Jetson Nano B01]（支持在 CPU 和 GPU 上运行）
  - [树莓派 Raspberry Pi][Raspberry Pi]
  - [RV1126][RV1126]
  - [LicheePi4A][LicheePi4A]
  - [VisionFive 2][VisionFive 2]
  - [旭日X3派][旭日X3派]
  - [爱芯派][爱芯派]
  - [RK3588][RK3588]
  - [SpacemiT-K1][SpacemiT-K1]
  - [SpacemiT-K3][SpacemiT-K3]
  - 等等

并提供以下 API：

  - C++、C、Python、Go、``C#``
  - Java、Kotlin、JavaScript
  - Swift、Rust
  - Dart、Object Pascal

### Huggingface Spaces 试用链接

<details>
<summary>无需安装任何东西，只要有浏览器，即可访问以下 Huggingface Spaces 在线试用 sherpa-onnx。</summary>

| 说明                                                  | 链接                                    | 中国镜像                               |
|-------------------------------------------------------|-----------------------------------------|----------------------------------------|
| 说话人分离                                            | [点此][hf-space-speaker-diarization]    | [镜像][hf-space-speaker-diarization-cn]|
| 语音识别                                              | [点此][hf-space-asr]                    | [镜像][hf-space-asr-cn]                |
| 语音识别（[Whisper][Whisper]）                        | [点此][hf-space-asr-whisper]            | [镜像][hf-space-asr-whisper-cn]        |
| 语音合成                                              | [点此][hf-space-tts]                    | [镜像][hf-space-tts-cn]                |
| 生成字幕                                              | [点此][hf-space-subtitle]               | [镜像][hf-space-subtitle-cn]           |
| 音频事件标注                                          | [点此][hf-space-audio-tagging]          | [镜像][hf-space-audio-tagging-cn]      |
| 音源分离                                              | [点此][hf-space-source-separation]      | [镜像][hf-space-source-separation-cn]  |
| 口语语种识别（[Whisper][Whisper]）                    | [点此][hf-space-slid-whisper]           | [镜像][hf-space-slid-whisper-cn]       |

我们还有一些用 WebAssembly 构建的 Spaces，列举如下：

| 说明                                                                                     | Huggingface space| ModelScope space|
|------------------------------------------------------------------------------------------|------------------|-----------------|
|语音活动检测（[silero-vad][silero-vad]）                                                   | [点此][wasm-hf-vad]|[地址][wasm-ms-vad]|
|实时语音识别（中文 + 英文），使用 Zipformer                                                | [点此][wasm-hf-streaming-asr-zh-en-zipformer]|[地址][wasm-hf-streaming-asr-zh-en-zipformer]|
|实时语音识别（中文 + 英文），使用 Paraformer                                               |[点此][wasm-hf-streaming-asr-zh-en-paraformer]| [地址][wasm-ms-streaming-asr-zh-en-paraformer]|
|实时语音识别（中文 + 英文 + 粤语），使用 [Paraformer-large][Paraformer-large]              |[点此][wasm-hf-streaming-asr-zh-en-yue-paraformer]| [地址][wasm-ms-streaming-asr-zh-en-yue-paraformer]|
|实时语音识别（英文） |[点此][wasm-hf-streaming-asr-en-zipformer]    |[地址][wasm-ms-streaming-asr-en-zipformer]|
|VAD + 语音识别（中文），使用 [Zipformer CTC](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-ctc/icefall/zipformer.html#sherpa-onnx-zipformer-ctc-zh-int8-2025-07-03-chinese)|[点此][wasm-hf-vad-asr-zh-zipformer-ctc-07-03]| [地址][wasm-ms-vad-asr-zh-zipformer-ctc-07-03]|
|VAD + 语音识别（中文 + 英文 + 韩文 + 日文 + 粤语），使用 [SenseVoice][SenseVoice]|[点此][wasm-hf-vad-asr-zh-en-ko-ja-yue-sense-voice]| [地址][wasm-ms-vad-asr-zh-en-ko-ja-yue-sense-voice]|
|VAD + 语音识别（英文），使用 [Whisper][Whisper] tiny.en|[点此][wasm-hf-vad-asr-en-whisper-tiny-en]| [地址][wasm-ms-vad-asr-en-whisper-tiny-en]|
|VAD + 语音识别（英文），使用 [Moonshine tiny][Moonshine tiny]|[点此][wasm-hf-vad-asr-en-moonshine-tiny-en]| [地址][wasm-ms-vad-asr-en-moonshine-tiny-en]|
|VAD + 语音识别（英文），使用基于 [GigaSpeech][GigaSpeech] 训练的 Zipformer    |[点此][wasm-hf-vad-asr-en-zipformer-gigaspeech]| [地址][wasm-ms-vad-asr-en-zipformer-gigaspeech]|
|VAD + 语音识别（中文），使用基于 [WenetSpeech][WenetSpeech] 训练的 Zipformer  |[点此][wasm-hf-vad-asr-zh-zipformer-wenetspeech]| [地址][wasm-ms-vad-asr-zh-zipformer-wenetspeech]|
|VAD + 语音识别（日文），使用基于 [ReazonSpeech][ReazonSpeech] 训练的 Zipformer|[点此][wasm-hf-vad-asr-ja-zipformer-reazonspeech]| [地址][wasm-ms-vad-asr-ja-zipformer-reazonspeech]|
|VAD + 语音识别（泰文），使用基于 [GigaSpeech2][GigaSpeech2] 训练的 Zipformer  |[点此][wasm-hf-vad-asr-th-zipformer-gigaspeech2]| [地址][wasm-ms-vad-asr-th-zipformer-gigaspeech2]|
|VAD + 语音识别（中文多种方言），使用 [TeleSpeech-ASR][TeleSpeech-ASR] CTC 模型|[点此][wasm-hf-vad-asr-zh-telespeech]| [地址][wasm-ms-vad-asr-zh-telespeech]|
|VAD + 语音识别（英文 + 中文，及多种中文方言），使用 Paraformer-large          |[点此][wasm-hf-vad-asr-zh-en-paraformer-large]| [地址][wasm-ms-vad-asr-zh-en-paraformer-large]|
|VAD + 语音识别（英文 + 中文，及多种中文方言），使用 Paraformer-small          |[点此][wasm-hf-vad-asr-zh-en-paraformer-small]| [地址][wasm-ms-vad-asr-zh-en-paraformer-small]|
|VAD + 语音识别（多语种及多种中文方言），使用 [Dolphin][Dolphin]-base          |[点此][wasm-hf-vad-asr-multi-lang-dolphin-base]| [地址][wasm-ms-vad-asr-multi-lang-dolphin-base]|
|语音合成（Piper，英文）                                                                  |[点此][wasm-hf-tts-piper-en]| [地址][wasm-ms-tts-piper-en]|
|语音合成（Piper，德文）                                                                   |[点此][wasm-hf-tts-piper-de]| [地址][wasm-ms-tts-piper-de]|
|语音合成（Matcha，中文）                                                                  |[点此][wasm-hf-tts-matcha-zh]| [地址][wasm-ms-tts-matcha-zh]|
|语音合成（Matcha，英文）                                                                  |[点此][wasm-hf-tts-matcha-en]| [地址][wasm-ms-tts-matcha-en]|
|语音合成（Matcha，中文+英文）                                                          |[点此][wasm-hf-tts-matcha-zh-en]| [地址][wasm-ms-tts-matcha-zh-en]|
|说话人分离                                                                         |[点此][wasm-hf-speaker-diarization]|[地址][wasm-ms-speaker-diarization]|
|声音克隆，使用 ZipVoice（中文+英文）                                               |[点此][wasm-hf-voice-cloning-zipvoice]|[地址][wasm-ms-voice-cloning-zipvoice]|
|声音克隆，使用 Pocket TTS（英文）                                               |[点此][wasm-hf-voice-cloning-pocket]|[地址][wasm-ms-voice-cloning-pocket]|

</details>

### 预编译 Android APK 链接

<details>

<summary>下表提供本仓库的预编译 Android APK</summary>

| 说明                                   | 链接                               | 中国用户                          |
|----------------------------------------|------------------------------------|-----------------------------------|
| 说话人分离                             | [地址][apk-speaker-diarization]    | [点此][apk-speaker-diarization-cn]|
| 流式语音识别                           | [地址][apk-streaming-asr]          | [点此][apk-streaming-asr-cn]      |
| 模拟流式语音识别                       | [地址][apk-simula-streaming-asr]   | [点此][apk-simula-streaming-asr-cn]|
| 文字转语音                             | [地址][apk-tts]                    | [点此][apk-tts-cn]                |
| 语音活动检测（VAD）                    | [地址][apk-vad]                    | [点此][apk-vad-cn]                |
| VAD + 非流式语音识别                   | [地址][apk-vad-asr]                | [点此][apk-vad-asr-cn]            |
| 两遍（two-pass）语音识别               | [地址][apk-2pass]                  | [点此][apk-2pass-cn]              |
| 音频事件标注                           | [地址][apk-at]                     | [点此][apk-at-cn]                 |
| 音频事件标注（WearOS）                 | [地址][apk-at-wearos]              | [点此][apk-at-wearos-cn]          |
| 说话人识别                             | [地址][apk-sid]                    | [点此][apk-sid-cn]                |
| 口语语种识别                           | [地址][apk-slid]                   | [点此][apk-slid-cn]               |
| 关键词检测                             | [地址][apk-kws]                    | [点此][apk-kws-cn]                |

</details>

### 预编译 Flutter 应用链接

<details>

#### 实时语音识别

| 说明                           | 链接                                | 中国用户                            |
|--------------------------------|-------------------------------------|-------------------------------------|
| 流式语音识别                   | [地址][apk-flutter-streaming-asr]   | [点此][apk-flutter-streaming-asr-cn]|

#### 文字转语音

| 说明                                     | 链接                               | 中国用户                           |
|------------------------------------------|------------------------------------|------------------------------------|
| Android (arm64-v8a, armeabi-v7a, x86_64) | [地址][flutter-tts-android]        | [点此][flutter-tts-android-cn]     |
| Linux (x64)                              | [地址][flutter-tts-linux]          | [点此][flutter-tts-linux-cn]       |
| macOS (x64)                              | [地址][flutter-tts-macos-x64]      | [点此][flutter-tts-macos-x64-cn] |
| macOS (arm64)                            | [地址][flutter-tts-macos-arm64]    | [点此][flutter-tts-macos-arm64-cn]   |
| Windows (x64)                            | [地址][flutter-tts-win-x64]        | [点此][flutter-tts-win-x64-cn]     |

> 注意：iOS 需要从源码构建。

</details>

### 预编译 Lazarus 应用链接

<details>

#### 生成字幕

| 说明                           | 链接                       | 中国用户                   |
|--------------------------------|----------------------------|----------------------------|
| 生成字幕                       | [地址][lazarus-subtitle]   | [点此][lazarus-subtitle-cn]|

</details>

### 预训练模型链接

<details>

| 说明                                        | 链接                                                                                  |
|---------------------------------------------|---------------------------------------------------------------------------------------|
| 语音识别（语音转文字，ASR）                 | [地址][asr-models]                                                                    |
| 文字转语音（TTS）                           | [地址][tts-models]                                                                    |
| VAD                                         | [地址][vad-models]                                                                    |
| 关键词检测                                  | [地址][kws-models]                                                                    |
| 音频事件标注                                | [地址][at-models]                                                                     |
| 说话人识别（Speaker ID）                    | [地址][sid-models]                                                                    |
| 口语语种识别（Language ID）                 | 见[语音识别][asr-models]中多语种的 [Whisper][Whisper] ASR 模型                        |
| 标点                                        | [地址][punct-models]                                                                  |
| 说话人分段                                  | [地址][speaker-segmentation-models]                                                   |
| 语音增强                                    | [地址][speech-enhancement-models]                                                     |
| 音源分离                                    | [地址][source-separation-models]                                                     |

</details>

#### 部分预训练 ASR 模型（流式）

<details>

更多模型请见

  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/index.html>
  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-paraformer/index.html>
  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-ctc/index.html>

下表仅列出其中**一部分**。


|名称 | 支持的语言 | 说明|
|-----|-----|----|
|[sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20][sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20]| 中文、英文 | [另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#csukuangfj-sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20-bilingual-chinese-english)|
|[sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16][sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16]| 中文、英文 | [另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16-bilingual-chinese-english)|
|[sherpa-onnx-streaming-zipformer-zh-14M-2023-02-23][sherpa-onnx-streaming-zipformer-zh-14M-2023-02-23]|中文| 适合 Cortex A7 CPU。[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#sherpa-onnx-streaming-zipformer-zh-14m-2023-02-23)|
|[sherpa-onnx-streaming-zipformer-en-20M-2023-02-17][sherpa-onnx-streaming-zipformer-en-20M-2023-02-17]|英文|适合 Cortex A7 CPU。[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#sherpa-onnx-streaming-zipformer-en-20m-2023-02-17)|
|[sherpa-onnx-streaming-zipformer-korean-2024-06-16][sherpa-onnx-streaming-zipformer-korean-2024-06-16]|韩文| [另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#sherpa-onnx-streaming-zipformer-korean-2024-06-16-korean)|
|[sherpa-onnx-streaming-zipformer-fr-2023-04-14][sherpa-onnx-streaming-zipformer-fr-2023-04-14]|法文| [另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/online-transducer/zipformer-transducer-models.html#shaojieli-sherpa-onnx-streaming-zipformer-fr-2023-04-14-french)|

</details>


#### 部分预训练 ASR 模型（非流式）

<details>

更多模型请见

  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-transducer/index.html>
  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-paraformer/index.html>
  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-ctc/index.html>
  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/telespeech/index.html>
  - <https://k2-fsa.github.io/sherpa/onnx/pretrained_models/whisper/index.html>

下表仅列出其中**一部分**。

|名称 | 支持的语言 | 说明|
|-----|-----|----|
|[sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-transducer/nemo-transducer-models.html#sherpa-onnx-nemo-parakeet-tdt-0-6b-v2-int8-english)| 英文 | 转换自 <https://huggingface.co/nvidia/parakeet-tdt-0.6b-v2>|
|[Whisper tiny.en](https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.en.tar.bz2)|英文| [另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/whisper/tiny.en.html)|
|[Moonshine tiny][Moonshine tiny]|英文|[另见](https://github.com/usefulsensors/moonshine)|
|[sherpa-onnx-zipformer-ctc-zh-int8-2025-07-03](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-ctc/icefall/zipformer.html#sherpa-onnx-zipformer-ctc-zh-int8-2025-07-03-chinese)|中文| 一个 Zipformer CTC 模型|
|[sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17][sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17]|中文、粤语、英文、韩文、日文| 支持多种中文方言。[另见](https://k2-fsa.github.io/sherpa/onnx/sense-voice/index.html)|
|[sherpa-onnx-paraformer-zh-2024-03-09][sherpa-onnx-paraformer-zh-2024-03-09]|中文、英文| 也支持多种中文方言。[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-paraformer/paraformer-models.html#csukuangfj-sherpa-onnx-paraformer-zh-2024-03-09-chinese-english)|
|[sherpa-onnx-zipformer-ja-reazonspeech-2024-08-01][sherpa-onnx-zipformer-ja-reazonspeech-2024-08-01]|日文|[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-transducer/zipformer-transducer-models.html#sherpa-onnx-zipformer-ja-reazonspeech-2024-08-01-japanese)|
|[sherpa-onnx-nemo-transducer-giga-am-russian-2024-10-24][sherpa-onnx-nemo-transducer-giga-am-russian-2024-10-24]|俄文|[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-transducer/nemo-transducer-models.html#sherpa-onnx-nemo-transducer-giga-am-russian-2024-10-24-russian)|
|[sherpa-onnx-nemo-ctc-giga-am-russian-2024-10-24][sherpa-onnx-nemo-ctc-giga-am-russian-2024-10-24]|俄文| [另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-ctc/nemo/russian.html#sherpa-onnx-nemo-ctc-giga-am-russian-2024-10-24)|
|[sherpa-onnx-zipformer-ru-2024-09-18][sherpa-onnx-zipformer-ru-2024-09-18]|俄文|[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-transducer/zipformer-transducer-models.html#sherpa-onnx-zipformer-ru-2024-09-18-russian)|
|[sherpa-onnx-zipformer-korean-2024-06-24][sherpa-onnx-zipformer-korean-2024-06-24]|韩文|[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-transducer/zipformer-transducer-models.html#sherpa-onnx-zipformer-korean-2024-06-24-korean)|
|[sherpa-onnx-zipformer-thai-2024-06-20][sherpa-onnx-zipformer-thai-2024-06-20]|泰文| [另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/offline-transducer/zipformer-transducer-models.html#sherpa-onnx-zipformer-thai-2024-06-20-thai)|
|[sherpa-onnx-telespeech-ctc-int8-zh-2024-06-04][sherpa-onnx-telespeech-ctc-int8-zh-2024-06-04]|中文| 支持多种方言。[另见](https://k2-fsa.github.io/sherpa/onnx/pretrained_models/telespeech/models.html#sherpa-onnx-telespeech-ctc-int8-zh-2024-06-04)|

</details>

### 有用的链接

- 文档：https://k2-fsa.github.io/sherpa/onnx/
- Bilibili 演示视频：https://search.bilibili.com/all?keyword=%E6%96%B0%E4%B8%80%E4%BB%A3Kaldi

### 如何联系我们

请见
https://k2-fsa.github.io/sherpa/social-groups.html
获取新一代 Kaldi 的**微信交流群**和 **QQ 交流群**。

## 使用 sherpa-onnx 的项目

### [Sherpa Voice / @siteed/sherpa-onnx.rn](https://github.com/deeeed/audiolab)

> 一个 React Native 封装及演示应用，用于在 iOS、Android 和 Web 上验证 sherpa-onnx，
> 涵盖 ASR、TTS、VAD、KWS、说话人识别、说话人分离、语种识别、标点、音频事件标注
> 和语音增强。

- [NPM 包](https://www.npmjs.com/package/@siteed/sherpa-onnx.rn)
- [在线演示](https://deeeed.github.io/audiolab/sherpa-voice/)

### [Speed of Sound](https://github.com/zugaldia/speedofsound)

> 一个面向 Linux 桌面（GTK4/Adwaita）的语音输入应用。它采集麦克风音频，使用
> Sherpa ONNX ASR 模型离线转写，可选地用 LLM 润色文本，并通过 XDG Remote Desktop
> Portal 键盘模拟把结果输入到当前活动窗口。

### [VoxSherpa TTS](https://github.com/CodeBySonu95/VoxSherpa-TTS)

> VoxSherpa TTS 是一个 100% 离线的 Android 文字转语音应用，由 Sherpa-ONNX 驱动。
> 它支持 Kokoro-82M、Piper 和 VITS 引擎，多语种支持包括印地语、英语、英式英语、
> 日语、中文以及 50 多种语言。

- [下载 APK（所有版本）](https://github.com/CodeBySonu95/VoxSherpa-TTS/releases)
- Android 11+ · 100% 离线 · 无遥测

<div align="center">

| Generate | Models | Library | Settings |
|:---:|:---:|:---:|:---:|
| <img src="https://raw.githubusercontent.com/CodeBySonu95/VoxSherpa-TTS/main/fastlane/metadata/android/en-US/images/phoneScreenshots/1.jpg" width="180"/> | <img src="https://raw.githubusercontent.com/CodeBySonu95/VoxSherpa-TTS/main/fastlane/metadata/android/en-US/images/phoneScreenshots/2.jpg" width="180"/> | <img src="https://raw.githubusercontent.com/CodeBySonu95/VoxSherpa-TTS/main/fastlane/metadata/android/en-US/images/phoneScreenshots/3.jpg" width="180"/> | <img src="https://raw.githubusercontent.com/CodeBySonu95/VoxSherpa-TTS/main/fastlane/metadata/android/en-US/images/phoneScreenshots/4.jpg" width="180"/> |

</div>

---
### [BreezeApp](https://github.com/mtkresearch/BreezeApp)，来自 [MediaTek Research](https://github.com/mtkresearch)

> BreezeAPP 是一个同时面向 Android 和 iOS 平台的移动 AI 应用。用户可以直接从
> App Store 下载，并离线享受多种功能，包括语音转文字、文字转语音、基于文本的
> 聊天机器人交互，以及图像问答。

  - [下载 BreezeAPP 的 APK](https://huggingface.co/MediaTek-Research/BreezeApp/resolve/main/BreezeApp.apk)
  - [APK 中国镜像](https://hf-mirror.com/MediaTek-Research/BreezeApp/blob/main/BreezeApp.apk)

| 1 | 2 | 3 |
|---|---|---|
|![](https://github.com/user-attachments/assets/1cdbc057-b893-4de6-9e9c-f1d7dfd1d992)|![](https://github.com/user-attachments/assets/d77cd98e-b057-442f-860d-d5befd5c769b)|![](https://github.com/user-attachments/assets/57e546bf-3d39-45b9-b392-b48ca4fb3c58)|

### [Open-LLM-VTuber](https://github.com/t41372/Open-LLM-VTuber)

用免手操作的语音交互、语音打断以及 Live2D 说话人脸，与任意 LLM 对话，全程在本地、跨平台运行。

另见 <https://github.com/t41372/Open-LLM-VTuber/pull/50>

### [voiceapi](https://github.com/ruzhila/voiceapi)

<details>
  <summary>基于 FastAPI 的流式 ASR 与 TTS</summary>


它展示了如何在 FastAPI 中使用 ASR 和 TTS 的 Python API。
</details>

### [腾讯会议摸鱼工具 TMSpeech](https://github.com/jxlpzqc/TMSpeech)

使用 C# 的流式 ASR，带图形用户界面。

中文视频演示：[【开源】Windows实时字幕软件（网课/开会必备）](https://www.bilibili.com/video/BV1rX4y1p7Nx)

### [lol互动助手](https://github.com/l1veIn/lol-wom-electron)

它使用 sherpa-onnx 的 JavaScript API 配合 [Electron](https://electronjs.org/)。

中文视频演示：[爆了！炫神教你开打字挂！真正影响胜率的英雄联盟工具！英雄联盟的最后一块拼图！和游戏中的每个人无障碍沟通！](https://www.bilibili.com/video/BV142tje9E74)

### [Sherpa-ONNX 语音识别服务器](https://github.com/hfyydd/sherpa-onnx-server)

一个基于 nodejs 的服务器，提供用于语音识别的 Restful API。

### [QSmartAssistant](https://github.com/xinhecuican/QSmartAssistant)

一个模块化，全过程可离线，低占用率的对话机器人/智能音箱

它使用 QT。同时用到了 [ASR](https://github.com/xinhecuican/QSmartAssistant/blob/master/doc/%E5%AE%89%E8%A3%85.md#asr)
和 [TTS](https://github.com/xinhecuican/QSmartAssistant/blob/master/doc/%E5%AE%89%E8%A3%85.md#tts)。

### [Flutter-EasySpeechRecognition](https://github.com/Jason-chen-coder/Flutter-EasySpeechRecognition)

它在 [./flutter-examples/streaming_asr](./flutter-examples/streaming_asr) 基础上扩展，
在应用内下载模型以减小应用体积。

注意：[[Team B] Sherpa AI backend](https://github.com/umgc/spring2025/pull/82) 也在一个
Flutter 应用中使用了 sherpa-onnx。

### [sherpa-onnx-unity](https://github.com/xue-fei/sherpa-onnx-unity)

Unity 中的 sherpa-onnx。另见 [#1695](https://github.com/k2-fsa/sherpa-onnx/issues/1695)、
[#1892](https://github.com/k2-fsa/sherpa-onnx/issues/1892) 和 [#1859](https://github.com/k2-fsa/sherpa-onnx/issues/1859)

### [xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server)

本项目为xiaozhi-esp32提供后端服务，帮助您快速搭建ESP32设备控制服务器

另见

  - [ASR新增轻量级sherpa-onnx-asr](https://github.com/xinnan-tech/xiaozhi-esp32-server/issues/315)
  - [feat: ASR增加sherpa-onnx模型](https://github.com/xinnan-tech/xiaozhi-esp32-server/pull/379)

### [KaithemAutomation](https://github.com/EternityForest/KaithemAutomation)

纯 Python、以 GUI 为核心的家庭自动化/消费级 SCADA。

它使用 sherpa-onnx 的 TTS。另见 [✨ Speak command that uses the new globally configured TTS model.](https://github.com/EternityForest/KaithemAutomation/commit/8e64d2b138725e426532f7d66bb69dd0b4f53693)

### [Open-XiaoAI KWS](https://github.com/idootop/open-xiaoai-kws)

为小爱音箱启用自定义唤醒词。让小爱音箱支持自定义唤醒词。

中文视频演示：[小爱同学启动～˶╹ꇴ╹˶！](https://www.bilibili.com/video/BV1YfVUz5EMj)

### [C++ WebSocket ASR Server](https://github.com/mawwalker/stt-server)

它提供一个基于 C++ 的 WebSocket 服务器，使用 sherpa-onnx 进行 ASR。

### [Go WebSocket Server](https://github.com/bbeyondllove/asr_server)

它提供一个基于 Go 编程语言的 WebSocket 服务器，使用 sherpa-onnx。

### [Making robot Paimon, Ep10 "The AI Part 1"](https://www.youtube.com/watch?v=KxPKkwxGWZs)

这是一个 [YouTube 视频](https://www.youtube.com/watch?v=KxPKkwxGWZs)，
展示了作者如何尝试使用 AI，从而能与派蒙对话。

它使用 sherpa-onnx 进行语音转文字和文字转语音。
|1|
|---|
|![](https://github.com/user-attachments/assets/f6eea2d5-1807-42cb-9160-be8da2971e1f)|

### [TtsReader - Desktop application](https://github.com/ys-pro-duction/TtsReader)

一个用 Kotlin Multiplatform 构建的桌面文字转语音应用。

### [MentraOS](https://github.com/Mentra-Community/MentraOS)

> 智能眼镜操作系统，内置数十个应用。用户可获得 AI 助手、通知、翻译、屏幕镜像、
> 字幕等功能。开发者只需编写 1 个应用，即可在任意一副智能眼镜上运行。

它在 iOS 和 Android 设备上使用 sherpa-onnx 进行实时语音识别。
另见 <https://github.com/Mentra-Community/MentraOS/pull/861>

iOS 使用 Swift，Android 使用 Java。

### [flet_sherpa_onnx](https://github.com/SamYuan1990/flet_sherpa_onnx)

基于 sherpa-onnx 的 Flet ASR/STT 组件。
示例：[一个聊天框 agent](https://github.com/SamYuan1990/i18n-agent-action)

### [achatbot-go](https://github.com/ai-bot-pro/achatbot-go)

一个基于 go、使用 sherpa-onnx 语音库 API 的多模态聊天机器人。

### [fcitx5-vinput](https://github.com/xifan2333/fcitx5-vinput)

面向 [Fcitx5](https://github.com/fcitx/fcitx5)（Linux 输入法框架）的本地离线语音输入插件。
它使用 C++ 配合离线 ASR 进行语音识别，支持按键说话（push-to-talk）、命令模式，
以及可选的 LLM 后处理。

中文视频演示：[fcitx5-vinput](https://www.bilibili.com/video/BV1a6cUzVE6F)

### [Wake Word](https://github.com/analyticsinmotion/wake-word)

一个用于免手操作、语音激活编码的 VS Code 扩展。它使用 sherpa-onnx 进行实时关键词检测
（KWS），以识别自定义唤醒短语并通过语音触发 VS Code 命令。音频采集由
[decibri](https://github.com/analyticsinmotion/decibri) 处理——一个跨平台的 Node.js
麦克风流式库，带预编译的原生二进制。

- [VS Code Marketplace](https://marketplace.visualstudio.com/items?itemName=analytics-in-motion.wake-word)
- [Open VSX](https://open-vsx.org/extension/analytics-in-motion/wake-word)
- [decibri 对接 sherpa-onnx 指南](https://decibri.dev/docs/node/integrations/sherpa-onnx-stt.html)

### [SmartSub](https://github.com/buxuku/SmartSub)

> SmartSub 是一个本地优先、跨平台的桌面应用，覆盖完整的字幕制作流程：音视频转写、
> 字幕翻译、校对，以及字幕压制/混流。
>
> 它原生集成 sherpa-onnx 来驱动三种离线 ASR 引擎——FunASR、Qwen3-ASR 和 FireRedASR，
> 完全在设备端提供高精度的中文与多语种语音识别，无需上传任何文件。

[silero-vad]: https://github.com/snakers4/silero-vad
[Raspberry Pi]: https://www.raspberrypi.com/
[RV1126]: https://www.rock-chips.com/uploads/pdf/2022.8.26/191/RV1126%20Brief%20Datasheet.pdf
[LicheePi4A]: https://sipeed.com/licheepi4a
[VisionFive 2]: https://www.starfivetech.com/en/site/boards
[旭日X3派]: https://developer.horizon.ai/api/v1/fileData/documents_pi/index.html
[爱芯派]: https://wiki.sipeed.com/hardware/zh/maixIII/ax-pi/axpi.html
[hf-space-speaker-diarization]: https://huggingface.co/spaces/k2-fsa/speaker-diarization
[hf-space-speaker-diarization-cn]: https://hf.qhduan.com/spaces/k2-fsa/speaker-diarization
[hf-space-asr]: https://huggingface.co/spaces/k2-fsa/automatic-speech-recognition
[hf-space-asr-cn]: https://hf.qhduan.com/spaces/k2-fsa/automatic-speech-recognition
[Whisper]: https://github.com/openai/whisper
[hf-space-asr-whisper]: https://huggingface.co/spaces/k2-fsa/automatic-speech-recognition-with-whisper
[hf-space-asr-whisper-cn]: https://hf.qhduan.com/spaces/k2-fsa/automatic-speech-recognition-with-whisper
[hf-space-tts]: https://huggingface.co/spaces/k2-fsa/text-to-speech
[hf-space-tts-cn]: https://hf.qhduan.com/spaces/k2-fsa/text-to-speech
[hf-space-subtitle]: https://huggingface.co/spaces/k2-fsa/generate-subtitles-for-videos
[hf-space-subtitle-cn]: https://hf.qhduan.com/spaces/k2-fsa/generate-subtitles-for-videos
[hf-space-audio-tagging]: https://huggingface.co/spaces/k2-fsa/audio-tagging
[hf-space-audio-tagging-cn]: https://hf.qhduan.com/spaces/k2-fsa/audio-tagging
[hf-space-source-separation]: https://huggingface.co/spaces/k2-fsa/source-separation
[hf-space-source-separation-cn]: https://hf.qhduan.com/spaces/k2-fsa/source-separation
[hf-space-slid-whisper]: https://huggingface.co/spaces/k2-fsa/spoken-language-identification
[hf-space-slid-whisper-cn]: https://hf.qhduan.com/spaces/k2-fsa/spoken-language-identification
[wasm-hf-vad]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-sherpa-onnx
[wasm-ms-vad]: https://modelscope.cn/studios/csukuangfj/web-assembly-vad-sherpa-onnx
[wasm-hf-streaming-asr-zh-en-zipformer]: https://huggingface.co/spaces/k2-fsa/web-assembly-asr-sherpa-onnx-zh-en
[wasm-ms-streaming-asr-zh-en-zipformer]: https://modelscope.cn/studios/k2-fsa/web-assembly-asr-sherpa-onnx-zh-en
[wasm-hf-streaming-asr-zh-en-paraformer]: https://huggingface.co/spaces/k2-fsa/web-assembly-asr-sherpa-onnx-zh-en-paraformer
[wasm-ms-streaming-asr-zh-en-paraformer]: https://modelscope.cn/studios/k2-fsa/web-assembly-asr-sherpa-onnx-zh-en-paraformer
[Paraformer-large]: https://www.modelscope.cn/models/damo/speech_paraformer-large_asr_nat-zh-cn-16k-common-vocab8404-pytorch/summary
[wasm-hf-streaming-asr-zh-en-yue-paraformer]: https://huggingface.co/spaces/k2-fsa/web-assembly-asr-sherpa-onnx-zh-cantonese-en-paraformer
[wasm-ms-streaming-asr-zh-en-yue-paraformer]: https://modelscope.cn/studios/k2-fsa/web-assembly-asr-sherpa-onnx-zh-cantonese-en-paraformer
[wasm-hf-streaming-asr-en-zipformer]: https://huggingface.co/spaces/k2-fsa/web-assembly-asr-sherpa-onnx-en
[wasm-ms-streaming-asr-en-zipformer]: https://modelscope.cn/studios/k2-fsa/web-assembly-asr-sherpa-onnx-en
[SenseVoice]: https://github.com/FunAudioLLM/SenseVoice
[wasm-hf-vad-asr-zh-zipformer-ctc-07-03]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-zipformer-ctc
[wasm-ms-vad-asr-zh-zipformer-ctc-07-03]: https://modelscope.cn/studios/csukuangfj/web-assembly-vad-asr-sherpa-onnx-zh-zipformer-ctc/summary
[wasm-hf-vad-asr-zh-en-ko-ja-yue-sense-voice]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-en-ja-ko-cantonese-sense-voice
[wasm-ms-vad-asr-zh-en-ko-ja-yue-sense-voice]: https://www.modelscope.cn/studios/csukuangfj/web-assembly-vad-asr-sherpa-onnx-zh-en-jp-ko-cantonese-sense-voice
[wasm-hf-vad-asr-en-whisper-tiny-en]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-en-whisper-tiny
[wasm-ms-vad-asr-en-whisper-tiny-en]: https://www.modelscope.cn/studios/csukuangfj/web-assembly-vad-asr-sherpa-onnx-en-whisper-tiny
[wasm-hf-vad-asr-en-moonshine-tiny-en]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-en-moonshine-tiny
[wasm-ms-vad-asr-en-moonshine-tiny-en]: https://www.modelscope.cn/studios/csukuangfj/web-assembly-vad-asr-sherpa-onnx-en-moonshine-tiny
[wasm-hf-vad-asr-en-zipformer-gigaspeech]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-en-zipformer-gigaspeech
[wasm-ms-vad-asr-en-zipformer-gigaspeech]: https://www.modelscope.cn/studios/k2-fsa/web-assembly-vad-asr-sherpa-onnx-en-zipformer-gigaspeech
[wasm-hf-vad-asr-zh-zipformer-wenetspeech]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-zipformer-wenetspeech
[wasm-ms-vad-asr-zh-zipformer-wenetspeech]: https://www.modelscope.cn/studios/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-zipformer-wenetspeech
[reazonspeech]: https://research.reazon.jp/_static/reazonspeech_nlp2023.pdf
[wasm-hf-vad-asr-ja-zipformer-reazonspeech]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-ja-zipformer
[wasm-ms-vad-asr-ja-zipformer-reazonspeech]: https://www.modelscope.cn/studios/csukuangfj/web-assembly-vad-asr-sherpa-onnx-ja-zipformer
[gigaspeech2]: https://github.com/speechcolab/gigaspeech2
[wasm-hf-vad-asr-th-zipformer-gigaspeech2]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-th-zipformer
[wasm-ms-vad-asr-th-zipformer-gigaspeech2]: https://www.modelscope.cn/studios/csukuangfj/web-assembly-vad-asr-sherpa-onnx-th-zipformer
[telespeech-asr]: https://github.com/tele-ai/telespeech-asr
[wasm-hf-vad-asr-zh-telespeech]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-telespeech
[wasm-ms-vad-asr-zh-telespeech]: https://www.modelscope.cn/studios/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-telespeech
[wasm-hf-vad-asr-zh-en-paraformer-large]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-en-paraformer
[wasm-ms-vad-asr-zh-en-paraformer-large]: https://www.modelscope.cn/studios/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-en-paraformer
[wasm-hf-vad-asr-zh-en-paraformer-small]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-en-paraformer-small
[wasm-ms-vad-asr-zh-en-paraformer-small]: https://www.modelscope.cn/studios/k2-fsa/web-assembly-vad-asr-sherpa-onnx-zh-en-paraformer-small
[dolphin]: https://github.com/dataoceanai/dolphin
[wasm-ms-vad-asr-multi-lang-dolphin-base]: https://modelscope.cn/studios/csukuangfj/web-assembly-vad-asr-sherpa-onnx-multi-lang-dophin-ctc
[wasm-hf-vad-asr-multi-lang-dolphin-base]: https://huggingface.co/spaces/k2-fsa/web-assembly-vad-asr-sherpa-onnx-multi-lang-dophin-ctc

[wasm-hf-tts-matcha-zh-en]: https://huggingface.co/spaces/k2-fsa/web-assembly-zh-en-tts-matcha
[wasm-hf-tts-matcha-zh]: https://huggingface.co/spaces/k2-fsa/web-assembly-zh-tts-matcha
[wasm-ms-tts-matcha-zh-en]: https://modelscope.cn/studios/csukuangfj/web-assembly-zh-en-tts-matcha
[wasm-ms-tts-matcha-zh]: https://modelscope.cn/studios/csukuangfj/web-assembly-zh-tts-matcha
[wasm-hf-tts-matcha-en]: https://huggingface.co/spaces/k2-fsa/web-assembly-en-tts-matcha
[wasm-ms-tts-matcha-en]: https://modelscope.cn/studios/csukuangfj/web-assembly-en-tts-matcha
[wasm-hf-tts-piper-en]: https://huggingface.co/spaces/k2-fsa/web-assembly-tts-sherpa-onnx-en
[wasm-ms-tts-piper-en]: https://modelscope.cn/studios/k2-fsa/web-assembly-tts-sherpa-onnx-en
[wasm-hf-tts-piper-de]: https://huggingface.co/spaces/k2-fsa/web-assembly-tts-sherpa-onnx-de
[wasm-ms-tts-piper-de]: https://modelscope.cn/studios/k2-fsa/web-assembly-tts-sherpa-onnx-de
[wasm-hf-speaker-diarization]: https://huggingface.co/spaces/k2-fsa/web-assembly-speaker-diarization-sherpa-onnx
[wasm-ms-speaker-diarization]: https://www.modelscope.cn/studios/csukuangfj/web-assembly-speaker-diarization-sherpa-onnx
[wasm-hf-voice-cloning-zipvoice]: https://huggingface.co/spaces/k2-fsa/web-assembly-zh-en-tts-zipvoice
[wasm-ms-voice-cloning-zipvoice]: https://modelscope.cn/studios/csukuangfj/web-assembly-zh-en-tts-zipvoice
[wasm-hf-voice-cloning-pocket]: https://huggingface.co/spaces/k2-fsa/web-assembly-en-tts-pocket
[wasm-ms-voice-cloning-pocket]: https://modelscope.cn/studios/csukuangfj/web-assembly-en-tts-pocket
[apk-speaker-diarization]: https://k2-fsa.github.io/sherpa/onnx/speaker-diarization/apk.html
[apk-speaker-diarization-cn]: https://k2-fsa.github.io/sherpa/onnx/speaker-diarization/apk-cn.html
[apk-streaming-asr]: https://k2-fsa.github.io/sherpa/onnx/android/apk.html
[apk-streaming-asr-cn]: https://k2-fsa.github.io/sherpa/onnx/android/apk-cn.html
[apk-simula-streaming-asr]: https://k2-fsa.github.io/sherpa/onnx/android/apk-simulate-streaming-asr.html
[apk-simula-streaming-asr-cn]: https://k2-fsa.github.io/sherpa/onnx/android/apk-simulate-streaming-asr-cn.html
[apk-tts]: https://k2-fsa.github.io/sherpa/onnx/tts/apk-engine.html
[apk-tts-cn]: https://k2-fsa.github.io/sherpa/onnx/tts/apk-engine-cn.html
[apk-vad]: https://k2-fsa.github.io/sherpa/onnx/vad/apk.html
[apk-vad-cn]: https://k2-fsa.github.io/sherpa/onnx/vad/apk-cn.html
[apk-vad-asr]: https://k2-fsa.github.io/sherpa/onnx/vad/apk-asr.html
[apk-vad-asr-cn]: https://k2-fsa.github.io/sherpa/onnx/vad/apk-asr-cn.html
[apk-2pass]: https://k2-fsa.github.io/sherpa/onnx/android/apk-2pass.html
[apk-2pass-cn]: https://k2-fsa.github.io/sherpa/onnx/android/apk-2pass-cn.html
[apk-at]: https://k2-fsa.github.io/sherpa/onnx/audio-tagging/apk.html
[apk-at-cn]: https://k2-fsa.github.io/sherpa/onnx/audio-tagging/apk-cn.html
[apk-at-wearos]: https://k2-fsa.github.io/sherpa/onnx/audio-tagging/apk-wearos.html
[apk-at-wearos-cn]: https://k2-fsa.github.io/sherpa/onnx/audio-tagging/apk-wearos-cn.html
[apk-sid]: https://k2-fsa.github.io/sherpa/onnx/speaker-identification/apk.html
[apk-sid-cn]: https://k2-fsa.github.io/sherpa/onnx/speaker-identification/apk-cn.html
[apk-slid]: https://k2-fsa.github.io/sherpa/onnx/spoken-language-identification/apk.html
[apk-slid-cn]: https://k2-fsa.github.io/sherpa/onnx/spoken-language-identification/apk-cn.html
[apk-kws]: https://k2-fsa.github.io/sherpa/onnx/kws/apk.html
[apk-kws-cn]: https://k2-fsa.github.io/sherpa/onnx/kws/apk-cn.html
[apk-flutter-streaming-asr]: https://k2-fsa.github.io/sherpa/onnx/flutter/pre-built-app.html#streaming-speech-recognition-stt-asr
[apk-flutter-streaming-asr-cn]: https://k2-fsa.github.io/sherpa/onnx/flutter/pre-built-app.html#streaming-speech-recognition-stt-asr
[flutter-tts-android]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-android.html
[flutter-tts-android-cn]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-android-cn.html
[flutter-tts-linux]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-linux.html
[flutter-tts-linux-cn]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-linux-cn.html
[flutter-tts-macos-x64]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-macos-x64.html
[flutter-tts-macos-arm64-cn]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-macos-arm64-cn.html
[flutter-tts-macos-arm64]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-macos-arm64.html
[flutter-tts-macos-x64-cn]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-macos-x64-cn.html
[flutter-tts-win-x64]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-win.html
[flutter-tts-win-x64-cn]: https://k2-fsa.github.io/sherpa/onnx/flutter/tts-win-cn.html
[lazarus-subtitle]: https://k2-fsa.github.io/sherpa/onnx/lazarus/download-generated-subtitles.html
[lazarus-subtitle-cn]: https://k2-fsa.github.io/sherpa/onnx/lazarus/download-generated-subtitles-cn.html
[asr-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/asr-models
[tts-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/tts-models
[vad-models]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx
[kws-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/kws-models
[at-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/audio-tagging-models
[sid-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/speaker-recongition-models
[slid-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/speaker-recongition-models
[punct-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/punctuation-models
[speaker-segmentation-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/speaker-segmentation-models
[GigaSpeech]: https://github.com/SpeechColab/GigaSpeech
[WenetSpeech]: https://github.com/wenet-e2e/WenetSpeech
[sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20.tar.bz2
[sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16.tar.bz2
[sherpa-onnx-streaming-zipformer-korean-2024-06-16]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-korean-2024-06-16.tar.bz2
[sherpa-onnx-streaming-zipformer-zh-14M-2023-02-23]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-zh-14M-2023-02-23.tar.bz2
[sherpa-onnx-streaming-zipformer-en-20M-2023-02-17]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-en-20M-2023-02-17.tar.bz2
[sherpa-onnx-zipformer-ja-reazonspeech-2024-08-01]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-zipformer-ja-reazonspeech-2024-08-01.tar.bz2
[sherpa-onnx-zipformer-ru-2024-09-18]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-zipformer-ru-2024-09-18.tar.bz2
[sherpa-onnx-zipformer-korean-2024-06-24]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-zipformer-korean-2024-06-24.tar.bz2
[sherpa-onnx-zipformer-thai-2024-06-20]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-zipformer-thai-2024-06-20.tar.bz2
[sherpa-onnx-nemo-transducer-giga-am-russian-2024-10-24]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-nemo-transducer-giga-am-russian-2024-10-24.tar.bz2
[sherpa-onnx-paraformer-zh-2024-03-09]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-paraformer-zh-2024-03-09.tar.bz2
[sherpa-onnx-nemo-ctc-giga-am-russian-2024-10-24]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-nemo-ctc-giga-am-russian-2024-10-24.tar.bz2
[sherpa-onnx-telespeech-ctc-int8-zh-2024-06-04]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-telespeech-ctc-int8-zh-2024-06-04.tar.bz2
[sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
[sherpa-onnx-streaming-zipformer-fr-2023-04-14]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-fr-2023-04-14.tar.bz2
[Moonshine tiny]: https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-moonshine-tiny-en-int8.tar.bz2
[NVIDIA Jetson Orin NX]: https://developer.download.nvidia.com/assets/embedded/secure/jetson/orin_nx/docs/Jetson_Orin_NX_DS-10712-001_v0.5.pdf?RCPGu9Q6OVAOv7a7vgtwc9-BLScXRIWq6cSLuditMALECJ_dOj27DgnqAPGVnT2VpiNpQan9SyFy-9zRykR58CokzbXwjSA7Gj819e91AXPrWkGZR3oS1VLxiDEpJa_Y0lr7UT-N4GnXtb8NlUkP4GkCkkF_FQivGPrAucCUywL481GH_WpP_p7ziHU1Wg==&t=eyJscyI6ImdzZW8iLCJsc2QiOiJodHRwczovL3d3dy5nb29nbGUuY29tLmhrLyJ9
[NVIDIA Jetson Nano B01]: https://www.seeedstudio.com/blog/2020/01/16/new-revision-of-jetson-nano-dev-kit-now-supports-new-jetson-nano-module/
[speech-enhancement-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/speech-enhancement-models
[source-separation-models]: https://github.com/k2-fsa/sherpa-onnx/releases/tag/source-separation-models
[RK3588]: https://www.rock-chips.com/uploads/pdf/2022.8.26/192/RK3588%20Brief%20Datasheet.pdf
[spleeter]: https://github.com/deezer/spleeter
[UVR]: https://github.com/Anjok07/ultimatevocalremovergui
[gtcrn]: https://github.com/Xiaobin-Rong/gtcrn
[tts-url]: https://k2-fsa.github.io/sherpa/onnx/tts/all-in-one.html
[ss-url]: https://k2-fsa.github.io/sherpa/onnx/source-separation/index.html
[sd-url]: https://k2-fsa.github.io/sherpa/onnx/speaker-diarization/index.html
[slid-url]: https://k2-fsa.github.io/sherpa/onnx/spoken-language-identification/index.html
[at-url]: https://k2-fsa.github.io/sherpa/onnx/audio-tagging/index.html
[vad-url]: https://k2-fsa.github.io/sherpa/onnx/vad/index.html
[kws-url]: https://k2-fsa.github.io/sherpa/onnx/kws/index.html
[punct-url]: https://k2-fsa.github.io/sherpa/onnx/punctuation/index.html
[se-url]: https://k2-fsa.github.io/sherpa/onnx/speech-enhancement/index.html
[rknpu-doc]: https://k2-fsa.github.io/sherpa/onnx/rknn/index.html
[qnn-doc]: https://k2-fsa.github.io/sherpa/onnx/qnn/index.html
[ascend-doc]: https://k2-fsa.github.io/sherpa/onnx/ascend/index.html
[axera-npu]: https://axera-tech.com/Skill/166.html
[SpacemiT-K1]: https://cdn-resource.spacemit.com/file/chip/K1/K1_brief_zh.pdf
[SpacemiT-K3]: https://cdn-resource.spacemit.com/file/chip/K3/K3_brief_zh.pdf
