#include "cath/puzzle/puzzle_world.hpp"

namespace cath {
namespace {

void dir_delta(int facing, int& dx, int& dy) {
  static const int DX[4] = {0, 1, 0, -1};
  static const int DY[4] = {-1, 0, 1, 0};
  dx = DX[facing & 3];
  dy = DY[facing & 3];
}

}  // namespace

void PuzzleWorld::reset_from(const PuzzleMap& m) {
  map = m;
  px = m.start_x;
  py = m.start_y;
  pz = m.start_z;
  facing = m.facing % 4;
  status = PuzzleStatus::Playing;
  moves = 0;
  name = m.source;
  // Stand on top of floor under start.
  const int top = top_z(px, py);
  if (top >= 0) {
    pz = top + 1;
  }
}

int PuzzleWorld::top_z(int x, int y) const {
  for (int z = int(map.cells.size()) - 1; z >= 0; --z) {
    if (map.get(x, y, z) != BlockType::Empty) {
      return z;
    }
  }
  return -1;
}

bool PuzzleWorld::on_goal() const {
  const int under = pz - 1;
  if (under < 0) {
    return false;
  }
  return map.get(px, py, under) == BlockType::Goal;
}

bool PuzzleWorld::step(PuzzleAction action) {
  if (status != PuzzleStatus::Playing) {
    return false;
  }
  if (action == PuzzleAction::None || action == PuzzleAction::Wait) {
    return true;
  }
  if (action == PuzzleAction::MoveN) {
    facing = 0;
  } else if (action == PuzzleAction::MoveE) {
    facing = 1;
  } else if (action == PuzzleAction::MoveS) {
    facing = 2;
  } else if (action == PuzzleAction::MoveW) {
    facing = 3;
  }

  int dx = 0, dy = 0;
  dir_delta(facing, dx, dy);
  const int nx = px + dx;
  const int ny = py + dy;
  if (nx < 0 || ny < 0 || nx >= map.width || ny >= map.height) {
    return false;
  }

  const int dest_top = top_z(nx, ny);
  const int cur_under = pz - 1;

  // Climb up one, walk same height, or step down any amount (Catherine-like).
  if (dest_top < 0) {
    // empty column — fall / death
    status = PuzzleStatus::Lost;
    return true;
  }
  const int dest_stand = dest_top + 1;
  const int climb = dest_stand - pz;
  if (climb > 1) {
    // Too high — try push block at face height if Pull not held / push when blocked
    const BlockType face = map.get(nx, ny, pz);
    if (face == BlockType::Solid || face == BlockType::Ice) {
      const int bx = nx + dx;
      const int by = ny + dy;
      if (bx >= 0 && by >= 0 && bx < map.width && by < map.height && map.get(bx, by, pz) == BlockType::Empty &&
          top_z(bx, by) == pz - 1) {
        map.set(bx, by, pz, face);
        map.set(nx, ny, pz, BlockType::Empty);
        // after push, space free — fall through to walk logic
      } else {
        return false;
      }
    } else {
      return false;
    }
  }

  // Recompute after possible push
  const int dest_top2 = top_z(nx, ny);
  if (dest_top2 < 0) {
    status = PuzzleStatus::Lost;
    return true;
  }
  const int stand = dest_top2 + 1;
  if (stand - pz > 1) {
    return false;
  }

  if (action == PuzzleAction::Pull) {
    // Pull block from behind into current cell's facing neighbor under feet level
    const int bx = px - dx;
    const int by = py - dy;
    const BlockType b = map.get(bx, by, cur_under + 1);
    if ((b == BlockType::Solid || b == BlockType::Ice) && map.get(px, py, cur_under + 1) == BlockType::Empty) {
      map.set(px, py, cur_under + 1, b);
      map.set(bx, by, cur_under + 1, BlockType::Empty);
    }
  }

  px = nx;
  py = ny;
  pz = stand;
  ++moves;

  if (on_goal()) {
    status = PuzzleStatus::Won;
  }
  // Fall off bottom
  if (pz <= 0) {
    status = PuzzleStatus::Lost;
  }
  return true;
}

}  // namespace cath
