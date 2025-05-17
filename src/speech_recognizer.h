#pragma once

#include <string>

class SpeechRecognizer {
public:
    virtual ~SpeechRecognizer() = default;

    virtual std::pair<std::string, bool> recognize_text(const float* data_chunk, unsigned int data_chunk_nlen) = 0;
};
