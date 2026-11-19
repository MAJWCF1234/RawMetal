  // ---- Material turns: planks, tiles, brick courses, tread plate and grating run the way they
  // are turned (map format 2: STRUCT and PRIM turn=, STAIR treadturn=, LAYER deckturn=, SURFACE
  // wallturn= floorturn= ceilingturn=; degrees, docs/Systems/map-format.md). The engine turns
  // the texture on the face (Scene3D setTurn), in the game and in this 3D view alike.
  // - The Paint tool lays its paint turned by paintTurn (so do the Floor and Flooring tools). R
  //   turns it a quarter, Shift+R back. A stroke just painted turns with it, so the 3D view shows
  //   the floor turning under the brush; with nothing just painted, the surface under the
  //   pointer shows the turned paint in the 3D view until the pointer moves on (a preview).
  // - The Turn box on the right: the brush's turn with those tools, or a picked surface's own.
  //   Advanced adds a box for any angle.
  float paintTurn = 0;
  std::vector<std::pair<std::string, std::string>> lastPainted; // (its line, turn key) for what the last stroke painted
  std::uint64_t lastPaintVersion = ~0ull;
  HWND turnList = nullptr, turnFineBox = nullptr;
  std::string paintPreviewAt; // where the paint preview stands (the hover it was made for)
  static float wrapTurn(float degrees) {
    if (!std::isfinite(degrees)) return 0;
    degrees = std::fmod(degrees, 360.f);
    if (degrees < 0) degrees += 360.f;
    return degrees > 359.995f ? 0.f : degrees;
  }
  // The field's text: blank (no field) for no turn, so an unturned surface is written as before.
  static std::string turnText(float degrees) {
    degrees = wrapTurn(degrees);
    if (degrees == 0) return {};
    char text[32];
    snprintf(text, sizeof text, "%.2f", double(degrees));
    std::string s = text;
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s;
  }
  static std::wstring turnWords(float degrees) {
    degrees = wrapTurn(degrees);
    if (degrees == 0) return L"as laid";
    if (degrees == 90) return L"a quarter turn";
    if (degrees == 180) return L"half a turn";
    if (degrees == 270) return L"three quarters round";
    return wide(turnText(degrees)) + L"°";
  }
  // The turn key of a record that carries a material: blocks and shapes, stair treads, balcony decks.
  static const char *turnKeyOf(const std::string &kind) {
    return kind == "STRUCT" || kind == "PRIM" ? "turn" : kind == "STAIR" ? "treadturn" : kind == "LAYER" ? "deckturn" : nullptr;
  }
  static float turnOf(const std::string &line, const char *key) {
    const auto value = namedValue(line, key);
    return value.empty() ? 0.f : wrapTurn(number(value, 0));
  }
  // The turn the paint and floor tools lay their material with.
  float layTurn() const { return tool == PaintTool || tool == FlooringTool || tool == 2 ? paintTurn : 0.f; }
  // A record's turn set (blank removes it). Returns where the record is now.
  size_t setTurnField(size_t at, const char *key, float degrees) {
    const auto text = turnText(degrees);
    if (namedValue(doc.lines[at], key) == text) return at;
    return setNamedValue(at, key, text);
  }
  // The paint's turn put on a record this stroke painted, remembered so R can turn it again.
  size_t paintTurned(size_t at, const char *key) {
    at = setTurnField(at, key, paintTurn);
    lastPainted.push_back({doc.lines[at], key});
    return at;
  }
  // Fields for a new STRUCT laid by a paint or floor tool: its turn after its own fields.
  void withLayTurn(std::vector<std::string> &f) {
    const auto text = turnText(layTurn());
    if (text.empty()) return;
    doc.ensureFormat2();
    f.push_back("turn=" + text);
  }
  // The record a just-added line is (the last line with this text).
  int lineOf(const std::string &text) const {
    std::string_view want = text;
    if (!want.empty() && want.back() == '\r') want.remove_suffix(1);
    for (size_t i = doc.lines.size(); i-- > 0;) {
      std::string_view line = doc.lines[i];
      if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
      if (line == want) return int(i);
    }
    return -1;
  }
  void noteLaid(const std::vector<std::string> &f) {
    if (layTurn() == 0 && tool != PaintTool && tool != FlooringTool) return;
    lastPainted.push_back({NativeDocument::join(f), "turn"});
  }
  // A stroke begins: what the last one painted is forgotten.
  void beginPaintStroke() {
    lastPainted.clear();
    // A turn needs FORMAT|2: put it in first, so no record moves under the stroke.
    if (layTurn() != 0) doc.ensureFormat2();
  }
  void endPaintStroke() { lastPaintVersion = doc.version; }
  // R in the Paint, Floor or Flooring tool: the paint turns a quarter (back, with Shift).
  void turnPaint(float by) {
    paintTurn = wrapTurn(std::round((paintTurn + by) / 90.f) * 90.f);
    sfx::play(sfx::Click, .6f);
    // The stroke just painted turns with it (part of the same Undo).
    if (!lastPainted.empty() && lastPaintVersion == doc.version) {
      bool any = false;
      for (auto &[line, key] : lastPainted) {
        const int at = lineOf(line);
        if (at < 0) continue;
        line = doc.lines[setTurnField(size_t(at), key.c_str(), paintTurn)], any = true;
      }
      if (any) {
        publish();
        lastPaintVersion = doc.version;
        showTurn();
        message(L"Turned " + turnWords(paintTurn) + L". R turns it on; Shift+R back; Ctrl+Z takes the paint back.");
        return;
      }
    }
    showTurn();
    previewPaint();
    message(L"The paint is laid " + turnWords(paintTurn) + L" now. R turns it on, Shift+R back.");
  }
  // The surface under the pointer, painted with the turned paint in the 3D view only (a scratch
  // copy, as previewGesture shows a drag): it goes when the pointer moves on or anything changes.
  std::string paintHoverRay; // the 3D view's last hover ray over a surface with the Paint tool
  void previewPaint() {
    if (tool != PaintTool || !engineLive || ghosting) return;
    const bool onPlan = hoverInside;
    if (!onPlan && paintHoverRay.empty()) return;
    auto keep = doc;
    const int keepRecord = record, keepKind = kind, keepTool = tool;
    const auto keepPainted = lastPainted;
    const auto keepVersion = lastPaintVersion;
    ghosting = true;
    try {
      if (onPlan)
        paintGesture(hoverX, hoverY, hoverX, hoverY);
      else {
        std::istringstream in(paintHoverRay);
        std::string tag;
        float ox, oy, oz, dx, dy, dz;
        if (in >> tag >> ox >> oy >> oz >> dx >> dy >> dz) paintRay(ox, oy, oz, dx, dy, dz);
      }
    } catch (...) {
    }
    ghosting = false;
    doc = std::move(keep);
    record = keepRecord, kind = keepKind, tool = keepTool, lastPainted = keepPainted, lastPaintVersion = keepVersion;
    ghostShown = true;
    paintPreviewAt = onPlan ? std::to_string(cellOf(hoverX)) + "," + std::to_string(cellOf(hoverY)) : paintHoverRay;
  }
  // The pointer moved off the previewed surface: the picture shows the document again.
  void paintPointer(const std::string &at) {
    if (!paintPreviewAt.empty() && at != paintPreviewAt) {
      paintPreviewAt.clear();
      dropGhost();
    }
  }
  // ---- The Turn box on the right.
  void createTurnControls() {
    turnList = controlWindow(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, TurnList);
    for (auto *text : {L"As laid (0°)", L"A quarter (90°)", L"Half (180°)", L"Three quarters (270°)"})
      SendMessageW(turnList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    SendMessageW(turnList, CB_SETCURSEL, 0, 0);
    turnFineBox = controlWindow(L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, TurnFineBox);
  }
  // The record whose turn the box shows: the picked block, shape, stair or storey (its balconies).
  int turnRecord() const {
    if (tool != 0 || record < 0 || size_t(record) >= doc.lines.size() || selectedRecords.size() > 1) return -1;
    const auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    return !f.empty() && turnKeyOf(f[0]) ? record : -1;
  }
  bool turnShown() const { return tool == PaintTool || tool == FlooringTool || tool == 2 || turnRecord() >= 0; }
  float shownTurn() const {
    const int at = turnRecord();
    if (at < 0) return paintTurn;
    const auto f = NativeDocument::fields(doc.lines[size_t(at)]);
    return turnOf(doc.lines[size_t(at)], turnKeyOf(f[0]));
  }
  void showTurn() {
    if (!turnList) return;
    const float t = shownTurn();
    const bool quarter = std::fmod(t, 90.f) == 0;
    SendMessageW(turnList, CB_SETCURSEL, quarter ? int(t / 90.f) % 4 : -1, 0);
    if (turnFineBox && GetFocus() != turnFineBox) SetWindowTextW(turnFineBox, wide(turnText(t).empty() ? "0" : turnText(t)).c_str());
  }
  template <class Row> void turnRows(Row &&row) {
    ShowWindow(turnList, SW_HIDE), ShowWindow(turnFineBox, SW_HIDE);
    if (!turnShown()) return;
    showTurn();
    row(turnList, L"Turn");
    if (advanced) row(turnFineBox, L"Turn (°)", 26);
  }
  // A turn chosen on the right: the brush's, or the picked record's (one Undo).
  void chooseTurn(float degrees) {
    degrees = wrapTurn(degrees);
    const int at = turnRecord();
    if (at < 0) {
      paintTurn = degrees;
      lastPainted.clear();
      showTurn();
      message(L"The paint is laid " + turnWords(paintTurn) + L" now.");
      return;
    }
    const auto f = NativeDocument::fields(doc.lines[size_t(at)]);
    const char *key = turnKeyOf(f[0]);
    if (turnOf(doc.lines[size_t(at)], key) == degrees && turnText(degrees) == namedValue(doc.lines[size_t(at)], key)) return;
    remember();
    record = int(setTurnField(size_t(at), key, degrees));
    publish();
    selectRecord(record, true);
    message(L"Its material is turned " + turnWords(degrees) + L".");
  }
  bool turnCommand(int id, int notification) {
    if (id == TurnList && notification == CBN_SELCHANGE) {
      const auto at = SendMessageW(turnList, CB_GETCURSEL, 0, 0);
      if (at >= 0) chooseTurn(float(at) * 90.f);
      return true;
    }
    if (id == TurnFineBox && notification == EN_KILLFOCUS) {
      applyFineTurn();
      return true;
    }
    return false;
  }
  void applyFineTurn() {
    if (!turnFineBox) return;
    wchar_t text[32]{};
    GetWindowTextW(turnFineBox, text, 32);
    const float value = number(utf8(text), std::numeric_limits<float>::quiet_NaN());
    if (!std::isfinite(value) || value < -360 || value > 360) {
      message(L"A turn is degrees, 0 to 360.");
      showTurn();
      return;
    }
    if (wrapTurn(value) != wrapTurn(shownTurn())) chooseTurn(value);
  }
