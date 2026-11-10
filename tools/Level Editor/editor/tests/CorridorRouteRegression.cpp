#include "../NativeCorridorRoute.h"

#include <iostream>
#include <stdexcept>
#include <string>

static void check(bool condition, const char* message) {
  if (!condition)
    throw std::runtime_error(message);
}
static int cell(int x, int y) { return y * 24 + x; }
static void room(corridor::Grid& grid, int id, int x0, int y0, int x1, int y1) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x)
      grid[size_t(cell(x, y))].room = id;
}
static void verify(const corridor::Grid& grid, const corridor::Route& route, int a, int b) {
  check(!route.cells.empty() && !route.anchors.empty(), "Expected a nonempty route");
  check(std::is_sorted(route.cells.begin(), route.cells.end()), "Route cells are not sorted");
  check(std::adjacent_find(route.cells.begin(), route.cells.end()) == route.cells.end(), "Route cells contain duplicates");
  check(std::binary_search(route.cells.begin(), route.cells.end(), a) &&
        std::binary_search(route.cells.begin(), route.cells.end(), b), "Passage does not reach both picked cells");
  std::array<bool, 576> covered{};
  for (size_t i = 0; i < route.anchors.size(); ++i) {
    const int anchor = route.anchors[i], x = anchor % 24, y = anchor / 24;
    check(x >= 1 && x <= 21 && y >= 1 && y <= 21, "Anchor clips its footprint or wall buffer at the boundary");
    if (i) {
      const int previous = route.anchors[i - 1];
      check(std::abs(x - previous % 24) + std::abs(y - previous / 24) == 1, "Path anchors are not cardinal neighbors");
    }
    for (int dy = 0; dy < 2; ++dy)
      for (int dx = 0; dx < 2; ++dx) {
        const int c = cell(x + dx, y + dy);
        check(!grid[size_t(c)].blocked, "Passage intersects a blocked object");
        check(std::binary_search(route.cells.begin(), route.cells.end(), c), "Passage lost its two-cell width at a bend");
        covered[size_t(c)] = true;
      }
    for (int dy = -1; dy <= 2; ++dy)
      for (int dx = -1; dx <= 2; ++dx) {
        if (dx >= 0 && dx < 2 && dy >= 0 && dy < 2)
          continue;
        const auto& ring = grid[size_t(cell(x + dx, y + dy))];
        check(!(ring.buildWalls && ring.wallProtected), "Wall buffer overlaps protected content");
      }
  }
  for (int c = 0; c < 576; ++c)
    check(covered[size_t(c)] == std::binary_search(route.cells.begin(), route.cells.end(), c), "Returned passage differs from anchor footprints");
  auto endpointAnchor = [&](int anchor, int picked) {
    const int roomId = grid[size_t(picked)].room;
    check(picked % 24 >= anchor % 24 && picked % 24 <= anchor % 24 + 1 &&
          picked / 24 >= anchor / 24 && picked / 24 <= anchor / 24 + 1, "Endpoint anchor misses the picked cell");
    for (int offset : {0, 1, 24, 25})
      check(grid[size_t(anchor + offset)].room == roomId, "Endpoint anchor extends outside its room");
  };
  if (b < a)
    std::swap(a, b);
  endpointAnchor(route.anchors.front(), a);
  endpointAnchor(route.anchors.back(), b);
  const auto reverse = corridor::plan(grid, b, a);
  check(reverse.cells == route.cells && reverse.anchors == route.anchors, "Reversing the drag changes the route");
}

