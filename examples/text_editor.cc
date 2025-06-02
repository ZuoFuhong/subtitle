// Copyright (c) 2025 Mars Zuo
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utf8cpp/utf8.h>
#include <spdlog/spdlog.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../third_party/clipp.h"

const int SCREEN_WIDTH = 800;

const int SCREEN_HEIGHT = 300;

const int TEXT_SIZE = 48;

const int TEXT_PADDING = 12;

// Display scale factor, for high DPI displays  
const int DISPLAY_SCALE = 2;

// Scroll step size, based on text size and padding
const int SCROLL_STEP = TEXT_SIZE + TEXT_PADDING; 

std::string get_last_utf8_char(const std::string& str) {
    if (str.empty()) {
        return "";
    }
    auto it = str.begin();
    auto end = str.end();
    auto last = it;
    while (it != end) {
        last = it;
        utf8::next(it, end);
    }
    return {last, end};
}

std::string get_first_utf8_char(const std::string& str) {
    if (str.empty()) return "";
    auto it = str.begin();
    auto end = str.end();
    auto next = it;
    utf8::next(next, end);
    return {it, next};
}

std::string utf8_substr(const std::string& str, size_t bytes_size) {
    if (str.empty() || bytes_size <= 0) {
        return "";
    }
    if (str.size() <= bytes_size) {
        return str;
    }
    auto it = str.begin();
    auto end = str.end();
    while (it != end) {
        utf8::next(it, end);
        if (auto size = std::distance(str.begin(), it); size_t(size) >= bytes_size) {
            break;
        }
    }
    return {str.begin(), it};
}

struct Textarea {
    TTF_Text* text;
    float x;
    float y;
    int w;
    int h;
    std::string content;
    int line_number;
    std::string lang;

    Textarea() {
        text = nullptr;
        x = 0;
        y = 0;
        w = 0;
        h = 0;
        content = "";
        line_number = 0;
        lang = "zh";
    }
};

struct Cursor {
private:
    bool  _show;
    float x;
    float y;
    float w;
    float h;
    int focus_line;
    int offset_x;
    float scroll_offset_y;
    uint64_t blink_timer;
    uint64_t blink_interval;

public:
    Cursor() {
        _show = false;
        x = 0;
        y = 0;
        w = 4.0f;
        h = 48.0f;
        focus_line = 0;
        offset_x = 0;
        scroll_offset_y = 0;
        blink_timer = 0;
        blink_interval = 500;
    }

    bool on_focus(TTF_Font* font, std::vector<Textarea>& text_lines_rendered, float mouse_x, float mouse_y) {
        for (auto & textarea : text_lines_rendered) {
            if (mouse_x >= textarea.x && mouse_x <= (textarea.x + float(textarea.w)) &&
                mouse_y >= textarea.y - scroll_offset_y && mouse_y <= (textarea.y - scroll_offset_y + float(textarea.h))) {
                focus_line = textarea.line_number;

                Textarea& textarea = text_lines_rendered[focus_line - 1];
                int relative_x = static_cast<int>(mouse_x);
                int relative_y = static_cast<int>(mouse_y);
                TTF_SubString substring;
                if (!TTF_GetTextSubStringForPoint(textarea.text, relative_x, relative_y, &substring)) {
                    std::cerr << "Failed to get text substring for point." << std::endl;
                    exit(EXIT_FAILURE);
                }
                int substr_w = 0;
                TTF_GetStringSize(font, textarea.content.c_str(), substring.offset, &substr_w, nullptr);
                x = textarea.x + static_cast<float>(substr_w);
                y = textarea.y;
                offset_x = substring.offset;
                _show = true;
                blink_timer = SDL_GetTicks();
                break;
            }
        }
        return focus_line > 0;
    }

    void on_blur() {
        _show = false;
        x = 0;
        y = 0;
        offset_x = 0;
        focus_line = 0;
    }

    size_t left(TTF_Font* font, Textarea& textarea) {
        std::string substr = textarea.content.substr(0, offset_x);
        std::string uc = get_last_utf8_char(substr);
        if (uc.size() > 0) {
            int substr_w = 0;
            TTF_GetStringSize(font, uc.c_str(), uc.length(), &substr_w, nullptr);
            x -= static_cast<float>(substr_w);
            offset_x -= static_cast<int>(uc.length());
        }
        return uc.size();
    }

