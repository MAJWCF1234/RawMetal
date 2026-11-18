  // ---- Connect: join a loose thing to another, a wall or the ceiling (JOINT; the engine's Vectra joints) ----
  // Pick how in plain words on the right, then drag from the thing to what it joins (or click the thing, then
  // click what it joins). While the drag is held the 3D view shows the joint as the game will play it (the
  // drag preview), and the plan shows a dashed line. One JOINT record is written, its options as named fields
  // with sensible defaults (the inspector lists them, the numbers behind Advanced). "Hanging lamp" needs no
  // thing: click under a ceiling and a caged lamp hangs there on a chain, its light swinging with it.
  HWND connectKindList = nullptr;
  bool connectPending = false;
  float connectFromX = 0, connectFromY = 0;
  enum ConnectKind { ConnectRope, ConnectChain, ConnectHinge, ConnectSpring, ConnectWeld, ConnectMotor, ConnectPulley, ConnectLamp, ConnectKindCount };
  int connectKind() const { return std::clamp(listIndex(connectKindList), 0, int(ConnectKindCount) - 1); }
  // A loose model prop (CLUTTER kind 7) under (x, y): its record line, or -1.
  int connectPropAt(float x, float y, int except = -1) const {
    int best = -1;
    float least = 1e9f;
    for (auto i : doc.records("CLUTTER", map)) {
      if (int(i) == except || !activeFloorRecord(i))
        continue;
      auto f = NativeDocument::fields(doc.lines[i]);
      auto b = creationBox(f);
      if (!b)
        continue;
      const float c = std::cos(b->yaw), s = std::sin(b->yaw), vx = x - b->x, vy = y - b->y, px = vx * c + vy * s, py = -vx * s + vy * c;
      const float dx = std::max(0.f, std::abs(px) - b->w * .5f), dy = std::max(0.f, std::abs(py) - b->d * .5f), d = std::hypot(dx, dy);
      if (d < .25f && d + std::hypot(vx, vy) * .01f < least)
        least = d + std::hypot(vx, vy) * .01f, best = int(i);
    }
    return best;
  }
  int clutterOrdinal(int line) const {
    auto all = doc.records("CLUTTER", map);
    return int(std::find(all.begin(), all.end(), size_t(line)) - all.begin());
  }
  // The JOINT record for a drag from (ax, ay) to (bx, by): false (and a message) when it joins nothing.
  bool connectFields(std::vector<std::string> &f, float ax, float ay, float bx, float by, float floorZ) {
    auto n = [](float v) { return nativeNumber(v); };
    const int kind = connectKind();
    const float ceiling = creationCeiling();
    doc.ensureFormat2();
    if (kind == ConnectLamp) {
      // A caged lamp on a chain from the ceiling over the click, its light riding it.
      const float drop = std::clamp((ceiling - floorZ) * .35f, .6f, 1.6f), top = ceiling - drop;
      const int slot = retro::model::mine_cage_lamp;
      const auto size = retro::model::size[slot];
      const std::string m = std::to_string(map);
      addAuthored({"CLUTTER", m, "7", n(bx), n(by), n(top - size[2]), "0", std::to_string(slot), n(size[0]), n(size[1]), n(size[2]), "0", "0", "", "mass=1.5"});
      int lamp = -1;
      {auto all = doc.records("CLUTTER", map);
       for (int k = int(all.size()) - 1; k >= 0 && lamp < 0; --k) {auto g = NativeDocument::fields(doc.lines[all[size_t(k)]]);
        if (g.size() > 7 && g[3] == n(bx) && g[4] == n(by) && g[7] == std::to_string(slot)) lamp = k;}}
      if (lamp < 0) return false;
      addAuthored({"LIGHT", m, n(bx), n(by), n(top - size[2] * .55f), "2", "0", "1.2", "7", "2700", "0", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "1", std::to_string(lamp)});
      f = {"JOINT", m, "chain", std::to_string(lamp), "-1", n(bx), n(by), n(top), n(bx), n(by), n(ceiling)};
      return true;
    }
    // A click on a thing starts a connection; the next click finishes it.
    if (std::hypot(bx - ax, by - ay) < .05f && !ghosting) {
      if (!connectPending) {
        if (connectPropAt(ax, ay) < 0) {
          message(L"Click a loose thing first (a crate, a plank, a lamp), then what it joins.");
          return false;
        }
        connectPending = true, connectFromX = ax, connectFromY = ay;
        message(L"Now click what it joins: another thing, a wall, or anywhere under the ceiling.");
        return false;
      }
      ax = connectFromX, ay = connectFromY, connectPending = false;
    }
    const int a = connectPropAt(ax, ay);
    if (a < 0) {
      message(L"Start on a loose thing (a crate, a plank, a lamp) and drag to what it joins.");
      return false;
    }
    const int b = connectPropAt(bx, by, a);
    const auto A = *creationBox(NativeDocument::fields(doc.lines[size_t(a)]));
    std::optional<creation::Box> B;
    if (b >= 0)
      B = creationBox(NativeDocument::fields(doc.lines[size_t(b)]));
    const std::string m = std::to_string(map), propA = std::to_string(clutterOrdinal(a)), propB = b >= 0 ? std::to_string(clutterOrdinal(b)) : "-1";
    std::vector<std::pair<std::string, std::string>> named;
    auto top = [](const creation::Box &x) { return x.top; };
    if (kind == ConnectRope || kind == ConnectChain || kind == ConnectSpring) {
      // From the thing's top to what it hangs from: the other thing (its underside if it is above), or the ceiling.
      float tx = bx, ty = by, tz = ceiling;
      if (B)
        tx = B->x, ty = B->y, tz = B->bottom >= top(A) ? B->bottom : top(*B);
      f = {"JOINT", m, kind == ConnectRope ? "rope" : kind == ConnectChain ? "chain" : "spring", propA, propB, n(A.x), n(A.y), n(top(A)), n(tx), n(ty), n(tz)};
      if (kind == ConnectSpring)
        named.push_back({"stiffness", "2"});
    } else if (kind == ConnectPulley) {
      if (!B) {
        message(L"A pulley joins two things: drag from one to the other. Each hangs from a wheel in the ceiling over it.");
        return false;
      }
      f = {"JOINT", m, "pulley", propA, propB, n(A.x), n(A.y), n(top(A)), n(B->x), n(B->y), n(top(*B))};
      named.push_back({"over", n(A.x) + "," + n(A.y) + "," + n(ceiling - .15f)});
      named.push_back({"over2", n(B->x) + "," + n(B->y) + "," + n(ceiling - .15f)});
    } else if (kind == ConnectWeld) {
      const float wx = B ? (A.x + B->x) * .5f : A.x, wy = B ? (A.y + B->y) * .5f : A.y;
      f = {"JOINT", m, "weld", propA, propB, n(wx), n(wy), n((A.bottom + top(A)) * .5f)};
    } else {
      // A hinge (or a motor, a hinge that turns by itself). A thing taller than it is wide (a gate, a door
      // panel) turns about an upright pin at its edge nearest the drag's end; a flat one (a hatch, a plank)
      // about a level pin: under it where the drag ends inside it (a seesaw), else along its nearest edge.
      const float c = std::cos(A.yaw), s = std::sin(A.yaw), vx = bx - A.x, vy = by - A.y, px = vx * c + vy * s, py = -vx * s + vy * c;
      const bool upright = (top(A) - A.bottom) > std::max(A.w, A.d) * .8f;
      auto world = [&](float lx, float ly) { return std::pair{A.x + lx * c - ly * s, A.y + lx * s + ly * c}; };
      float hx = A.x, hy = A.y, hz = A.bottom;
      std::string axis = "z";
      const bool inside = std::abs(px) <= A.w * .5f && std::abs(py) <= A.d * .5f;
      if (kind == ConnectMotor) {
        axis = "z", hz = A.bottom;
        named.push_back({"motor", "speed"}), named.push_back({"speed", "45"}), named.push_back({"force", "200"});
      } else if (upright) {
        // The pin at the end of its long side nearest the pointer, from its foot to its top.
        const bool alongX = A.w >= A.d;
        const float edge = alongX ? (px >= 0 ? A.w * .5f : -A.w * .5f) : (py >= 0 ? A.d * .5f : -A.d * .5f);
        auto [wx, wy] = alongX ? world(edge, 0) : world(0, edge);
        hx = wx, hy = wy, hz = (A.bottom + top(A)) * .5f, axis = "z";
        named.push_back({"min", "-100"}), named.push_back({"max", "100"});
      } else if (inside) {
        // A seesaw: the pin under it at the drag's end, across its length.
        hx = bx, hy = by, hz = A.bottom;
        const bool alongX = A.w >= A.d;
        const float ax2 = alongX ? -s : c, ay2 = alongX ? c : s;
        axis = n(ax2 + 0.f) + "," + n(ay2 + 0.f) + ",0";
        named.push_back({"min", "-20"}), named.push_back({"max", "20"});
      } else {
        // A hatch: the pin along its nearest edge, at its foot.
        const bool sideX = std::abs(px) / std::max(.01f, A.w) >= std::abs(py) / std::max(.01f, A.d);
        auto [wx, wy] = sideX ? world(px >= 0 ? A.w * .5f : -A.w * .5f, 0) : world(0, py >= 0 ? A.d * .5f : -A.d * .5f);
        hx = wx, hy = wy, hz = A.bottom;
        const float ex = sideX ? -s : c, ey = sideX ? c : s;
        axis = n(ex + 0.f) + "," + n(ey + 0.f) + ",0";
        named.push_back({"min", "-110"}), named.push_back({"max", "110"});
      }
      f = {"JOINT", m, "hinge", propA, propB, n(hx), n(hy), n(hz)};
      named.insert(named.begin(), {"axis", axis});
    }
    for (auto &[k, v] : named)
      f.push_back(k + "=" + v);
    return true;
  }
  void connectControlChanged(int id, int notification) {
    if (notification != CBN_SELCHANGE || id != ConnectKindList)
      return;
    connectPending = false;
    const wchar_t *how[] = {L"Rope: drag from a thing to the ceiling (or to another thing) and it hangs there; shoot the rope and it drops.",
                            L"Chain: the same as a rope, drawn as links.",
                            L"Hinge: drag from a gate to where its post is, from a hatch to its edge, or across a plank to where it balances.",
                            L"Spring: it hangs and bounces back.",
                            L"Weld: held fast to the other thing (or the wall) until it is hit hard enough.",
                            L"Motor: a hinge that turns by itself, like a fan or a turntable.",
                            L"Pulley: drag from one thing to another; as one goes down the other comes up.",
                            L"Hanging lamp: click under a ceiling and a lamp hangs there on a chain. Shoot it and it swings."};
    message(how[connectKind()]);
    relayout();
  }
  template <class Row> void connectRows(Row &&row) { row(connectKindList, L"Join with"); }
  std::vector<HWND> connectControls() const { return {connectKindList}; }
  template <class Choices> void makeConnectControls(Choices &&choices) {
    connectKindList = choices(ConnectKindList, {L"Rope", L"Chain", L"Hinge", L"Spring", L"Weld", L"Motor", L"Pulley", L"Hanging lamp"}, 0);
  }
