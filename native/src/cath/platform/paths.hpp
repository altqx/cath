#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace cath {

std::filesystem::path default_steam_game_dir();
std::filesystem::path resolve_game_dir(const std::optional<std::string>& cli_override = std::nullopt);

}  // namespace cath
