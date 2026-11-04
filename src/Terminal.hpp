#pragma once

#include "Tetro.hpp"

#include <chrono>
#include <iostream>
#include <vector>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
	#include <conio.h>
	#include <windows.h>
	#include <filesystem>
#endif

namespace tetro {

	inline std::filesystem::path executable_directory() {
		std::wstring buffer(32768, L'\0');

		DWORD length = GetModuleFileNameW(
			nullptr,
			buffer.data(),
			static_cast<DWORD>(buffer.size())
		);

		if (length == 0 || length >= buffer.size()) throw std::runtime_error("Failed to locate executable.");

		buffer.resize(length);
		return std::filesystem::path(buffer).parent_path();
	}

	struct DasKeyState {

		bool was_down = false;
		double hold_timer = 0.0;
		double repeat_timer = 0.0;

		// das_delay: initial pause before repeat (e.g. 0.15s)
		// arr_rate: delay between repeat steps (e.g. 0.03s)
		bool update(int vk, double dt, double das_delay = 0.100, double arr_rate = 0.040) {
			bool is_down = (GetAsyncKeyState(vk) & 0x8000) != 0;

			// Key pressed
			if (is_down && !was_down) {
				was_down = true;
				hold_timer = 0.0;
				repeat_timer = 0.0;
				return true;
			}

			// Key released
			if (!is_down) {
				was_down = false;
				hold_timer = 0.0;
				repeat_timer = 0.0;
				return false;
			}

			// Key held
			hold_timer += dt;
			if (hold_timer >= das_delay) {
				repeat_timer += dt;
				if (repeat_timer >= arr_rate) {
					repeat_timer -= arr_rate;
					return true;
				}
			}

			return false;
		}
	};

	struct TerminalConfig {
		int fontSize = 20;
		char BLK_EMPTY = char(176);
		char BLK_FULL = char(219);
	} TERMINAL_CONFIG;

	class Terminal {

	public:

		Terminal(TerminalConfig cfg) { initialize(cfg); }
		Terminal() { initialize(TERMINAL_CONFIG); }
		~Terminal() { restore(true); }

		Terminal(const Terminal&) = delete;
		Terminal& operator=(const Terminal&) = delete;

