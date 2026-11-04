
#include "Terminal.hpp"
#include "AudioManager.hpp"
#include "cxxopts.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

using namespace std;

const auto assetDir = tetro::executable_directory() / "assets";

const double lock_delay = 0.0;
bool wallkick_flag = true;
bool lock_flag = false;

bool isNumber(string& param) {
	if (param.empty()) return false;

	char* p = nullptr;
	long converted = strtol(param.c_str(), &p, 10);
	if (p == param.c_str() || *p != '\0') return false;
	return true;
}

int main(int argc, char** argv) {

	tetro::TerminalConfig tcfg;
	tetro::AudioManager audio;

	try {
		cxxopts::Options options("Tetro", "A lightweight Tetris-like terminal game.");
		options.set_width(100);
		options.add_options()
			("h,help", "Print usage.")
			("f,font", "Change font size of the game.", cxxopts::value<int>()->default_value("20"))
			("blk_empty", "Character or ASCII code for empty block.", cxxopts::value<string>()->default_value(string(1, tcfg.BLK_EMPTY)))
			("blk_full", "Character or ASCII code for full block.", cxxopts::value<string>()->default_value(string(1, tcfg.BLK_FULL)));
		auto result = options.parse(argc, argv);

		if (result.count("help")) {
			cout << options.help() << endl;
			return 0;
		}

		tcfg.fontSize = result["font"].as<int>();

		string empty_str = result["blk_empty"].as<string>();
		if (isNumber(empty_str)) tcfg.BLK_EMPTY = char(stoi(empty_str));
		else tcfg.BLK_EMPTY = empty_str[0];

		string full_str = result["blk_full"].as<string>();
		if (isNumber(full_str)) tcfg.BLK_FULL = char(stoi(full_str));
		else tcfg.BLK_FULL = full_str[0];

	}
	catch (const cxxopts::exceptions::exception& e) {
		cerr << "Error parsing option: " << e.what() << endl;
		return 1;
	}

	using clock = chrono::steady_clock;
	using system = chrono::system_clock;
	srand(static_cast<unsigned>(system::to_time_t(system::now())));

	tetro::Game game;
	tetro::Terminal terminal(tcfg);

	//audio.add_audio("theme", (assetDir / "tetris_theme.mp3").string());
	//audio.start_bgm("theme", 0.2f, true);

	auto previous = clock::now();
	while (game.running()) {
		const auto now = clock::now();
		const double dt = chrono::duration<double>(now - previous).count();
		previous = now;

		tetro::Action action{};
		while (terminal.poll_action(action, dt))
			game.handle(action);

		game.tick(dt);
		terminal.render(game);
		this_thread::sleep_for(chrono::milliseconds(1));
	}

	//audio.stop_bgm("theme");
	terminal.restore(true);

	return 0;
}