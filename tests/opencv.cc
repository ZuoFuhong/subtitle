#include <iostream>
#include <opencv2/opencv.hpp>

// 图像的梯度来检测边缘
int gradient() {
    cv::Mat src = cv::imread("../resources/image/opencv_logo.jpg", cv::IMREAD_GRAYSCALE);
    if (src.empty()) {
        std::cout << "Could not read the image" << std::endl;
        return 1;
    }
    cv::Mat edges;
    cv::Canny(src, edges, 50, 150);
    cv::imshow("Edges", edges);

    cv::waitKey(0);
    return 0;
}

// 图像滤波
int blur() {
    cv::Mat src = cv::imread("../resources/image/plane.jpg");
    if (src.empty()) {
        std::cout << "Could not read the image" << std::endl;
        return 1;
    }
    cv::imshow("Original Image", src);

    cv::Mat blurred;
    cv::GaussianBlur(src, blurred, cv::Size(5, 5), 1.5, 1.5);
    cv::imshow("Blurred Image", blurred);

    cv::waitKey(0);
    return 0;
}

int main() {
    cv::Mat src = cv::imread("../resources/image/opencv_logo.jpg");
    if (src.empty()) {
        std::cout << "Could not read the image" << std::endl;
        return 1;
    }
    std::cout << "image width: " << src.cols << " height: " << src.rows << " channels: " << src.channels() << std::endl;

    // 显示图像
    cv::imshow("Original Image", src);

    // 分离图像通道
    std::vector<cv::Mat> channels;
    cv::split(src, channels);
    // 显示蓝色通道
    cv::imshow("Channel 1 Image", channels[1]);

    // 将图像从 RGB 颜色空间转换成灰度颜色空间
    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_RGB2GRAY);
    cv::imshow("Gray Image", gray);

    cv::waitKey(0);
    return 0;
}