		void initialize(TerminalConfig cfg){
			if (initialized_) return;

			TERMINAL_CONFIG = cfg;

			std::ios_base::sync_with_stdio(false);

#if defined(_WIN32)
			input_ = GetStdHandle(STD_INPUT_HANDLE);
			stdout_ = GetStdHandle(STD_OUTPUT_HANDLE);

			have_input_mode_ = GetConsoleMode(input_, &old_input_mode_);
			if (have_input_mode_) SetConsoleMode(input_, old_input_mode_ & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

			have_output_mode_ = GetConsoleMode(stdout_, &old_output_mode_);
			if (have_output_mode_) SetConsoleMode(stdout_, old_output_mode_ | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

			// Enable alternate screen buffer (\x1b[?1049h) and hide cursor (\x1b[?25l)
			std::cout << "\x1b[?1049h\x1b[?25l";
			std::cout.flush();
			
			// Open the active screen buffer.
			output_ = CreateFileW(
				L"CONOUT$",
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				nullptr,
				OPEN_EXISTING,
				0,
				nullptr
			);
			
			if (output_ == INVALID_HANDLE_VALUE) {
				std::cerr << "CreateFileW failed: " << GetLastError() << '\n';
				return;
			} else if (stdout_ == INVALID_HANDLE_VALUE || input_ == INVALID_HANDLE_VALUE) {
				std::cerr << "GetStdHandle failed: " << GetLastError() << '\n';
				return;
			}else if (!have_input_mode_ || !have_output_mode_) {
				std::cerr << "SetConsoleMode failed: " << GetLastError() << '\n';
				return;
			}
			
			old_font_info_.cbSize = sizeof(CONSOLE_FONT_INFOEX);

			have_font_info_ = GetCurrentConsoleFontEx(output_, FALSE, &old_font_info_);
			if (!have_font_info_) {
				std::cerr << "GetCurrentConsoleFontEx failed: " << GetLastError() << '\n';
				return;
			}

			font(output_, TERMINAL_CONFIG.fontSize);
			center(output_);

#endif
			initialized_ = true;
		}
		
		void restore(bool prefill_command){
			
			if (!initialized_) return;

#if defined(_WIN32)
			if (have_font_info_) {
				if (!SetCurrentConsoleFontEx(
					output_,
					FALSE,
					&old_font_info_
				)) {
					std::cerr << "Font restoration failed: " << GetLastError() << '\n';
				}
			}

			// Restore main screen buffer (\x1b[?1049l) and show cursor (\x1b[?25h)
			std::cout << "\x1b[0m\x1b[?25h\x1b[?1049l";
			std::cout.flush();

			if (have_output_mode_) SetConsoleMode(stdout_, old_output_mode_);
			if (have_input_mode_) SetConsoleMode(input_, old_input_mode_);

			if (input_ != INVALID_HANDLE_VALUE) {

				if (!FlushConsoleInputBuffer(input_)) std::cerr << "Failed to flush console input: " << GetLastError() << '\n';
				else if (prefill_command) type_command(L"tetro");
			}

			if (output_ != INVALID_HANDLE_VALUE) {
				CloseHandle(output_);
				output_ = INVALID_HANDLE_VALUE;
			}
#endif

			initialized_ = false;
		}

		void type_command(const std::wstring& command) {
#if defined(_WIN32)
			std::vector<INPUT_RECORD> events;
			events.reserve(command.size() * 2);

			for (wchar_t ch : command) {
				SHORT mapped = VkKeyScanW(ch);
				if (mapped == -1) continue;

				WORD vk = LOBYTE(mapped);

				KEY_EVENT_RECORD key{};
				key.bKeyDown = TRUE;
				key.wRepeatCount = 1;
				key.wVirtualKeyCode = vk;
				key.wVirtualScanCode = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
				key.uChar.UnicodeChar = ch;

				INPUT_RECORD event{};
				event.EventType = KEY_EVENT;
				event.Event.KeyEvent = key;

				events.push_back(event);
				
				key.bKeyDown = FALSE;
				event.Event.KeyEvent = key;


				events.push_back(event);
			}

			DWORD written = 0;

			if (!events.empty()) {
				if (!WriteConsoleInputW(input_, events.data(), static_cast<DWORD>(events.size()), &written) ||
					written != events.size()) {
					std::cerr << "Failed to insert restart command: " << GetLastError() << '\n';
				}
			}

#endif
		}

		bool poll_action(Action& action, double dt) {
#if defined(_WIN32)
			// Direct key-state polling bypasses Windows console auto-repeat delay (DAS stutter)
			if (left_key_.update(VK_LEFT, dt)) { action = Action::Left; return true; }
			if (right_key_.update(VK_RIGHT, dt)) { action = Action::Right; return true; }
			if (rotate_key_.update(VK_UP, dt, .1, .15)) { action = Action::Rotate; return true; }
			if (drop_key_.update(VK_DOWN, dt, .1, .05)) { action = Action::SoftDrop; return true; }
			if (harddrop_key_.update(VK_SPACE, dt, .3, .3)) { action = Action::HardDrop; return true; }
			if (GetAsyncKeyState(VK_ESCAPE) & 0x8000 || GetAsyncKeyState('Q') & 0x8000) { action = Action::Quit; return true; }
#endif
			return false;
		}

		void center(HANDLE hndl) {
#if defined(_WIN32)
			CONSOLE_SCREEN_BUFFER_INFO csbi{};
			int rows, cols;

			if (!GetConsoleScreenBufferInfo(hndl, &csbi))
				return;

			rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
			cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;

			rows = (rows / 2) - (HEIGHT_ / 2);
			cols = (cols / 2) - (WIDTH_ / 2);

			cursor_pos_y_ = "\x1b[" + std::to_string(rows) + ";" + std::to_string(cols) + "H";
			cursor_pos_x_ = "\x1b[" + std::to_string(cols) + "G";
#endif
		}

		void font(HANDLE hndl, int fontSize) {
			CONSOLE_FONT_INFOEX info{};

			info.cbSize = sizeof(info);
			info.nFont = 0;
			info.dwFontSize.X = 0;
			info.dwFontSize.Y = fontSize;
			info.FontFamily = FF_MODERN | TMPF_TRUETYPE;
			info.FontWeight = FW_NORMAL;

			wcscpy_s(info.FaceName, LF_FACESIZE, L"Consolas");

			if (!SetCurrentConsoleFontEx(hndl, FALSE, &info)) {
				std::cerr << "SetCurrentConsoleFontEx failed: " << GetLastError() << '\n';
				return;
			}

		}

		void render(const Game& game) {
			const auto view = game.visible_board();

			frame_buffer_.clear();
			frame_buffer_.reserve((WIDTH_ + 2) * HEIGHT_ + 128);

			frame_buffer_.append(cursor_pos_y_); // Cursor home

			for (int y = 0; y < HEIGHT_; ++y) {
				const std::uint16_t row = view[y] & 0x0FFF;
				for (int x = 0; x < WIDTH_; ++x) {
					const bool occupied = (row & (0x0800 >> x)) != 0;
					frame_buffer_.push_back(occupied ? TERMINAL_CONFIG.BLK_FULL : TERMINAL_CONFIG.BLK_EMPTY);
				}
				frame_buffer_.append("\n");
				frame_buffer_.append(cursor_pos_x_);
			}

			frame_buffer_.append("\r\n(Arrows: move/rotate/drop, Space: hard drop, Q: quit)\x1b[J");

			// Single continuous write pass eliminates frame tearing
			std::cout.write(frame_buffer_.data(), frame_buffer_.size());
			std::cout.flush();
		}

		private:
			bool initialized_ = false;
			std::string frame_buffer_;

			DasKeyState left_key_;
			DasKeyState right_key_;
			DasKeyState rotate_key_;
			DasKeyState drop_key_;
			DasKeyState harddrop_key_;

			std::string cursor_pos_y_, cursor_pos_x_;

#if defined(_WIN32)
			HANDLE input_ = INVALID_HANDLE_VALUE;
			HANDLE output_ = INVALID_HANDLE_VALUE;
			HANDLE stdout_ = INVALID_HANDLE_VALUE;
			DWORD old_input_mode_ = 0;
			DWORD old_output_mode_ = 0;
			CONSOLE_FONT_INFOEX old_font_info_ = { sizeof(CONSOLE_FONT_INFOEX) };
			bool have_input_mode_ = false;
			bool have_output_mode_ = false;
			bool have_font_info_ = false;
#endif

			int FONTSIZE_;
	};
}