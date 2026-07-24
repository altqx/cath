#include "cath/event/story.hpp"

#include "cath/platform/log.hpp"

#include <fstream>
#include <cstring>

namespace cath {

StoryScript make_story_one_ending_script() {
  StoryScript s;
  auto add = [&](StoryOpcode op, std::string arg = {}, std::string note = {}) {
    s.commands.push_back({op, std::move(arg), std::move(note)});
  };
  add(StoryOpcode::Title, {}, "title menu");
  add(StoryOpcode::PlayMovie2, "movie2/000_00.wmv", "logo");
  add(StoryOpcode::PlayMovie2, "movie2/001_00.wmv", "shakespeare");
  add(StoryOpcode::PlayMovie2, "movie2/002_00.wmv", "golden playhouse");
  add(StoryOpcode::SaveCheckpoint, "after_intro");
  add(StoryOpcode::EnterPuzzle, "builtin:0", "tutorial tower");
  add(StoryOpcode::EnterPuzzle, "map:p002_001", "night 1 stage A");
  add(StoryOpcode::EnterPuzzle, "builtin:1", "night 1 stage B");
  add(StoryOpcode::EnterLounge, {}, "bar lounge stub");
  add(StoryOpcode::Cellphone, {}, "mail stub");
  add(StoryOpcode::EnterPuzzle, "builtin:2", "night 2");
  add(StoryOpcode::Confessional, {}, "confession stub");
  add(StoryOpcode::PlayMovie, "movie/012_00.wmv", "story beat");
  add(StoryOpcode::Ending, "freedom", "one ending");
  add(StoryOpcode::SaveCheckpoint, "ending_freedom");
  return s;
}

bool probe_bf(const std::filesystem::path& path, BfProbe& out, std::string* error) {
  out = {};
  out.path = path.string();
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    if (error) {
      *error = "open failed";
    }
    return false;
  }
  const auto sz = size_t(in.tellg());
  in.seekg(0);
  std::vector<char> data(sz);
  in.read(data.data(), std::streamsize(sz));
  // Collect ASCII labels
  std::string cur;
  for (size_t i = 0; i < sz; ++i) {
    const unsigned char c = (unsigned char)data[i];
    if (c >= 32 && c < 127) {
      cur.push_back(char(c));
    } else {
      if (cur.size() >= 4 && (cur.find("pzl_") != std::string::npos || cur.find("MSG_") != std::string::npos ||
                              cur.find("FLW") != std::string::npos || cur.find("_start") != std::string::npos)) {
        out.labels.push_back(cur);
      }
      cur.clear();
    }
  }
  out.valid = !out.labels.empty() || sz > 16;
  CATH_LOG_INFO("BF probe %s: %zu labels", path.filename().c_str(), out.labels.size());
  return out.valid;
}

}  // namespace cath
