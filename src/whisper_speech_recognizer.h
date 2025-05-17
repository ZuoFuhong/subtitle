#pragma once

#include <string_view>
#include <whisper.h>
#include "speech_recognizer.h"

class WhisperSpeechRecognizer: public SpeechRecognizer {
public:
    explicit WhisperSpeechRecognizer(std::string_view model_path);

    ~WhisperSpeechRecognizer();

    std::pair<std::string, bool> recognize_text(const float* data_chunk, unsigned int data_chunk_nlen) override;

private:
    whisper_context* whisper_ctx;
};
