#pragma once
#include "cath/puzzle/pzl_map.hpp"

#include <string>

namespace cath {

enum class PuzzleAction { None, MoveN, MoveS, MoveW, MoveE, Pull, Wait };

enum class PuzzleStatus { Playing, Won, Lost };

struct PuzzleWorld {
  PuzzleMap map;
  int px = 0, py = 0, pz = 1;
  int facing = 0;  // 0N 1E 2S 3W
  PuzzleStatus status = PuzzleStatus::Playing;
  int moves = 0;
  std::string name;

  void reset_from(const PuzzleMap& m);
  bool step(PuzzleAction action);
  bool on_goal() const;
  // Height of top solid block at x,y (or -1).
  int top_z(int x, int y) const;
};

}  // namespace cath
