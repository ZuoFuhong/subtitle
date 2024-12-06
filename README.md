## 实时字幕

开源轻量的 macOS 实时字幕应用程序，在收听播客或观看视频时提供高质量的双语流式字幕。语音转写使用服务端 ASR 能力，翻译字幕使用 OpenAI gpt-3.5-turbo 模型接口。

![subtitile_preview](./docs/subtitle_youtube.png)

安装运行时依赖：

```shell
# 虚拟声卡
brew install --cask blackhole-2ch

# 外部依赖
brew install sdl2 spdlog boost
```

终端窗口全屏展示效果最佳：

```shell
# 仅支持 OpenAI API_KEY（可选）
export OPENAI_API_KEY=sk-xxxxx

# 指定 ASR 服务地址
./main -s 127.0.0.1:8000
```

### 2、语音活动检测

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

下面是 Silero-vad ONNX 检测出的语音片段：

```shell
# 音频帧位置
start: 5120,   end: 37376
start: 53760,  end: 60416
start: 66560,  end: 127488
start: 131072, end: 169472

# 时间刻度
start_time: 320ms,  end_time: 2336ms
start_time: 3360ms, end_time: 3776ms
start_time: 4160ms, end_time: 7968ms
start_time: 8192ms, end_time: 10592ms
```
