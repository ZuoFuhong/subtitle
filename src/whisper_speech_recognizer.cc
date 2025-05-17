#include "whisper_speech_recognizer.h"
#include <string>
#include <thread>
#include <utility>
#include "utils.h"

static void cb_log_disable(enum ggml_log_level , const char * , void * ) { }

WhisperSpeechRecognizer::WhisperSpeechRecognizer(std::string_view model_path) {
    whisper_log_set(cb_log_disable, nullptr);
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu    = true;
    cparams.flash_attn = false;
    whisper_ctx = whisper_init_from_file_with_params(model_path.data(), cparams);
}

WhisperSpeechRecognizer::~WhisperSpeechRecognizer() {
    whisper_free(whisper_ctx);
}

bool sentence_has_finished(std::string_view text) {
    if (!text.empty() && !std::ispunct(text.back())) { // 非标点符号
        return false;
    }
    std::array<std::string_view, 4> suffixes = {"--", "--.", "...", ","}; // 半句话后缀
    return !std::any_of(suffixes.begin(), suffixes.end(), [text](std::string_view suffix) {
        return utils::ends_with(text, suffix);
    });
}

std::pair<std::string, bool> WhisperSpeechRecognizer::recognize_text(const float* data_chunk, unsigned int data_chunk_nlen) {
    int32_t n_threads = std::min(4, (int32_t) std::thread::hardware_concurrency());
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_timestamps = false;
    wparams.print_realtime   = false;
    wparams.translate        = false;
    wparams.single_segment   = true;
    wparams.max_tokens       = 0;
    wparams.language         = "en";
    wparams.n_threads        = n_threads;
    if (whisper_full(whisper_ctx, wparams, data_chunk, int(data_chunk_nlen)) != 0) {
        exit(EXIT_FAILURE);
    }
    const int n_segments = whisper_full_n_segments(whisper_ctx);
    std::string text;
    if (n_segments > 0) {
        text = utils::trim(whisper_full_get_segment_text(whisper_ctx, 0));
    }
    return std::make_pair(text, sentence_has_finished(text));
}
