#pragma once
#include <filesystem>
#include <string>

namespace cath {

struct SaveGame {
  int version = 1;
  int script_index = 0;
  int puzzle_stage = 0;
  std::string checkpoint;
  std::string ending;  // non-empty if reached
  bool new_game_started = false;
};

bool save_game(const std::filesystem::path& path, const SaveGame& save, std::string* error = nullptr);
bool load_game(const std::filesystem::path& path, SaveGame& save, std::string* error = nullptr);

std::filesystem::path default_save_path();

}  // namespace cath
