  // ---- Hang cloth: a curtain, a strap or a cable (CLOTH; the engine's src/game/Cloth.cpp) ----
  // Pick what on the right, then: a curtain, drag along where its top hangs (it hangs from the ceiling down to just over
  // the floor, or as far as picked); a strap, click where it hangs from; a cable, drag from one end to the other (it sags
  // between them); a loose cable, drag from where it comes out of the wall to where its free end lies. The engine shows it
  // hanging while the button is held (the 3D view's ghost), and it sways, parts and drapes as the game plays it.
  HWND clothKindList = nullptr, clothDropList = nullptr;
  static constexpr const char *clothWords[] = {"curtain", "strap", "cable", "cable"};
  // The record's fields from the gesture and the tool boxes.
  bool clothFields(std::vector<std::string> &f, const Schema &s, float ax, float ay, float bx, float by, float floorZ) {
    auto set = [&](const char *name, const std::string &value) {
      auto it = std::find(s.fields.begin(), s.fields.end(), name);
      if (it == s.fields.end())
        return;
      size_t i = size_t(it - s.fields.begin());
      if (f.size() <= i)
        f.resize(i + 1);
      f[i] = value;
    };
    const int kind = std::clamp(listIndex(clothKindList), 0, 3);
    const float top = creationCeiling() - .03f, room = std::max(.3f, top - floorZ);
    const float span = std::hypot(bx - ax, by - ay);
    if ((kind == 0 || kind == 2) && span < .25f) {
      message(kind == 0 ? L"A curtain hangs along a line: drag along where its top goes." : L"A cable hangs between two ends: drag from one to the other.");
      return false;
    }
    if (kind == 3 && span < .25f) {
      message(L"A loose cable: drag from where it comes out of the wall to where its end lies.");
      return false;
    }
    auto n = [](float v) { return nativeNumber(v); };
    std::vector<std::pair<std::string, std::string>> named;
    set("kind", clothWords[kind]);
    if (kind == 1) {
      // A strap from the ceiling where clicked (or the drag's start), its face turned across the drag if there was one.
      set("x", n(ax)), set("y", n(ay)), set("z", n(top));
      if (span >= .25f)
        set("x2", n(bx)), set("y2", n(by)), set("z2", n(top));
      static constexpr float lengths[] = {1.f, .6f, 1.5f, 2.2f};
      named.push_back({"length", shortNumber(std::min(lengths[std::clamp(listIndex(clothDropList), 0, 3)], room - .1f))});
    } else if (kind == 3) {
      // Out of the wall at waist height, its far end on the floor where the drag ends; long enough to reach and lie on.
      const float z = floorZ + std::min(1.4f, room * .5f);
      set("x", n(ax)), set("y", n(ay)), set("z", n(z)), set("x2", n(bx)), set("y2", n(by)), set("z2", n(floorZ));
      named.push_back({"length", shortNumber(std::hypot(span, z - floorZ) * 1.2f)});
      named.push_back({"loose", "1"});
    } else {
      set("x", n(ax)), set("y", n(ay)), set("z", n(top)), set("x2", n(bx)), set("y2", n(by)), set("z2", n(top));
      if (kind == 0) {
        // Down to just over the floor, or as far as picked.
        static constexpr float drops[] = {99.f, 1.f, 1.6f, 2.2f};
        named.push_back({"drop", shortNumber(std::clamp(std::min(drops[std::clamp(listIndex(clothDropList), 0, 3)], room - .12f), .3f, 8.f))});
      } else
        named.push_back({"length", shortNumber(span * 1.2f)});
    }
    doc.ensureFormat2();
    for (auto &[k, v] : named)
      f.push_back(k + "=" + v);
    return true;
  }
  void clothControlChanged(int id, int notification) {
    if (notification != CBN_SELCHANGE)
      return;
    if (id == ClothKindList) {
      const wchar_t *how[] = {L"Curtain: drag along where its top hangs. It hangs from the ceiling and parts when you walk through it.",
                              L"Strap: click where it hangs from. It swings when bumped or shot.",
                              L"Cable: drag from one end to the other. It sags between them and drapes over what is under it.",
                              L"Loose cable: drag from where it comes out of the wall to where its end lies on the floor."};
      message(how[std::clamp(listIndex(clothKindList), 0, 3)]);
    }
    relayout();
  }
  template <class Row> void clothRows(Row &&row) {
    row(clothKindList, L"Hang");
    row(clothDropList, L"How long");
  }
  std::vector<HWND> clothControls() const { return {clothKindList, clothDropList}; }
  template <class Choices> void makeClothControls(Choices &&choices) {
    clothKindList = choices(ClothKindList, {L"Curtain", L"Strap", L"Cable", L"Loose cable"}, 0);
    clothDropList = choices(ClothDropList, {L"Floor length / 1 m", L"Short", L"Medium", L"Long"}, 0);
  }
