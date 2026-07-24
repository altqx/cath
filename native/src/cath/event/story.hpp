#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace cath {

enum class StoryOpcode {
  Nop,
  Title,
  PlayMovie,   // arg = relative path under data/
  PlayMovie2,
  EnterLounge,
  EnterPuzzle,  // arg = stage key e.g. builtin:0 or map:p002_001
  Cellphone,
  Confessional,
  Ending,  // arg = ending id
  SaveCheckpoint,
};

struct StoryCommand {
  StoryOpcode op = StoryOpcode::Nop;
  std::string arg;
  std::string note;
};

struct StoryScript {
  std::vector<StoryCommand> commands;
};

// Hardcoded vertical-slice script: New Game → cutscenes → stages 1–3 → lounge → Freedom ending.
StoryScript make_story_one_ending_script();

// BF/FLW0 probe — records flow labels for RE notes.
struct BfProbe {
  std::string path;
  std::vector<std::string> labels;
  bool valid = false;
};

bool probe_bf(const std::filesystem::path& path, BfProbe& out, std::string* error = nullptr);

}  // namespace cath