    void right(TTF_Font* font, Textarea& textarea) {
        std::string substr = textarea.content.substr(offset_x, textarea.content.length());
        if (std::string uc = get_first_utf8_char(substr); uc.size() > 0) {
            int substr_w = 0;
            TTF_GetStringSize(font, uc.c_str(), uc.length(), &substr_w, nullptr);
            x += static_cast<float>(substr_w);
            offset_x += static_cast<int>(uc.length());
        }
    }

    void right(int text_w, size_t text_length) {
        x += static_cast<float>(text_w);
        offset_x += static_cast<int>(text_length);
    }

    void up(TTF_Font* font, std::vector<Textarea>& text_lines_rendered) {
        if (scroll_offset_y >= SCROLL_STEP && y - scroll_offset_y == TEXT_PADDING) {
            scroll_offset_y -= SCROLL_STEP;
        }
        if (focus_line > 1) {
            focus_line -= 1;
            vertical_move(font, text_lines_rendered[focus_line - 1]);
        }
    }

    void down(TTF_Font* font, std::vector<Textarea>& text_lines_rendered) {
        if (focus_line < static_cast<int>(text_lines_rendered.size())) {
            focus_line += 1;
            vertical_move(font, text_lines_rendered[focus_line - 1]);
        }
        if (y - scroll_offset_y > SCREEN_HEIGHT * DISPLAY_SCALE && text_lines_rendered.back().y - scroll_offset_y > SCREEN_HEIGHT * DISPLAY_SCALE) {
            scroll_offset_y += SCROLL_STEP;
        }
    }

    void vertical_move(TTF_Font* font, Textarea& textarea) {
        std::string substr = utf8_substr(textarea.content, offset_x);
        int substr_w = 0;
        TTF_GetStringSize(font, substr.c_str(), substr.length(), &substr_w, nullptr);

        y = textarea.y;
        x = textarea.x + static_cast<float>(substr_w);
        offset_x = static_cast<int>(substr.length());
    }

    void draw(SDL_Renderer *renderer) {
        if (!_show) {
            return;
        }
        uint64_t current_time = SDL_GetTicks();
        bool cursor_visible = ((current_time - blink_timer) % (blink_interval * 2)) < blink_interval;
        if (cursor_visible) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            float draw_y = y - scroll_offset_y;
            SDL_FRect cursor_rect = {x,draw_y,w,h};
            SDL_RenderFillRect(renderer, &cursor_rect);
        } 
    }

    void refresh_blink_timer() {
        blink_timer = SDL_GetTicks();
    }

    [[nodiscard]] bool show() const {
        return _show;
    }

    [[nodiscard]] int get_focus_line() const {
        return focus_line;
    }

    [[nodiscard]] int get_offset_x() const {
        return offset_x;
    }

    [[nodiscard]] float get_scroll_offset_y() const {
        return scroll_offset_y;
    }

    [[nodiscard]] float get_x() const {
        return x;
    }

    [[nodiscard]] float get_y() const {
        return y;
    }
};

/**
 * Main entry point for the SDL3 Text Editor application.
 *
 * This program is a simple text editor built with SDL3 and SDL_ttf that supports:
 * - Loading and displaying text files with UTF-8 encoding
 * - Interactive text editing with cursor positioning via mouse clicks
 * - Keyboard navigation (arrow keys) and text input
 * - Backspace deletion and scrolling support
 * - Real-time text rendering with proper font handling
 *
 * Workflow:
 * 1. Parses command-line arguments for text file path
 * 2. Loads the specified text file and reads all lines
 * 3. Initializes SDL3 video system and TTF font rendering
 * 4. Creates a window with high DPI support and Metal rendering
 * 5. Pre-renders all text lines as Textarea objects
 * 6. Runs the main event loop handling:
 *    - Mouse clicks for cursor positioning and text focus
 *    - Keyboard input for text editing and navigation
 *    - Real-time rendering with 60 FPS frame rate control
 * 7. Performs proper cleanup of all SDL resources on exit
 *
 * Command-line options:
 *   -f <file_path>    Path to the text file to edit (default: "./test.txt")
 *   -h                Show help message
 *
 * Dependencies:
 *   - SDL3 (video, events, rendering)
 *   - SDL3_ttf (font rendering and text layout)
 *   - utf8cpp (UTF-8 string processing)
 *   - spdlog (logging)
 *   - clipp (command-line parsing)
 */
