#include "cath/platform/paths.hpp"

#include <cstdlib>

namespace cath {
namespace fs = std::filesystem;

fs::path default_steam_game_dir() {
  const char* home = std::getenv("HOME");
  if (!home) {
    return {};
  }
  return fs::path(home) / ".local/share/Steam/steamapps/common/CatherineClassic";
}

fs::path resolve_game_dir(const std::optional<std::string>& cli_override) {
  if (cli_override && !cli_override->empty()) {
    return fs::path(*cli_override);
  }
  if (const char* env = std::getenv("CATH_GAME_DIR"); env && *env) {
    return fs::path(env);
  }
  return default_steam_game_dir();
}

}  // namespace cath
