## 实时字幕

开源轻量的 macOS 实时字幕应用程序，在收听播客或观看视频时提供高质量的双语流式字幕。使用 Silero-Vad + Whisper 实现自动语音识别（ASR），翻译字幕使用 DeepSeek-V3 模型 API。

![subtitile_preview](./docs/subtitle_youtube.png)

安装运行时依赖：

```shell
# 音频路由
brew install --cask loopback

# 禁用纳米区域分配器
export MallocNanoZone=0
```

终端窗口全屏展示效果最佳：

```shell
# DeepSeek API_KEY（可选）
export DEEPSEEK_API_KEY=sk-xxxxx

# 离线模式
./main -m offline

# 服务端 ASR
./main -m server -s 9.135.97.184 8000
```

### 2、语音转文本

[Whisper](https://github.com/openai/whisper) 提供了多种大小的模型供选择, 包括 small、medium 和 large 等, 不同大小的模型在精度、速度和计算资源占用方面有所差异.

```shell
# 模型下载
# small.en 仅支持英文, 速度较快, 适合实时语音识别
curl -L --output ggml-small.en.bin https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small.en.bin

# medium.en 仅支持英文, 速度中等, 识别精度极佳，推荐转换字幕
curl -L --output ggml-medium.en.bin https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin
```

注意: Whisper 模型在大段静音、背景音乐或显著噪音的英文音频下存在严重的幻觉问题.

下面是使用 [whisper.cpp](https://github.com/ggerganov/whisper.cpp) 离线跑模型实现语音识别 ASR 服务

```text
whisper_init_with_params_no_state: use gpu    = 1
whisper_init_with_params_no_state: flash attn = 0
whisper_init_with_params_no_state: gpu_device = 0
whisper_init_with_params_no_state: dtw        = 0
whisper_init_with_params_no_state: backends   = 3
whisper_backend_init_gpu: using Metal backend
ggml_metal_init: allocating
ggml_metal_init: found device: Apple M1 Pro
ggml_metal_init: picking default device: Apple M1 Pro
ggml_metal_init: using embedded metal library
ggml_metal_init: GPU name:   Apple M1 Pro
ggml_metal_init: GPU family: MTLGPUFamilyApple7  (1007)
ggml_metal_init: GPU family: MTLGPUFamilyCommon3 (3003)
ggml_metal_init: GPU family: MTLGPUFamilyMetal3  (5001)

And so my fellow Americans, ask not what your country can do for you, ask what you can do for your country.
```

### 3、语音活动检测

音频活动检测（Voice Activity Detection，VAD）通过分析音频信号的特征来判断是否存在语音活动, 能够有效的识别和分离语音信号和非语音信号.
在这里通过 VAD 将语音部分切断提供给 whisper 做转写, 可以大幅度降低了 whisper 的幻觉, 从而提高语音处理系统的效率和准确性.

![doc](docs/jfk_waveform.png)

下面是商用模型离线跑出的结果:

```json
{
	"sentences": [
		{"seId":"1", "seTime":1980, "sourceText":"AND SO MY FELLOW AMERICANS", "startTime":230, "endTime":2210},
		{"seId":"2", "seTime":900, "sourceText":"ASK NOT", "startTime":3290, "endTime":4190},
		{"seId":"3", "seTime":2020, "sourceText":"WHAT YOUR COUNTRY CAN DO FOR YOU", "startTime":5290, "endTime":7310},
		{"seId":"4", "seTime":2300, "sourceText":"ASK WHAT YOU CAN DO FOR YOUR COUNTRY", "startTime":8150, "endTime":10450}
	]
}
```

使用 ffmpeg 切割音频片段播放验证:

```shell
ffmpeg -i jfk.wav -ss 00:00:00.230 -to 00:00:02.210 -acodec copy output1.wav
ffmpeg -i jfk.wav -ss 00:00:03.290 -to 00:00:04.190 -acodec copy output2.wav
ffmpeg -i jfk.wav -ss 00:00:05.290 -to 00:00:07.310 -acodec copy output3.wav
ffmpeg -i jfk.wav -ss 00:00:08.150 -to 00:00:10.450 -acodec copy output4.wav
```

下面是 Silero-Vad + Whisper 流式识别的片段：

```shell
{"end":37376,"se_id":1,"start":5120,"text":"And so, my fellow Americans."}
{"end":86016,"se_id":2,"start":53760,"text":"Ask not."}
{"end":129024,"se_id":3,"start":87552,"text":"what your country can do for you."}
{"end":171008,"se_id":4,"start":131072,"text":"Ask what you can do for your country."}
```
