#include <iostream>
#include <fmt/format.h>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>

// 图像预处理
std::vector<float> preprocess_image(const cv::Mat& image) {
    cv::Mat resized_image;
    cv::resize(image, resized_image, cv::Size(28, 28));
    resized_image.convertTo(resized_image, CV_32F, 1.0 / 255);
    std::vector<float> input_data(resized_image.total());
    std::memcpy(input_data.data(), resized_image.data, resized_image.total() * sizeof(float ));
    return input_data;
}

int predict(const std::shared_ptr<Ort::Session>& session, std::string_view image_path) {
    // 创建输入张量
    cv::Mat image = cv::imread(image_path.data(), cv::IMREAD_GRAYSCALE);
    std::vector<float> input_data = preprocess_image(image);
    std::vector<int64_t> input_shape = {1, 1, 28, 28};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeCPU);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, input_data.data(), input_data.size(), input_shape.data(), input_shape.size());

    std::vector<const char *> input_node_names = {"input"};
    std::vector<Ort::Value> ort_inputs;
    ort_inputs.emplace_back(std::move(input_tensor));

    // 创建输出张量
    std::vector<Ort::Value> ort_outputs;
    std::vector<const char *> output_node_names = {"output"};

    // 运行推理
    ort_outputs = session->Run(Ort::RunOptions{nullptr},
                              input_node_names.data(), ort_inputs.data(), ort_inputs.size(),
                              output_node_names.data(), output_node_names.size());

    auto output_data = ort_outputs[0].GetTensorMutableData<float>();
    return static_cast<int>(std::distance(output_data, std::max_element(output_data, output_data + 10)));
}

// C++ ONNXRuntime 加载 MNIST 模型进行推理识别手写数字
int main() {
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ONNXRuntime");
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    std::shared_ptr<Ort::Session> session = std::make_shared<Ort::Session>(env, "../resources/model/cnn_mnist.onnx", session_options);

    std::vector<std::string_view> numbers = {"eight", "four", "two", "zero", "seven"};
    for (const auto& number : numbers) {
        int predicted = predict(session, fmt::format("../resources/image/mnist_{}.png", number));
        std::cout << "predict " << number << " => " << predicted << std::endl;
    }
}
