#include "cath/game/save.hpp"

#include "cath/platform/log.hpp"

#include <fstream>
#include <sstream>

namespace cath {

std::filesystem::path default_save_path() {
  const char* home = getenv("HOME");
  std::filesystem::path base = home ? std::filesystem::path(home) / ".local/share/cath" : std::filesystem::path(".");
  std::filesystem::create_directories(base);
  return base / "save0.json";
}

bool save_game(const std::filesystem::path& path, const SaveGame& save, std::string* error) {
  std::ofstream out(path);
  if (!out) {
    if (error) {
      *error = "save open failed";
    }
    return false;
  }
  out << "{\n";
  out << "  \"version\": " << save.version << ",\n";
  out << "  \"script_index\": " << save.script_index << ",\n";
  out << "  \"puzzle_stage\": " << save.puzzle_stage << ",\n";
  out << "  \"checkpoint\": \"" << save.checkpoint << "\",\n";
  out << "  \"ending\": \"" << save.ending << "\",\n";
  out << "  \"new_game_started\": " << (save.new_game_started ? "true" : "false") << "\n";
  out << "}\n";
  CATH_LOG_INFO("saved %s (idx=%d ending=%s)", path.string().c_str(), save.script_index, save.ending.c_str());
  return true;
}

static bool extract_int(const std::string& s, const char* key, int& v) {
  const auto p = s.find(std::string("\"") + key + "\"");
  if (p == std::string::npos) {
    return false;
  }
  const auto colon = s.find(':', p);
  if (colon == std::string::npos) {
    return false;
  }
  v = std::atoi(s.c_str() + colon + 1);
  return true;
}

static bool extract_string(const std::string& s, const char* key, std::string& v) {
  const auto p = s.find(std::string("\"") + key + "\"");
  if (p == std::string::npos) {
    return false;
  }
  const auto colon = s.find(':', p);
  const auto q1 = s.find('"', colon + 1);
  const auto q2 = s.find('"', q1 + 1);
  if (q1 == std::string::npos || q2 == std::string::npos) {
    return false;
  }
  v = s.substr(q1 + 1, q2 - q1 - 1);
  return true;
}

bool load_game(const std::filesystem::path& path, SaveGame& save, std::string* error) {
  std::ifstream in(path);
  if (!in) {
    if (error) {
      *error = "load open failed";
    }
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  SaveGame s;
  extract_int(content, "version", s.version);
  extract_int(content, "script_index", s.script_index);
  extract_int(content, "puzzle_stage", s.puzzle_stage);
  extract_string(content, "checkpoint", s.checkpoint);
  extract_string(content, "ending", s.ending);
  s.new_game_started = content.find("\"new_game_started\": true") != std::string::npos;
  save = s;
  CATH_LOG_INFO("loaded %s (idx=%d ending=%s)", path.string().c_str(), save.script_index, save.ending.c_str());
  return true;
}

}  // namespace cath
