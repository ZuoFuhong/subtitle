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