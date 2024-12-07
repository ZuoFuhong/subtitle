#include <iostream>
#include <vector>
#include <thread>
#include <whisper.h>
#include "../src/wav.h"

int main() {
    // 归一化预处理
    wav::WavReader wav_reader{};
    if (!wav_reader.open_file("../resources/audio/jfk.wav")) { // 16000, 1, s16
        exit(EXIT_FAILURE);
    }
    std::vector<float> pcmf32(wav_reader.num_samples());
    for (int i = 0; i < wav_reader.num_samples(); i++) {
        pcmf32[i] = static_cast<float>(*(wav_reader.data() + i));
    }

    // 模型推理
    std::string model_path = "../resources/model/ggml-small.en.bin";
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;
    struct whisper_context * ctx = whisper_init_from_file_with_params(model_path.c_str(), cparams);
    if (ctx == nullptr) {
        std::cerr << "initialize whisper context failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    whisper_ctx_init_openvino_encoder(ctx, nullptr, "CPU", nullptr);
    int32_t n_threads     = std::min(4, (int32_t) std::thread::hardware_concurrency());
    int32_t best_of       = whisper_full_default_params(WHISPER_SAMPLING_GREEDY).greedy.best_of;
    int32_t beam_size = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH).beam_search.beam_size;
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.strategy = beam_size > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    wparams.print_realtime   = false;
    wparams.print_progress   = false;
    wparams.print_timestamps = true;
    wparams.print_special    = false;
    wparams.translate        = false;
    wparams.language         = "en";
    wparams.detect_language  = false;
    wparams.n_threads        = n_threads;
    wparams.n_max_text_ctx   = wparams.n_max_text_ctx;
    wparams.offset_ms        = 0;
    wparams.duration_ms      = 0;
    wparams.token_timestamps = false;
    wparams.thold_pt         = 0.01f;
    wparams.max_len          = 0;
    wparams.split_on_word    = false;
    wparams.audio_ctx        = 0;
    wparams.debug_mode       = false;
    wparams.tdrz_enable      = false;
    wparams.suppress_regex   = nullptr;
    wparams.initial_prompt   = "";
    wparams.greedy.best_of   = best_of;
    wparams.beam_search.beam_size = beam_size;
    wparams.temperature_inc  = 0;
    wparams.entropy_thold    = 2.40f;
    wparams.logprob_thold    = -1.00f;
    wparams.no_timestamps    = false;
    if (whisper_full_parallel(ctx, wparams, pcmf32.data(), static_cast<int>(pcmf32.size()), 1) != 0) {
        std::cerr << "process audio failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        std::cout << whisper_full_get_segment_text(ctx, i) << std::endl;
    }
    whisper_print_timings(ctx);
    whisper_free(ctx);
    return 0;
}
