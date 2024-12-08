#include <vector>
#include <iostream>
#include "../third_party/wav.h"
#include "../src/silero_vad.h"

int main() {
    // 归一化预处理
    wav::WavReader wav_reader{};
    if (!wav_reader.open_file("../resources/audio/jfk.wav")) { // 16000, 1, s16
        exit(EXIT_FAILURE);
    }
    std::vector<float> input_wav(wav_reader.num_samples());
    std::vector<float> output_wav;
    for (int i = 0; i < wav_reader.num_samples(); i++) {
        input_wav[i] = static_cast<float>(*(wav_reader.data() + i));
    }

    // 模型推理
    VadSession session( "../resources/model/silero_vad.onnx");
    session.process(input_wav);
    std::vector<Speech> stamps = session.get_speeches();
    for (auto & stamp : stamps) {
        std::cout << "start_time: " << stamp.start / 16 << "ms, end_time: " << stamp.end / 16 << "ms" << std::endl;
    }
    return 0;
}