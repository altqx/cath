#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace cath {

struct TeaString {
  std::string id;
  std::string text;
};

struct TeaMenuItem {
  std::string id;
  std::string label;
};

struct TeaDocument {
  std::vector<TeaString> strings;
  std::vector<TeaMenuItem> menu;
  std::unordered_map<std::string, std::string> patches;
};

// Minimal XML scrape for StringPatch / MailPatch and synthetic title menu.
bool load_tea_string_patch(const std::filesystem::path& xml, TeaDocument& out, std::string* error = nullptr);
TeaDocument make_title_menu();

}  // namespace cath
