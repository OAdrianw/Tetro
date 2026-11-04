#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <iostream>
#include <string>
#include <map>

namespace tetro {
	class AudioManager {
	private:
		ma_engine engine;
		std::map<std::string, std::string> audioFiles;
		std::map<std::string, ma_sound> activeSounds;
		bool initialized = false;
	public:
		AudioManager() : initialized(false) {
			ma_engine_config config = ma_engine_config_init();
			if (ma_engine_init(&config, &engine) == MA_SUCCESS) initialized = true;
			else std::cerr << "Audio failed initializing." << std::endl;
		}

		~AudioManager() {
			if (initialized) {

				for (auto& pair : activeSounds) {
					ma_sound_stop(&pair.second);
					ma_sound_uninit(&pair.second);
				}
				ma_engine_uninit(&engine);
			}
		}

		void add_audio(const std::string identifier, const std::string& path) {
			audioFiles[identifier] = path;
		}

		void play_sfx(const std::string& identifier) {
			if (!initialized) return;
			else {
				auto it = audioFiles.find(identifier);
				
				if (it != audioFiles.end()) ma_engine_play_sound(&engine, it->second.c_str(), NULL);
				else std::cerr << "Sound identifier not found: " << identifier << std::endl;
			}
		}

		void start_bgm(const std::string& identifier, float volume = 0.4f, bool loop = true) {
			if (!initialized) {
				std::cerr << "! Audio Engine not initialized!" << std::endl;
				return;
			}

			auto it = audioFiles.find(identifier);
			if (it != audioFiles.end()) {
				stop_bgm(identifier);

				auto [soundIt, inserted] = activeSounds.try_emplace(identifier);
				ma_sound& sound = soundIt->second;
				ma_result result = ma_sound_init_from_file(&engine, it->second.c_str(), 0, NULL, NULL, &sound);

				if (result == MA_SUCCESS) {
					ma_sound_set_volume(&sound, volume);
					ma_sound_set_looping(&sound, loop ? MA_TRUE : MA_FALSE);
					result = ma_sound_start(&sound);

					if (result != MA_SUCCESS) {
						std::cerr << "Failed to start audio. Error: "
							<< result << std::endl;

						ma_sound_uninit(&sound);
						activeSounds.erase(soundIt);
					}

				} else {
					std::cerr << "Failed to load audio file: " << it->second << std::endl;
					//std::cerr << "(Error code: " << result << ")" << std::endl;
					activeSounds.erase(soundIt);
					return;
				}
			} else {
				std::cerr << "! Identifier '" << identifier << "' not found in AudioManager!" << std::endl;
				return;
			}
		}

		void stop_bgm(const std::string& identifier) {
			if (!initialized) return;

			auto it = activeSounds.find(identifier);
			if (it != activeSounds.end()) {
				ma_sound_stop(&it->second);
				ma_sound_uninit(&it->second);
				activeSounds.erase(it);
			}
		}
	};
};

