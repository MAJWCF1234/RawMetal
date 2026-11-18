  // ---- The Shape tool (PRIM records; the kinds are src/world/PrimitiveKinds.h) ----
  // The whole palette in plain words and pictures; a click places a shape at its usual size, a
  // drag sizes it with a ghost and soft snapping to sizes a player reads as right; R turns it.
  // Picked in 3D, a shape shows handles: its four sides stretch it, its top raises it, the cyan
  // handle in front lifts it off the floor, the orange one above it turns it a quarter at a time;
  // an arrow on its top shows the way it faces. Each drag is one undo step.
  int primFacing = 0;  // the facing the next shape takes (R turns it)
  int primHandle = -1; // the 3D handle in hand: 0 +u side, 1 -u, 2 front (+v), 3 back, 4 top, 5 lift, 6 turn
  float primGrab = 0;  // where on the handle it was taken (metres along its axis, or the height)
  static const char *primKindName(int k) {
    return retro::prim::kinds[size_t(std::clamp(k, 0, retro::prim::kindCount - 1))].name.data();
  }
  static int primKindIndex(const std::string &name) { return std::max(0, retro::prim::indexOf(name)); }
  // Sizes snap softly: near a size a player reads as right (a door, a storey, a rail, a tread) it
  // takes that size; elsewhere 5 cm steps.
  static float primSnap(float v) {
    for (float s : {.3f, .4f, .6f, .9f, 1.1f, 1.2f, 1.5f, 1.8f, 2.2f, 2.4f, 3.f, 3.4f, 3.6f, 4.f, 4.5f, 6.f})
      if (std::abs(v - s) <= .08f)
        return s;
    return std::max(.1f, std::round(v / .05f) * .05f);
  }
  static bool primRecord(const std::vector<std::string> &f) { return f.size() > 10 && f[0] == "PRIM"; }
  static retro::prim::Size primSize(const std::vector<std::string> &f) {
    return retro::prim::resolve(f[2], number(f[6], 0), number(f[7], 0), number(f[8], 0));
  }
  // A shape's 0 sizes ("the usual") written out, so a handle has a size to stretch.
  static void primMaterialize(std::vector<std::string> &f) {
    auto s = primSize(f);
    for (auto [i, v] : {std::pair{size_t(6), s.w}, {size_t(7), s.d}, {size_t(8), s.h}})
      if (number(f[i], 0) <= 0)
        f[i] = trimmed(v);
  }
  static std::wstring primDims(float w, float d, float h) {
    wchar_t text[96];
    swprintf(text, 96, L"Width %.2f m  ·  Depth %.2f m  ·  Height %.2f m", w, d, h);
    return text;
  }
  // Where a click or a drag from (ax, ay) to (bx, by) puts the next shape.
  struct ShapePlan {
    int kind = 0, facing = 0;
    float x = 0, y = 0, w = 0, d = 0, h = 0;
    bool sized = false; // dragged to a size (else the usual size, written as 0)
  };
  ShapePlan shapePlacement(float ax, float ay, float bx, float by) const {
    ShapePlan p;
    p.kind = std::clamp(listIndex(primList), 0, retro::prim::kindCount - 1);
    p.facing = primFacing & 3;
    auto usual = retro::prim::resolve(retro::prim::kinds[size_t(p.kind)].name, 0, 0, 0);
    p.w = usual.w, p.d = usual.d, p.h = usual.h;
    if (std::hypot(bx - ax, by - ay) < .3f) {
      p.x = bx, p.y = by;
      return p;
    }
    // Dragged: the corner pressed stays put; across and along swap when it faces east or west.
    float ex = primSnap(std::abs(bx - ax)), ey = primSnap(std::abs(by - ay));
    p.sized = true;
    p.w = p.facing % 2 ? ey : ex, p.d = p.facing % 2 ? ex : ey;
    p.x = ax + (bx >= ax ? 1.f : -1.f) * ex * .5f;
    p.y = ay + (by >= ay ? 1.f : -1.f) * ey * .5f;
    return p;
  }
  // The record's fields for the shape planned (authorKind's PRIM branch).
  template <class Set> void authorShape(Set set, float ax, float ay, float bx, float by, float z) {
    const auto p = shapePlacement(ax, ay, bx, by);
    set("kind", primKindName(p.kind));
    set("x", nativeNumber(p.x)), set("y", nativeNumber(p.y)), set("z", nativeNumber(z));
    set("w", p.sized ? trimmed(p.w) : "0"), set("d", p.sized ? trimmed(p.d) : "0"), set("h", "0");
    set("facing", std::to_string(p.facing));
    if (p.sized && !quiet())
      message(std::wstring(wide(retro::prim::kinds[size_t(p.kind)].label)) + L": " + primDims(p.w, p.d, p.h));
  }
  // The footprint a shape covers on the plan, turned the way it faces (PrimitiveKinds.h outline).
  static std::vector<std::pair<float, float>> primOutline(const std::string &kind, float x, float y, int facing, float w, float d) {
    std::vector<std::pair<float, float>> out;
    for (auto [u, v] : retro::prim::outline(kind, w, d))
      out.push_back(retro::prim::toPlan(x, y, facing, u, v));
    return out;
  }
  // Picking on the plan: inside the shape's real turned outline, or within 0.3 m of it.
  bool primHit(const std::vector<std::string> &f, float px, float py) const {
    const auto s = primSize(f);
    const int facing = int(number(f[9], 0)) & 3;
    const auto [fx, fy] = retro::prim::forward(facing);
    const float dx = px - number(f[3], 0), dy = py - number(f[4], 0);
    const float u = dx * fy - dy * fx, v = dx * fx + dy * fy;
    return retro::prim::insideOutline(retro::prim::outline(f[2], s.w, s.d), u, v, .3f);
  }
  // ---- on the plan ----
  void drawPrimShape(HDC dc, float ox, float oy, float cell, const std::string &kind, float x, float y, int facing, float w, float d, COLORREF colour,
                     int style, bool dims, float h) {
    auto pts = primOutline(kind, x, y, facing, w, d);
    std::vector<POINT> poly;
    for (auto [px, py] : pts)
      poly.push_back({int(ox + px * cell), int(oy + py * cell)});
    poly.push_back(poly.front());
    auto pen = style == PS_SOLID ? CreatePen(PS_SOLID, 2, colour) : ui::dashedPen(2, colour);
    auto old = SelectObject(dc, pen);
    Polyline(dc, poly.data(), int(poly.size()));
    // The facing arrow: from the middle to the side it faces, with its head.
    const auto c = retro::prim::toPlan(x, y, facing, 0, 0), tip = retro::prim::toPlan(x, y, facing, 0, d * .5f + .25f);
    const auto l = retro::prim::toPlan(x, y, facing, -.18f, d * .5f + .02f), r = retro::prim::toPlan(x, y, facing, .18f, d * .5f + .02f);
    auto at = [&](std::pair<float, float> p) { return POINT{int(ox + p.first * cell), int(oy + p.second * cell)}; };
    POINT shaft[2] = {at(c), at(tip)}, head[3] = {at(l), at(tip), at(r)};
    Polyline(dc, shaft, 2);
    Polyline(dc, head, 3);
    SelectObject(dc, old);
    DeleteObject(pen);
    if (dims) {
      wchar_t text[64];
      swprintf(text, 64, L"%.2f × %.2f m, %.2f high", w, d, h);
      SetTextColor(dc, colour);
      SetBkMode(dc, TRANSPARENT);
      float right = -1e9f, top = 1e9f;
      for (auto [px, py] : pts)
        right = std::max(right, px), top = std::min(top, py);
      TextOutW(dc, int(ox + right * cell) + 6, int(oy + top * cell) - 4, text, int(wcslen(text)));
    }
  }
  // The ghost of the shape a click or drag would place, sizes and the way it faces written by it.
  void drawShapeGhost(HDC dc, float ox, float oy, float cell, bool stretching, float hx, float hy, COLORREF colour) {
    const float ax = stretching ? snapped(dragFX) : snapped(hx), ay = stretching ? snapped(dragFY) : snapped(hy);
    const auto p = shapePlacement(ax, ay, snapped(hx), snapped(hy));
    drawPrimShape(dc, ox, oy, cell, std::string(retro::prim::kinds[size_t(p.kind)].name), p.x, p.y, p.facing, p.w, p.d, colour, PS_DASH, true, p.h);
  }
  // A picked shape on the plan: its real outline and the way it faces.
  void drawShapeSelection(HDC dc, float ox, float oy, float cell) {
    if (record < 0 || size_t(record) >= doc.lines.size() || selectedRecords.size() > 1)
      return;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (!primRecord(f))
      return;
    const auto s = primSize(f);
    drawPrimShape(dc, ox, oy, cell, f[2], number(f[3], 0), number(f[4], 0), int(number(f[9], 0)) & 3, s.w, s.d, ui::Selected, PS_SOLID,
                  objectDrag, s.h);
  }
  // ---- in 3D ----
  struct PrimHandle {
    int id;
    float x, y, z;
  };
  std::vector<PrimHandle> primHandles(const std::vector<std::string> &f) const {
    const auto s = primSize(f);
    const float x = number(f[3], 0), y = number(f[4], 0), z = number(f[5], 0) + liftOf(f), hw = s.w * .5f, hd = s.d * .5f;
    const int facing = int(number(f[9], 0)) & 3;
    auto at = [&](int id, float u, float v, float zz) {
      auto p = retro::prim::toPlan(x, y, facing, u, v);
      return PrimHandle{id, p.first, p.second, zz};
    };
    return {at(0, hw, 0, z + s.h * .5f), at(1, -hw, 0, z + s.h * .5f), at(2, 0, hd, z + s.h * .5f), at(3, 0, -hd, z + s.h * .5f),
            at(4, 0, 0, z + s.h),     at(5, 0, hd + .45f, z),        at(6, 0, hd + .45f, z + s.h + .3f)};
  }
  // The picked shape's outline, handles and facing arrow (selectionGuides), in the engine's guide
  // kinds: 0 lines (the outline, the arrow, the stems), 1 the top handle (yellow), 2 the lift handle
  // (cyan), 3 the side handles (green), 4 the turn handle's blue ring.
  bool primGuides(std::vector<retro::EditorGuide> &out) const {
    if (record < 0 || size_t(record) >= doc.lines.size())
      return false;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (!primRecord(f))
      return false;
    const auto s = primSize(f);
    const float x = number(f[3], 0), y = number(f[4], 0), z = number(f[5], 0) + liftOf(f);
    const int facing = int(number(f[9], 0)) & 3;
    auto pts = primOutline(f[2], x, y, facing, s.w, s.d);
    for (size_t i = 0; i < pts.size(); ++i) {
      auto a = pts[i], b = pts[(i + 1) % pts.size()];
      out.push_back({a.first, a.second, z + s.h, b.first, b.second, z + s.h, 0});
      out.push_back({a.first, a.second, z, b.first, b.second, z, 0});
      if (pts.size() <= 8 || i % 4 == 0)
        out.push_back({a.first, a.second, z, a.first, a.second, z + s.h, 0});
    }
    const float top = z + s.h + .02f;
    auto c = retro::prim::toPlan(x, y, facing, 0, 0), tip = retro::prim::toPlan(x, y, facing, 0, s.d * .5f);
    auto l = retro::prim::toPlan(x, y, facing, -.2f, s.d * .5f - .3f), r = retro::prim::toPlan(x, y, facing, .2f, s.d * .5f - .3f);
    out.push_back({c.first, c.second, top, tip.first, tip.second, top, 0});
    out.push_back({l.first, l.second, top, tip.first, tip.second, top, 0});
    out.push_back({r.first, r.second, top, tip.first, tip.second, top, 0});
    for (const auto &h : primHandles(f)) {
      if (h.id == 6) { // the turn handle: a ring
        for (int k = 0; k < 12; ++k) {
          const float a0 = k * .5236f, a1 = (k + 1) * .5236f;
          out.push_back({h.x + .16f * std::cos(a0), h.y + .16f * std::sin(a0), h.z, h.x + .16f * std::cos(a1), h.y + .16f * std::sin(a1), h.z, 4});
        }
        continue;
      }
      const int kind = h.id < 4 ? 3 : h.id == 4 ? 1 : 2;
      out.push_back({h.x, h.y, h.z, h.x, h.y, h.z, kind});
    }
    // The turn handle hangs off the top's front edge on a stem; the lift handle off the foot.
    const auto &hs = primHandles(f);
    auto front = retro::prim::toPlan(x, y, facing, 0, s.d * .5f);
    out.push_back({front.first, front.second, z + s.h, hs[6].x, hs[6].y, hs[6].z, 0});
    out.push_back({front.first, front.second, z, hs[5].x, hs[5].y, hs[5].z, 0});
    return true;
  }
  // A press on one of the picked shape's handles starts its drag.
  bool beginPrimHandle3D(float ox, float oy, float oz, float dx, float dy, float dz) {
    if (tool != 0 || record < 0 || size_t(record) >= doc.lines.size() || selectedRecords.size() > 1)
      return false;
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (!primRecord(f) || !editableRecord(size_t(record)))
      return false;
    const float a = dx * dx + dy * dy + dz * dz;
    if (a < 1e-8f)
      return false;
    int best = -1;
    float closest = .045f; // wider than the footprint and height grips, so a shape's own handles win
    PrimHandle chosen{};
    for (const auto &h : primHandles(f)) {
      const float wx = h.x - ox, wy = h.y - oy, wz = h.z - oz, t = (wx * dx + wy * dy + wz * dz) / a;
      if (t <= .05f)
        continue;
      const float ex = ox + dx * t - h.x, ey = oy + dy * t - h.y, ez = oz + dz * t - h.z;
      const float screen = std::sqrt(ex * ex + ey * ey + ez * ez) / (t * std::sqrt(a));
      if (screen < closest)
        closest = screen, best = h.id, chosen = h;
    }
    if (best < 0)
      return false;
    primHandle = best;
    verticalX = chosen.x, verticalY = chosen.y, verticalStart = chosen.z;
    const float len = std::hypot(dx, dy);
    verticalNx = len > 1e-4f ? dx / len : 1.f, verticalNy = len > 1e-4f ? dy / len : 0.f;
    objectDrag = viewDrag = true;
    objectMoved = false;
    objectOriginal = doc.lines[size_t(record)];
    captureDragState(); // before the usual sizes are written out: the drag's undo brings back the 0s
    primMaterialize(f);
    doc.replace(size_t(record), f);
    objectFields = f;
    lastDragPublish = GetTickCount64();
    // Where on its axis it was taken, so the side does not jump to the pointer.
    primGrab = 0;
    if (best < 4) {
      float px, py;
      if (floorPoint(ox, oy, oz, dx, dy, dz, chosen.z, px, py))
        primGrab = primAlong(f, px, py, best) - primEdge(f, best);
    } else if (best == 4 || best == 5) {
      float zz;
      if (verticalRay(ox, oy, oz, dx, dy, dz, zz))
        primGrab = zz;
    }
    message(primDims(primSize(f).w, primSize(f).d, primSize(f).h));
    return true;
  }
  // A plan point's distance along a side handle's axis (u for 0 and 1, v for 2 and 3), and that side's edge.
  static float primAlong(const std::vector<std::string> &f, float px, float py, int handle) {
    const int facing = int(number(f[9], 0)) & 3;
    const auto [fx, fy] = retro::prim::forward(facing);
    const float dx = px - number(f[3], 0), dy = py - number(f[4], 0);
    return handle < 2 ? dx * fy - dy * fx : dx * fx + dy * fy;
  }
  static float primEdge(const std::vector<std::string> &f, int handle) {
    const auto s = primSize(f);
    return handle == 0 ? s.w * .5f : handle == 1 ? -s.w * .5f : handle == 2 ? s.d * .5f : -s.d * .5f;
  }
  void applyPrimHandle3D(float ox, float oy, float oz, float dx, float dy, float dz) {
    auto f = objectFields;
    const auto s0 = primSize(objectFields);
    const int facing = int(number(f[9], 0)) & 3;
    const float x0 = number(f[3], 0), y0 = number(f[4], 0);
    if (primHandle < 4) {
      float px, py;
      if (!floorPoint(ox, oy, oz, dx, dy, dz, verticalStart, px, py))
        return;
      const float edge = primAlong(objectFields, px, py, primHandle) - primGrab, opposite = -primEdge(objectFields, primHandle);
      const float size = primSnap(std::max(.1f, std::abs(edge - opposite))), middle = opposite + (primHandle % 2 ? -1.f : 1.f) * size * .5f;
      const auto c = primHandle < 2 ? retro::prim::toPlan(x0, y0, facing, middle, 0) : retro::prim::toPlan(x0, y0, facing, 0, middle);
      f[primHandle < 2 ? 6 : 7] = trimmed(size);
      f[3] = nativeNumber(c.first), f[4] = nativeNumber(c.second);
    } else if (primHandle == 4 || primHandle == 5) {
      float zz;
      if (!verticalRay(ox, oy, oz, dx, dy, dz, zz))
        return;
      if (primHandle == 4)
        f[8] = trimmed(primSnap(std::max(.1f, s0.h + zz - primGrab)));
      else {
        float z = number(objectFields[5], 0) + zz - primGrab;
        z = std::abs(z) < .08f ? 0.f : std::round(z / .05f) * .05f;
        f[5] = trimmed(z);
      }
    } else {
      float px, py;
      if (!floorPoint(ox, oy, oz, dx, dy, dz, verticalStart, px, py))
        return;
      int best = facing;
      float most = -1e9f;
      for (int q = 0; q < 4; ++q) {
        const auto [fx, fy] = retro::prim::forward(q);
        if (const float along = (px - x0) * fx + (py - y0) * fy; along > most)
          most = along, best = q;
      }
      f[9] = std::to_string(best);
    }
    if (f != NativeDocument::fields(doc.lines[size_t(record)])) {
      doc.replace(size_t(record), f);
      objectMoved = true;
      updateInspector();
      InvalidateRect(plan, nullptr, FALSE);
    }
    const auto s = primSize(f);
    message(primHandle == 6 ? L"Faces " + wide(retro::prim::facingWord(int(number(f[9], 0)))) + L"  ·  " + primDims(s.w, s.d, s.h)
            : primHandle == 5 ? L"Raised " + wide(trimmed(number(f[5], 0))) + L" m off the floor  ·  " + primDims(s.w, s.d, s.h)
                              : primDims(s.w, s.d, s.h));
  }