int main() {
  try {
    int scenarios = 0;
    for (int dx : {-1, 1}) {
      for (int dy : {-1, 1}) {
        corridor::Grid grid;
        const int x = dx < 0 ? 4 : 18, y = dy < 0 ? 4 : 18;
        room(grid, 0, 9, 9, 12, 12);
        room(grid, 1, x - 1, y - 1, x + 2, y + 2);
        const int a = cell(10, 10), b = cell(x, y);
        const auto route = corridor::plan(grid, a, b);
        verify(grid, route, a, b);
        bool bent = false;
        for (size_t i = 2; i < route.anchors.size(); ++i)
          bent |= route.anchors[i] - route.anchors[i - 1] != route.anchors[i - 1] - route.anchors[i - 2];
        check(bent, "Diagonal room placement produced no cardinal bend");
        ++scenarios;
      }
    }
    {
      corridor::Grid grid;
      room(grid, 0, 2, 9, 5, 12); room(grid, 1, 18, 9, 21, 12);
      for (int y = 8; y <= 13; ++y)
        for (int x = 10; x <= 13; ++x)
          grid[size_t(cell(x, y))].blocked = true;
      const auto route = corridor::plan(grid, cell(4, 10), cell(19, 10));
      verify(grid, route, cell(4, 10), cell(19, 10));
      bool detour = false;
      for (int c : route.cells) detour |= c / 24 < 8 || c / 24 > 13;
      check(detour, "Router did not detour around the protected object");
      ++scenarios;
      for (int y = 0; y < 24; ++y) grid[size_t(cell(12, y))].blocked = true;
      check(corridor::plan(grid, cell(4, 10), cell(19, 10)).cells.empty(), "Router crossed a complete blocked barrier");
      ++scenarios;
    }
    {
      corridor::Grid grid;
      room(grid, 0, 1, 1, 3, 3); room(grid, 1, 20, 20, 22, 22);
      verify(grid, corridor::plan(grid, cell(1, 1), cell(22, 22)), cell(1, 1), cell(22, 22));
      room(grid, 2, 0, 8, 0, 10);
      check(corridor::plan(grid, cell(0, 9), cell(22, 22)).cells.empty(), "Router clipped a boundary endpoint to make it fit");
      check(corridor::plan(grid, -1, cell(22, 22)).cells.empty() && corridor::plan(grid, 576, cell(22, 22)).cells.empty(), "Router accepted an out-of-bounds endpoint");
      ++scenarios;
    }
    {
      corridor::Grid grid;
      room(grid, 0, 3, 3, 4, 4); room(grid, 1, 17, 3, 18, 4);
      // The source has exactly one valid endpoint footprint. Its north ring
      // touches this protected object although the passage itself would not.
      auto& protectedCell = grid[size_t(cell(3, 2))];
      protectedCell.wallProtected = true;
      check(corridor::plan(grid, cell(3, 3), cell(18, 3)).cells.empty(), "Router ignored protected content in a required wall buffer");
      protectedCell.buildWalls = false;
      verify(grid, corridor::plan(grid, cell(3, 3), cell(18, 3)), cell(3, 3), cell(18, 3));
      ++scenarios;
    }
    {
      corridor::Grid grid;
      room(grid, 0, 3, 7, 4, 8); room(grid, 1, 6, 7, 7, 8);
      for (int y = 0; y < 24; ++y) grid[size_t(cell(5, y))].cost = 30;
      verify(grid, corridor::plan(grid, cell(4, 7), cell(6, 7)), cell(4, 7), cell(6, 7));
      check(corridor::plan(grid, cell(3, 7), cell(4, 8)).cells.empty(), "Router connected two points in the same room");
      check(corridor::plan(grid, cell(4, 7), cell(12, 7)).cells.empty(), "Router accepted an endpoint without a room");
      ++scenarios;
    }
    {
      corridor::Grid grid;
      room(grid, 0, 2, 9, 5, 12); room(grid, 1, 18, 9, 21, 12);
      for (int y = 7; y <= 14; ++y)
        for (int x = 9; x <= 14; ++x) grid[size_t(cell(x, y))].cost = 1000;
      const auto route = corridor::plan(grid, cell(4, 10), cell(19, 10));
      verify(grid, route, cell(4, 10), cell(19, 10));
      for (int c : route.cells) check(grid[size_t(c)].cost < 1000, "Router ignored cell traversal costs");
      ++scenarios;
    }
    std::cout << "PASS: " << scenarios << " corridor routing scenarios; quadrants and reversal; full width at bends; protected-object detours; impossible routes; map boundary; protected wall buffers; nearby rooms; traversal costs\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
