#include "cath/ui/tea.hpp"

#include <fstream>
#include <regex>

namespace cath {

bool load_tea_string_patch(const std::filesystem::path& xml, TeaDocument& out, std::string* error) {
  std::ifstream in(xml);
  if (!in) {
    if (error) {
      *error = "open failed";
    }
    return false;
  }
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  // <String id="..." value="..."/> or similar
  std::regex re(R"re(id="([^"]+)".*?value="([^"]*)")re", std::regex::icase);
  auto begin = std::sregex_iterator(content.begin(), content.end(), re);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    TeaString s;
    s.id = (*it)[1];
    s.text = (*it)[2];
    out.strings.push_back(s);
    out.patches[s.id] = s.text;
  }
  // Also catch <Entry Key= Value=
  std::regex re2(R"re(Key="([^"]+)".*?Value="([^"]*)")re", std::regex::icase);
  begin = std::sregex_iterator(content.begin(), content.end(), re2);
  for (auto it = begin; it != end; ++it) {
    out.patches[(*it)[1]] = (*it)[2];
  }
  return true;
}

TeaDocument make_title_menu() {
  TeaDocument doc;
  doc.menu = {
      {"new_game", "New Game"},
      {"continue", "Continue"},
      {"settings", "Settings"},
      {"quit", "Quit"},
  };
  return doc;
}

}  // namespace cath