int main(int argc, char *argv[]) {
    std::string file_path = "./test.txt";
    bool show_help = false;
    auto cli = (
        clipp::option("-f").doc("Path to the text file") & clipp::value("file path", file_path),
        clipp::option("-h").set(show_help).doc("Show help")
    );
    if (!clipp::parse(argc, argv, cli) || show_help) {
        std::cout << "Usage:\n";
        std::cout << clipp::usage_lines(cli) << std::endl;
        exit(EXIT_FAILURE);
    }
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        exit(EXIT_FAILURE);
    }
    std::vector<std::string> text_lines;
    std::string line;
    while (std::getline(file, line)) {
        text_lines.push_back(line);
    }
    file.close();

    if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO)) {
        std::cout << "SDL could not initialize! Get_Error: " << SDL_GetError() << std::endl;  
        exit(EXIT_FAILURE);
    }
    if (!TTF_Init()) {
        std::cout << "TTF could not initialize! Get_Error: " << SDL_GetError() << std::endl;  
        exit(EXIT_FAILURE);
    }
    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Hiragino Sans GB.ttc", TEXT_SIZE);
    if (font == nullptr) {
        std::cout << "Couldn't open font: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    SDL_Window *window;
    SDL_Renderer *renderer;
    if (!SDL_CreateWindowAndRenderer("SDL3 Text Editor", SCREEN_WIDTH, SCREEN_HEIGHT,  SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) {
        std::cout << "SDL_CreateWindowAndRenderer failed: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    TTF_TextEngine *text_engine = TTF_CreateRendererTextEngine(renderer);
    if (text_engine == nullptr) {
        std::cout << "TTF_CreateRendererTextEngine failed: " << SDL_GetError() << std::endl;
        exit(EXIT_FAILURE);
    }

    int win_w, win_h = 0;
    SDL_GetWindowSizeInPixels(window, &win_w, &win_h);

    std::vector<Textarea> text_lines_rendered;
    int prev_text_h = TEXT_PADDING;
    for (size_t i = 0; i < text_lines.size(); ++i) {
        const std::string& line = text_lines[i];
        TTF_Text *text = TTF_CreateText(text_engine, font, line.c_str(), line.size());
        if (text == nullptr) {
            std::cout << "TTF_CreateText failed for line: " << SDL_GetError() << std::endl;
            exit(EXIT_FAILURE);
        }
        // Set text wrapping width to window width
        // TTF_SetTextWrapWidth(text, win_w);

        int text_w = 0, text_h = TEXT_SIZE;
        TTF_GetStringSize(font, line.c_str(), line.size(), &text_w, nullptr);

        Textarea textarea;
        textarea.text = text;
        textarea.x = static_cast<float>(TEXT_PADDING);
        textarea.y = static_cast<float>(prev_text_h);
        textarea.w = text_w;
        textarea.h = text_h;
        textarea.content = std::string(line);
        textarea.line_number = static_cast<int>(i + 1);
        textarea.lang = "zh";
        text_lines_rendered.push_back(textarea);

        prev_text_h += text_h + TEXT_PADDING;
    }

    Cursor cursor;

    bool quit = false;
    SDL_Event event;
    while (!quit) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float mouse_x = event.button.x * DISPLAY_SCALE;
                float mouse_y = event.button.y * DISPLAY_SCALE;
                if (cursor.on_focus(font, text_lines_rendered, mouse_x, mouse_y)) {
                    SDL_StartTextInput(window);
                    SDL_Rect input_rect = {
                        static_cast<int>(cursor.get_x() / DISPLAY_SCALE),0,100, 48
                    };
                    SDL_SetTextInputArea(window, &input_rect, 0);
                } else {
                    cursor.on_blur();
                    SDL_StopTextInput(window);
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                cursor.on_blur();
                SDL_StopTextInput(window);
            }
            if (event.type == SDL_EVENT_KEY_DOWN && cursor.show()) {
                Textarea& textarea = text_lines_rendered[cursor.get_focus_line() - 1];
                if (event.key.key == SDLK_LEFT) {
                    cursor.left(font, textarea);
                } else if (event.key.key == SDLK_RIGHT) {
                    cursor.right(font, textarea);
                } else if (event.key.key == SDLK_UP) {
                    cursor.up(font, text_lines_rendered);
                } else if (event.key.key == SDLK_DOWN) {
                    cursor.down(font, text_lines_rendered);
                } else if (event.key.key == SDLK_BACKSPACE) {
                    size_t distance = cursor.left(font, textarea);
                    textarea.content.erase(cursor.get_offset_x(), distance);
                    TTF_SetTextString(textarea.text, textarea.content.c_str(), textarea.content.length());
                    TTF_GetTextSize(textarea.text, &textarea.w, &textarea.h);
                }
                spdlog::info("Cursor focus_line={} x={} y={} offset_x={} scroll_offset_y={}", cursor.get_focus_line(), cursor.get_x(), cursor.get_y(), cursor.get_offset_x(), cursor.get_scroll_offset_y());
                cursor.refresh_blink_timer();
                SDL_Rect input_rect = {
                    static_cast<int>(cursor.get_x() / DISPLAY_SCALE),0,100, 48
                };
                SDL_SetTextInputArea(window, &input_rect, 0);
            }
            if (event.type == SDL_EVENT_TEXT_INPUT && cursor.show()) {
                std::string intput_text(event.text.text);
                int substr_w = 0;
                TTF_GetStringSize(font, intput_text.c_str(), intput_text.length(), &substr_w, nullptr);

                Textarea& textarea = text_lines_rendered[cursor.get_focus_line() - 1];
                textarea.content.insert(cursor.get_offset_x(), intput_text.data(), intput_text.length());
                TTF_SetTextString(textarea.text, textarea.content.c_str(), textarea.content.length());
                TTF_GetTextSize(textarea.text, &textarea.w, &textarea.h);

                cursor.right(substr_w, intput_text.length());
            }
            if (event.type == SDL_EVENT_MOUSE_WHEEL && cursor.show()) {
                if (event.wheel.y > 0) {
                    cursor.up(font, text_lines_rendered);
                } else if (event.wheel.y < 0) {
                    cursor.down(font, text_lines_rendered);
                } else if (event.wheel.x > 0) {
                    cursor.left(font, text_lines_rendered[cursor.get_focus_line() - 1]);
                } else if (event.wheel.x < 0) {
                    cursor.right(font, text_lines_rendered[cursor.get_focus_line() - 1]);
                }
            }
            if (event.key.key == SDLK_S && ((event.key.mod & SDL_KMOD_CTRL) || (event.key.mod & SDL_KMOD_GUI))) {
                std::ofstream out_file(file_path);
                for (const auto& textarea : text_lines_rendered) {
                    out_file << textarea.content << "\n";
                }
                out_file.close();
                spdlog::info("File saved successfully");
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        for (auto & textarea : text_lines_rendered) {
            auto scroll_offset_y = cursor.get_scroll_offset_y();
            float draw_y = textarea.y - scroll_offset_y;
            if (draw_y + static_cast<float>(textarea.h) < 0) {
                continue;
            }
            if (draw_y > SCREEN_HEIGHT * DISPLAY_SCALE) {
                break;
            }
            TTF_DrawRendererText(textarea.text, textarea.x, draw_y);
        }
        cursor.draw(renderer);
        SDL_RenderPresent(renderer);

        auto frame_end = std::chrono::high_resolution_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
        constexpr int target_frame_us = 16667; // 60 FPS
        if (frame_duration.count() < target_frame_us) {
            std::this_thread::sleep_for(std::chrono::microseconds(target_frame_us - frame_duration.count()));
        }
    }
    TTF_DestroyRendererTextEngine(text_engine);
    SDL_StopTextInput(window);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    SDL_Quit();
    return 0;
}