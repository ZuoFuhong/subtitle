#include <iostream>
#include <thread>
#include <whisper.h>
#include "../third_party/wav.h"
#include "../src/utils.h"

// Code lifted from https://github.com/ggerganov/whisper.cpp/blob/v1.7.2/examples/stream/stream.cpp

static void cb_log_disable(enum ggml_log_level , const char * , void * ) { }

int main() {
    whisper_log_set(cb_log_disable, nullptr);

    // 归一化预处理
    wav::WavReader wav_reader{};
    if (!wav_reader.open_file("../resources/audio/jfk.wav")) { // 16000, 1, s16
        exit(EXIT_FAILURE);
    }
    std::vector<float> pcmf32(wav_reader.num_samples());
    for (int i = 0; i < wav_reader.num_samples(); i++) {
        pcmf32[i] = static_cast<float>(*(wav_reader.data() + i));
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu    = true;
    cparams.flash_attn = false;
    whisper_context* ctx = whisper_init_from_file_with_params("../resources/model/ggml-small.en.bin", cparams);

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

    // Run the inference
    if (whisper_full(ctx, wparams, pcmf32.data(), static_cast<int>(pcmf32.size())) != 0) {
        exit(EXIT_FAILURE);
    }
    const int n_segments = whisper_full_n_segments(ctx);
    if (n_segments > 0) {
        const char* text = whisper_full_get_segment_text(ctx, 0);
        std::cout << utils::trim(text) << std::endl;
    }
    whisper_print_timings(ctx);
    whisper_free(ctx);
    return 0;
}
