#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

// Native geometry and ownership stay with the caller. This router considers
// the complete two-cell passage and the one-cell ring needed for its walls.
namespace corridor {
struct Cell {
  int cost = 10;
  bool blocked = false;
  bool wallProtected = false;
  bool buildWalls = true;
  int room = -1;
};
using Grid = std::array<Cell, 576>;
struct Route {
  std::vector<int> cells;
  std::vector<int> anchors; // Upper-left cell IDs, ordered along the route.
};

inline Route plan(const Grid& grid, int startCell, int endCell) {
  if (startCell < 0 || startCell >= 576 || endCell < 0 || endCell >= 576)
    return {};
  // Canonical endpoints also make tie-breaking independent of drag direction.
  if (endCell < startCell)
    std::swap(startCell, endCell);
  const int startRoom = grid[size_t(startCell)].room;
  const int endRoom = grid[size_t(endCell)].room;
  if (startRoom < 0 || endRoom < 0 || startRoom == endRoom)
    return {};

  std::array<bool, 576> valid{}, starts{}, ends{};
  std::array<std::int64_t, 576> cost{};
  auto contains = [](int anchor, int cell) {
    const int dx = cell % 24 - anchor % 24;
    const int dy = cell / 24 - anchor / 24;
    return dx >= 0 && dx < 2 && dy >= 0 && dy < 2;
  };
  for (int y = 1; y <= 21; ++y) {
    for (int x = 1; x <= 21; ++x) {
      const int anchor = y * 24 + x;
      bool fits = true, inStart = true, inEnd = true;
      for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
          const auto& cell = grid[size_t((y + dy) * 24 + x + dx)];
          fits = fits && !cell.blocked;
          inStart = inStart && cell.room == startRoom;
          inEnd = inEnd && cell.room == endRoom;
          // A positive cost keeps Dijkstra valid even for malformed input.
          cost[size_t(anchor)] += std::max(1, cell.cost);
        }
      }
      for (int dy = -1; dy <= 2 && fits; ++dy) {
        for (int dx = -1; dx <= 2; ++dx) {
          if (dx >= 0 && dx < 2 && dy >= 0 && dy < 2)
            continue;
          const auto& cell = grid[size_t((y + dy) * 24 + x + dx)];
          if (cell.buildWalls && cell.wallProtected) {
            fits = false;
            break;
          }
        }
      }
      valid[size_t(anchor)] = fits;
      starts[size_t(anchor)] = fits && inStart && contains(anchor, startCell);
      ends[size_t(anchor)] = fits && inEnd && contains(anchor, endCell);
    }
  }

  // Direction 4 is the initial state, which has no incoming turn penalty.
  constexpr int directions = 5;
  constexpr std::int64_t infinity = std::numeric_limits<std::int64_t>::max() / 4;
  constexpr std::int64_t turnPenalty = 24;
  std::array<std::int64_t, 576 * directions> distance;
  std::array<int, 576 * directions> previous;
  distance.fill(infinity);
  previous.fill(-1);
  using Pending = std::pair<std::int64_t, int>;
  std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> pending;
  for (int anchor = 0; anchor < 576; ++anchor) {
    if (!starts[size_t(anchor)])
      continue;
    const int state = anchor * directions + 4;
    distance[size_t(state)] = cost[size_t(anchor)];
    pending.push({distance[size_t(state)], state});
  }

  constexpr int deltaX[] = {1, 0, -1, 0};
  constexpr int deltaY[] = {0, 1, 0, -1};
  int finish = -1;
  while (!pending.empty()) {
    const auto [travel, state] = pending.top();
    pending.pop();
    if (travel != distance[size_t(state)])
      continue;
    const int anchor = state / directions, direction = state % directions;
    if (ends[size_t(anchor)]) {
      finish = state;
      break;
    }
    const int x = anchor % 24, y = anchor / 24;
    for (int nextDirection = 0; nextDirection < 4; ++nextDirection) {
      const int nx = x + deltaX[nextDirection], ny = y + deltaY[nextDirection];
      if (nx < 1 || nx > 21 || ny < 1 || ny > 21)
        continue;
      const int nextAnchor = ny * 24 + nx;
      if (!valid[size_t(nextAnchor)])
        continue;
      const int nextState = nextAnchor * directions + nextDirection;
      const auto nextDistance = travel + cost[size_t(nextAnchor)] +
          (direction != 4 && direction != nextDirection ? turnPenalty : 0);
      if (nextDistance < distance[size_t(nextState)]) {
        distance[size_t(nextState)] = nextDistance;
        previous[size_t(nextState)] = state;
        pending.push({nextDistance, nextState});
      }
    }
  }
  if (finish < 0)
    return {};

  Route route;
  for (int state = finish; state >= 0; state = previous[size_t(state)])
    route.anchors.push_back(state / directions);
  std::reverse(route.anchors.begin(), route.anchors.end());
  std::array<bool, 576> used{};
  for (int anchor : route.anchors) {
    used[size_t(anchor)] = used[size_t(anchor + 1)] = true;
    used[size_t(anchor + 24)] = used[size_t(anchor + 25)] = true;
  }
  for (int cell = 0; cell < 576; ++cell)
    if (used[size_t(cell)])
      route.cells.push_back(cell);
  return route;
}
} // namespace corridor
