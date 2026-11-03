#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>

namespace tetro {

	double GRAVITY_DELAY = 0.25;

	constexpr int WIDTH_LIMIT = 2;
	constexpr int HEIGHT_LIMIT = 1;
	constexpr int WIDTH_ = 10 + WIDTH_LIMIT; 
	constexpr int HEIGHT_ = 20 + HEIGHT_LIMIT;
	
	// Four 4-bit rows per piece rotation
	constexpr uint16_t TETROMINOES[7][4] = {
		{0x0660, 0x0660, 0x0660, 0x0660}, // O 
		{0x00F0, 0x4444, 0x00F0, 0x4444}, // I
		{0x006C, 0x08C4, 0x006C, 0x08C4}, // S
		{0x00C6, 0x04C8, 0x00C6, 0x04C8}, // Z
		{0x00E2, 0x044C, 0x08E0, 0x0644}, // J
		{0x00E8, 0x0C44, 0x02E0, 0x0446}, // L
		{0x00E4, 0x04C4, 0x04E0, 0x0464}, // T
	};

	enum class Action {
		Left,
		Right,
		Rotate,
		SoftDrop,
		HardDrop,
		Quit
	};

	struct GameConfig {
		float speed = 0.25;
	} MAIN_CONFIG;

	class Game {
	public:
		using Board = std::array<uint16_t, HEIGHT_>;

		Game() { init(MAIN_CONFIG); reset(); }

		void init(GameConfig cfg) {
			GRAVITY_DELAY = cfg.speed;
		}

		void reset(uint8_t type0 = 6) {

			for (int y = 0; y < HEIGHT_ - 1; y++) {
				board_[y] = 0x0801;
			}
			board_[HEIGHT_ - 1] = 0xFFFF;

			gravity_accumulator_ = 0.0;
			running_ = true;
			spawn(type0 % 7);
		};

		void handle(Action action) {
			if (!running_) return;

			switch (action) {
			case Action::Left: 

				if (can_place(x_ + 1, y_, rotation_)) ++x_;
				break;
			case Action::Right: 

				if (can_place(x_ - 1, y_, rotation_)) --x_;
				break;
			case Action::Rotate:
			{
				const auto next = static_cast<uint8_t>((rotation_ + 1) % 4);
				if (can_place(x_, y_, next)) rotation_ = next;
				break;
			}
			case Action::SoftDrop: 

				if (can_place(x_, y_ + 1, rotation_)) {
					++y_;
					gravity_accumulator_ = 0.0;
				}
				break;
			case Action::HardDrop: 
				while (can_place(x_, y_ + 1, rotation_))
					++y_;
				lock();
				break;
			case Action::Quit: 
				running_ = false;
				break;
			}
		}

		void tick(double dt) {
			if (!running_ || dt <= 0.0) return;

			gravity_accumulator_ += dt;

			while(gravity_accumulator_ > GRAVITY_DELAY && running_) {
				gravity_accumulator_ -= GRAVITY_DELAY;
				
				if (can_place(x_, y_ + 1, rotation_)) {
					++y_;
				}
				else {
					lock();
				}
			}
		};

		bool running() const { return running_; }
		Board& board() { return board_; }

		Board visible_board() const {
			Board result = board_;
			uint16_t tetromino = TETROMINOES[type_][rotation_];

			for (int i = 0; i < 4; ++i) {
				const uint16_t row = (tetromino >> (12 - 4 * i)) & 0xF;
				const int by = y_ + i;

				if (row && by >= 0 && by < HEIGHT_)
					result[by] |= shift_row(row, x_);
			}
			return result;
		}

	private:
		Board board_{};
		int type_ = 6;
		uint8_t rotation_ = 0;
		int x_ = (WIDTH_ - 4) / 2;
		int y_ = 0;
		double gravity_accumulator_ = 0.0;
		bool running_ = true;

		static uint16_t shift_row(uint16_t row, int x) {
			if (x >= 0) return static_cast<uint16_t>(row << x);
			return static_cast<uint16_t>(row >> -x);
		}

		bool can_place(int x, int y, uint8_t rotation) const {

			if (x < -3 || x > 11) return false;

			uint16_t tetromino = TETROMINOES[type_][rotation];
			for (int i = 0; i < 4; ++i) {
				const uint16_t row = (tetromino >> (12 - 4 * i)) & 0xF;
				if (!row) continue;

				const int by = y + i;
				if (by < 0 || by >= HEIGHT_) return false;

				const uint16_t mask = shift_row(row, x);

				if ((mask & 0xF001) || (mask & board_[by])) return false;
			}
			return true;
		}
		
		void spawn(int type) {

			type_ = type != -1 ? type : rand() % 7;
			rotation_ = 0;
			x_ = (WIDTH_ - 4) / 2;
			y_ = 0;

			if (!can_place(x_, y_, rotation_)) running_ = false;
		}

		void lock() {
			uint16_t tetromino = TETROMINOES[type_][rotation_];
			for (int i = 0; i < 4; ++i) {
				const uint16_t row = (tetromino >> (12 - 4 * i)) & 0xF;
				if (!row) continue;

				const int by = y_ + i;
				if (by >= 0 && by < HEIGHT_ - 1) 
					board_[by] |= shift_row(row, x_);
			}

			clear_lines();
			spawn(-1);
		}

		void clear_lines() {
			int write = HEIGHT_ - 1;

			for (int read = HEIGHT_ - 1; read >= 0; --read) {

				if (board_[read] == 0x0FFF) continue;
				if (write != read) board_[write] = board_[read];
				--write;
			}

			while (write >= 0) {
				board_[write--] = 0x0801;
			}
		}

	};
}