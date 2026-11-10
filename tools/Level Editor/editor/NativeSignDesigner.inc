  // ---- The Sign designer: a small window beside the plan for the Sign tool. ----
  // It designs the sign (words, plate style, symbol, colours, size, an imported
  // picture), shows the engine's own pixels for it (renderer/SignArt.h), keeps a
  // library (ready-made, this campaign's, and the author's own .rmsign files in
  // custom maps/signs), imports and exports designs and pictures, and while a
  // sign is picked on the plan every change applies to it at once (Ctrl+Z undoes).
  struct SignDesign {
    std::string title = "NEW SIGN", subtitle;
    std::uint32_t accent = 14238785, ink = 0;
    int style = 0, icon = 0;
    float width = 1.6f, height = .55f;
    std::string artName;
    std::shared_ptr<const retro::signart::Image> art;
  };
  struct SignLibraryItem {
    std::wstring label;
    SignDesign design;
    fs::path file; // My signs only
  };
  enum SignControl { SignTitle = 1, SignSubtitle, SignStyle, SignIcon, SignAccent, SignInk, SignInkAuto, SignWidth, SignHeight,
                     SignLibrary, SignPlace, SignSave, SignImport, SignExport, SignNoPicture, SignDelete, SignSnap };
  SignDesign signDesign;
  HWND signDesigner = nullptr, signPreview = nullptr, signTitleBox = nullptr, signSubBox = nullptr, signStyleList = nullptr,
       signIconList = nullptr, signAccentButton = nullptr, signInkButton = nullptr, signWidthBox = nullptr,
       signHeightBox = nullptr, signLibraryList = nullptr, signStatus = nullptr, signLink = nullptr, signSnapBox = nullptr;
  bool signFilling = false, signWallSnap = true;
  int signBurst = -1;         // the control whose run of changes is one Undo step
  int signLinkedRecord = -1;  // the picked SIGN the burst belongs to
  std::vector<SignLibraryItem> signItems;
  retro::signart::Raster signRaster;
  bool signRasterStale = true;
  static retro::signart::Design artDesign(const SignDesign &d) {
    retro::signart::Design out;
    out.title = d.title, out.subtitle = d.subtitle, out.accent = d.accent | 0xff000000u, out.style = d.style, out.icon = d.icon;
    out.ink = d.ink ? d.ink | 0xff000000u : 0;
    out.art = d.art.get();
    return out;
  }
  // Signs letter printable ASCII; anything else would show as '?'.
  static std::string signText(const std::wstring &text) {
    std::string out;
    for (wchar_t c : text) {
      if (c >= 32 && c < 127) out += char(c);
      else if (c == L'\x2190') out += "<-";
      else if (c == L'\x2192') out += "->";
      else if (c == L'\x2014' || c == L'\x2013') out += '-';
      else if (c == L'\x2018' || c == L'\x2019') out += '\'';
      else if (c == L'\x201C' || c == L'\x201D') out += '"';
    }
    return out;
  }
  // ---- Pictures held in the document (SIGN_ART records).
  std::optional<size_t> signArtLine(const std::string &name) const {
    for (auto i : doc.records("SIGN_ART")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() == 5 && decoded(f[1]) == name) return i;
    }
    return std::nullopt;
  }
  std::shared_ptr<const retro::signart::Image> signArtFromDoc(const std::string &name) const {
    auto at = signArtLine(name);
    if (!at) return nullptr;
    auto f = NativeDocument::fields(doc.lines[*at]);
    auto image = std::make_shared<retro::signart::Image>();
    if (!retro::signart::unbase64(f[4], int(number(f[2], 0)), int(number(f[3], 0)), *image)) return nullptr;
    return image;
  }
  static std::string artRecordPixels(const retro::signart::Image &art) { return retro::signart::base64(art.pixels); }
  // The design's picture is in the document under its name (a clash with a
  // different picture of that name gets a new one). Never touches the selection.
  void ensureSignArt(SignDesign &d) {
    if (!d.art) { d.artName.clear(); return; }
    auto pixels = artRecordPixels(*d.art);
    auto sameAs = [&](size_t line) { auto f = NativeDocument::fields(doc.lines[line]); return f[4] == pixels && f[2] == std::to_string(d.art->width); };
    if (!d.artName.empty())
      if (auto at = signArtLine(d.artName)) { if (sameAs(*at)) return; d.artName.clear(); }
    // Same picture already stored under another name: share it.
    for (auto i : doc.records("SIGN_ART")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() == 5 && f[4] == pixels) { d.artName = decoded(f[1]); return; }
    }
    std::string base = d.artName.empty() ? "picture" : d.artName, name = base;
    for (int n = 2; signArtLine(name); ++n) name = base + "_" + std::to_string(n);
    d.artName = name;
    doc.add({"SIGN_ART", encoded(name), std::to_string(d.art->width), std::to_string(d.art->height), pixels});
  }
  // Pictures no sign wears any more leave the file (the picked record index is kept right).
  void pruneSignArt() {
    std::set<std::string> used;
    for (auto i : doc.records("SIGN")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() > 14 && !f[14].empty()) used.insert(decoded(f[14]));
    }
    auto art = doc.records("SIGN_ART");
    for (auto it = art.rbegin(); it != art.rend(); ++it) {
      auto f = NativeDocument::fields(doc.lines[*it]);
      if (f.size() > 1 && used.count(decoded(f[1]))) continue;
      doc.eraseRecord(*it);
      if (record > int(*it)) --record;
      for (auto &s : selectedRecords) if (s > int(*it)) --s;
    }
  }
  // ---- A SIGN record and a design.
  SignDesign signFromFields(const std::vector<std::string> &f) const {
    SignDesign d;
    if (f.size() < 11) return d;
    d.width = number(f[5], 1.6f), d.height = number(f[6], .55f);
    d.title = decoded(f[8]), d.subtitle = decoded(f[9]);
    d.accent = std::uint32_t(number(f[10], 14238785)) & 0xffffffu;
    if (f.size() > 11 && !f[11].empty()) d.style = std::clamp(int(number(f[11], 0)), 0, retro::signart::StyleCount - 1);
    if (f.size() > 12 && !f[12].empty()) d.icon = std::clamp(int(number(f[12], 0)), 0, retro::signart::IconCount - 1);
    if (f.size() > 13 && !f[13].empty()) d.ink = std::uint32_t(number(f[13], 0)) & 0xffffffu;
    if (f.size() > 14 && !f[14].empty()) d.artName = decoded(f[14]), d.art = signArtFromDoc(d.artName);
    return d;
  }
  static void signToFields(const SignDesign &d, std::vector<std::string> &f) {
    if (f.size() < 15) f.resize(15);
    f[0] = "SIGN";
    f[5] = nativeNumber(d.width), f[6] = nativeNumber(d.height);
    f[8] = encoded(d.title), f[9] = encoded(d.subtitle), f[10] = std::to_string(d.accent & 0xffffffu);
    f[11] = d.style ? std::to_string(d.style) : "";
    f[12] = d.icon ? std::to_string(d.icon) : "";
    f[13] = d.ink ? std::to_string(d.ink & 0xffffffu) : "";
    f[14] = d.art ? encoded(d.artName) : "";
    while (f.size() > 11 && f.back().empty()) f.pop_back();
  }
  bool pickedSign() const {
    return record >= 0 && size_t(record) < doc.lines.size() && doc.lines[size_t(record)].starts_with("SIGN|");
  }
  // ---- Putting a sign on a wall: the face nearest the click, the plate 5 cm
  // proud of it (paint 1 cm), facing the room, slid along so it stays on the wall.
  bool snapSign(float &x, float &y, float &yaw, float width, bool painted) const {
    auto walls = barriers();
    auto wallAt = [&](int tx, int ty) { return tx < 0 || ty < 0 || tx >= 24 || ty >= 24 || walls[size_t(ty * 24 + tx)] == 1; };
    int cx = int(std::floor(x)), cy = int(std::floor(y));
    if (cx < 0 || cy < 0 || cx > 23 || cy > 23) return false;
    const float off = painted ? .01f : .05f;
    struct Face { float gap; int dx, dy; float yaw; } best{wallAt(cx, cy) ? .75f : .9f, 0, 0, 0};
    bool found = false;
    auto consider = [&](float gap, int dx, int dy, float faceYaw) { if (gap < best.gap) best = {gap, dx, dy, faceYaw}, found = true; };
    if (wallAt(cx, cy)) { // clicked on a wall: the open side nearest the click
      if (!wallAt(cx, cy - 1)) consider(y - cy, 0, -1, 3.14159265f);
      if (!wallAt(cx, cy + 1)) consider(cy + 1 - y, 0, 1, 0);
      if (!wallAt(cx - 1, cy)) consider(x - cx, -1, 0, -1.5707963f);
      if (!wallAt(cx + 1, cy)) consider(cx + 1 - x, 1, 0, 1.5707963f);
    } else {
      if (wallAt(cx, cy - 1)) consider(y - cy, 0, -1, 0);
      if (wallAt(cx, cy + 1)) consider(cy + 1 - y, 0, 1, 3.14159265f);
      if (wallAt(cx - 1, cy)) consider(x - cx, -1, 0, 1.5707963f);
      if (wallAt(cx + 1, cy)) consider(cx + 1 - x, 1, 0, -1.5707963f);
    }
    if (!found) return false;
    yaw = best.yaw;
    bool onWall = wallAt(cx, cy);
    // The wall face line and the run of wall along it, so the plate does not overhang a corner.
    if (best.dy != 0) {
      int wallRow = onWall ? cy : cy + best.dy, openRow = onWall ? cy + best.dy : cy;
      float face = float(best.dy < 0 ? cy : cy + 1);
      y = face + (openRow < wallRow ? -off : off);
      int left = cx, right = cx;
      while (left > 0 && wallAt(left - 1, wallRow) && !wallAt(left - 1, openRow)) --left;
      while (right < 23 && wallAt(right + 1, wallRow) && !wallAt(right + 1, openRow)) ++right;
      float lo = left + width * .5f, hi = right + 1 - width * .5f;
      x = lo <= hi ? std::clamp(x, lo, hi) : (left + right + 1) * .5f;
    } else {
      int wallCol = onWall ? cx : cx + best.dx, openCol = onWall ? cx + best.dx : cx;
      float face = float(best.dx < 0 ? cx : cx + 1);
      x = face + (openCol < wallCol ? -off : off);
      int top = cy, bottom = cy;
      while (top > 0 && wallAt(wallCol, top - 1) && !wallAt(openCol, top - 1)) --top;
      while (bottom < 23 && wallAt(wallCol, bottom + 1) && !wallAt(openCol, bottom + 1)) ++bottom;
      float lo = top + width * .5f, hi = bottom + 1 - width * .5f;
      y = lo <= hi ? std::clamp(y, lo, hi) : (top + bottom + 1) * .5f;
    }
    return true;
  }
  // authorKind's SIGN: the designer's design, on the nearest wall, centred at eye height.
  void authorSign(std::vector<std::string> &f, float x, float y, float floorZ) {
    float yaw = 0;
    bool snapped = signWallSnap && snapSign(x, y, yaw, signDesign.width, signDesign.style == retro::signart::Stencil);
    if (!ghosting && !signWallSnap) message(L"Sign placed where you clicked (wall snap is off in the Sign designer).");
    else if (!ghosting && !snapped) message(L"No wall within reach: the sign stands where you clicked, facing south. R turns it.");
    ensureSignArt(signDesign);
    if (f.size() < 15) f.resize(15);
    f[2] = nativeNumber(x), f[3] = nativeNumber(y);
    f[4] = nativeNumber(floorZ + 1.7f - signDesign.height * .5f);
    f[7] = nativeNumber(yaw);
    signToFields(signDesign, f);
  }
  // Metres as a person types them: two places, no trailing noise.
  static std::wstring metres(float v) {
    wchar_t text[32];
    swprintf_s(text, L"%.2f", v);
    std::wstring out = text;
    while (out.size() > 1 && out.back() == L'0') out.pop_back();
    if (!out.empty() && out.back() == L'.') out.pop_back();
    return out;
  }
  // ---- The window.
  void fillSignControls() {
    if (!signDesigner) return;
    signFilling = true;
    SetWindowTextW(signTitleBox, wide(signDesign.title).c_str());
    SetWindowTextW(signSubBox, wide(signDesign.subtitle).c_str());
    SendMessageW(signStyleList, CB_SETCURSEL, signDesign.style, 0);
    SendMessageW(signIconList, CB_SETCURSEL, signDesign.icon, 0);
    SetWindowTextW(signWidthBox, metres(signDesign.width).c_str());
    SetWindowTextW(signHeightBox, metres(signDesign.height).c_str());
    SendMessageW(signSnapBox, BM_SETCHECK, signWallSnap ? BST_CHECKED : BST_UNCHECKED, 0);
    signFilling = false;
    signRasterStale = true;
    InvalidateRect(signAccentButton, nullptr, TRUE);
    InvalidateRect(signInkButton, nullptr, TRUE);
    InvalidateRect(signPreview, nullptr, FALSE);
    showSignLink();
  }
  void showSignLink() {
    if (!signDesigner) return;
    int onMap = int(doc.records("SIGN", map).size());
    std::wstring text = pickedSign() ? L"Editing the sign picked on the plan. Changes show at once; Ctrl+Z undoes."
                                     : L"New signs use this design. Choose the Sign tool and click a wall.";
    SetWindowTextW(signLink, text.c_str());
    std::wstring count = L"Signs on this map: " + std::to_wstring(onMap) + (onMap > 3 ? L"  (the game's own maps keep to 3)" : L"");
    SetWindowTextW(signStatus, count.c_str());
  }
  void signSay(const std::wstring &text) { if (signStatus) SetWindowTextW(signStatus, text.c_str()); message(text); }
  // A change in the window: read it, redraw, and (when a sign is picked) apply it.
  void signChanged(int control) {
    if (signFilling || !signDesigner) return;
    auto readFloat = [&](HWND box, float fallback) { auto t = utf8(label(box)); try { return std::clamp(std::stof(t), .1f, 12.f); } catch (...) { return fallback; } };
    signDesign.title = signText(label(signTitleBox));
    signDesign.subtitle = signText(label(signSubBox));
    signDesign.style = std::clamp(int(SendMessageW(signStyleList, CB_GETCURSEL, 0, 0)), 0, retro::signart::StyleCount - 1);
    signDesign.icon = std::clamp(int(SendMessageW(signIconList, CB_GETCURSEL, 0, 0)), 0, retro::signart::IconCount - 1);
    signDesign.width = readFloat(signWidthBox, signDesign.width);
    signDesign.height = readFloat(signHeightBox, signDesign.height);
    signRasterStale = true;
    InvalidateRect(signPreview, nullptr, FALSE);
    applySignToPicked(control);
  }
  void applySignToPicked(int control) {
    if (!pickedSign()) return;
    if (!editableRecord(size_t(record))) { signSay(L"Unlock and show this floor before editing its signs."); return; }
    if (signBurst != control || signLinkedRecord != record) remember(), signBurst = control, signLinkedRecord = record;
    ensureSignArt(signDesign);
    auto f = NativeDocument::fields(doc.lines[size_t(record)]);
    auto before = f;
    // A painted-on sign sits flush; a plate stands 5 cm proud. Move it along its facing.
    bool wasPainted = before.size() > 11 && before[11] == std::to_string(int(retro::signart::Stencil));
    bool painted = signDesign.style == retro::signart::Stencil;
    if (wasPainted != painted) {
      float yaw = number(f[7], 0), shift = painted ? -.04f : .04f;
      f[2] = nativeNumber(number(f[2], 0) + std::sin(yaw) * shift), f[3] = nativeNumber(number(f[3], 0) + std::cos(yaw) * shift);
    }
    // Keep the sign's centre height when it grows or shrinks.
    float oldHeight = number(before[6], signDesign.height);
    f[4] = nativeNumber(number(f[4], 0) + (oldHeight - signDesign.height) * .5f);
    signToFields(signDesign, f);
    doc.replace(size_t(record), f);
    pruneSignArt();
    signLinkedRecord = record;
    publish(true, false);
    InvalidateRect(plan, nullptr, FALSE);
    showSignLink();
  }
  // Selecting a sign on the plan shows it in the designer (and opens it).
  void signSelectionChanged() {
    if (!pickedSign()) { showSignLink(); return; }
    signDesign = signFromFields(NativeDocument::fields(doc.lines[size_t(record)]));
    signBurst = -1;
    if (!signDesigner && !backgroundTest) openSignDesigner();
    fillSignControls();
  }
  // ---- Library.
  static SignDesign preset(const char *title, const char *subtitle, std::uint32_t accent, int style, int icon, float w, float h, std::uint32_t ink = 0) {
    SignDesign d;
    d.title = title, d.subtitle = subtitle, d.accent = accent, d.style = style, d.icon = icon, d.width = w, d.height = h, d.ink = ink;
    return d;
  }
  static const std::vector<std::pair<const wchar_t *, SignDesign>> &signPresets() {
    using namespace retro::signart;
    static const std::vector<std::pair<const wchar_t *, SignDesign>> list = {
        {L"Danger: high voltage", preset("DANGER", "HIGH VOLTAGE", 0xf2bc24, Hazard, Electric, 1.2f, .6f)},
        {L"Warning: hazard", preset("WARNING", "KEEP CLEAR", 0xf2bc24, Hazard, Warning, 1.2f, .6f)},
        {L"Radiation", preset("RADIATION", "CONTROLLED AREA", 0xf2bc24, Hazard, Radiation, 1.2f, .6f)},
        {L"Fire risk", preset("FIRE RISK", "NO NAKED FLAMES", 0xf2bc24, Hazard, Fire, 1.2f, .6f)},
        {L"No entry", preset("NO ENTRY", "AUTHORISED STAFF ONLY", 0xc4221c, Notice, NoEntry, 1.f, .7f)},
        {L"Notice: door sealed", preset("NOTICE", "THIS DOOR IS SEALED", 0x2a5aa8, Notice, NoIcon, 1.f, .7f)},
        {L"Exit", preset("EXIT", "", 0x1c8048, Route, Exit, 1.f, .36f)},
        {L"Route: left", preset("STAIRS", "LEVEL 2", 0x1f5f9e, Route, ArrowLeft, 1.4f, .45f)},
        {L"Route: right", preset("LIFT", "FREIGHT", 0x1f5f9e, Route, ArrowRight, 1.4f, .45f)},
        {L"Painted: keep out", preset("KEEP OUT", "", 0xd8d4c8, Stencil, NoIcon, 2.f, .5f)},
        {L"Painted: arrow", preset("THIS WAY", "", 0xe0b030, Stencil, ArrowRight, 1.6f, .45f)},
        {L"Chalk: shift note", preset("NIGHT SHIFT", "pump 3 is leaking again", 0x7fb0d8, Chalk, NoIcon, 1.2f, .8f)},
        {L"Enamel plate", preset("PUMP HALL", "HIGH PRESSURE", 0xa7a766, Plate, NoIcon, 1.6f, .45f)},
    };
    return list;
  }
  fs::path signFolder() const { return root / L"custom maps" / L"signs"; }
  void fillSignLibrary() {
    if (!signLibraryList) return;
    signItems.clear();
    SendMessageW(signLibraryList, LB_RESETCONTENT, 0, 0);
    auto header = [&](const wchar_t *text) { auto at = SendMessageW(signLibraryList, LB_ADDSTRING, 0, (LPARAM)text); SendMessageW(signLibraryList, LB_SETITEMDATA, at, (LPARAM)-1); };
    auto item = [&](SignLibraryItem it) {
      auto at = SendMessageW(signLibraryList, LB_ADDSTRING, 0, (LPARAM)(L"    " + it.label).c_str());
      SendMessageW(signLibraryList, LB_SETITEMDATA, at, (LPARAM)signItems.size());
      signItems.push_back(std::move(it));
    };
    header(L"READY-MADE");
    for (auto &[name, design] : signPresets()) item({name, design, {}});
    // Every distinct design already in this campaign, so a sign can be repeated anywhere.
    std::set<std::string> seen;
    bool any = false;
    for (auto i : doc.records("SIGN")) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 11) continue;
      auto d = signFromFields(f);
      std::string key = f[5] + '|' + f[6];
      for (size_t k = 8; k < f.size(); ++k) key += '|' + f[k];
      if (!seen.insert(key).second) continue;
      if (!any) header(L"IN THIS CAMPAIGN"), any = true;
      item({wide(d.title.empty() ? d.subtitle : d.title) + (d.subtitle.empty() || d.title.empty() ? L"" : L" / " + wide(d.subtitle)) + L"  (map " + wide(f[1]) + L")", d, {}});
    }
    header(L"MY SIGNS  (custom maps\\signs)");
    std::error_code ec;
    if (fs::is_directory(signFolder(), ec))
      for (auto &entry : fs::directory_iterator(signFolder(), ec)) {
        if (entry.path().extension() != L".rmsign") continue;
        SignDesign d;
        std::wstring why;
        if (readSignFile(entry.path(), d, why)) item({entry.path().stem().wstring(), d, entry.path()});
      }
  }
  void signLibraryPicked() {
    auto at = SendMessageW(signLibraryList, LB_GETCURSEL, 0, 0);
    if (at < 0) return;
    auto data = SendMessageW(signLibraryList, LB_GETITEMDATA, at, 0);
    if (data < 0 || size_t(data) >= signItems.size()) return;
    auto keepArt = signItems[size_t(data)].design;
    signDesign = keepArt;
    fillSignControls();
    applySignToPicked(SignLibrary);
  }
  // ---- Files: .rmsign is the SIGN fields that make the look, plus its picture.
  bool writeSignFile(const fs::path &file, std::wstring &why) const {
    std::ofstream out(file, std::ios::binary);
    if (!out) { why = L"Could not write " + file.wstring(); return false; }
    out << "# DepthWorks sign design (Raw Metal Campaign Builder). Import it in the Sign designer.\n";
    out << "SIGN_DESIGN|" << nativeNumber(signDesign.width) << '|' << nativeNumber(signDesign.height) << '|' << encoded(signDesign.title) << '|'
        << encoded(signDesign.subtitle) << '|' << (signDesign.accent & 0xffffffu) << '|' << signDesign.style << '|' << signDesign.icon << '|'
        << (signDesign.ink & 0xffffffu) << '|' << encoded(signDesign.art ? (signDesign.artName.empty() ? "picture" : signDesign.artName) : "") << '\n';
    if (signDesign.art)
      out << "SIGN_ART|" << encoded(signDesign.artName.empty() ? "picture" : signDesign.artName) << '|' << signDesign.art->width << '|'
          << signDesign.art->height << '|' << artRecordPixels(*signDesign.art) << '\n';
    if (!out) { why = L"Could not finish " + file.wstring(); return false; }
    return true;
  }
  bool readSignFile(const fs::path &file, SignDesign &d, std::wstring &why) const {
    std::ifstream in(file, std::ios::binary);
    if (!in) { why = L"Could not open " + file.wstring(); return false; }
    std::string line;
    bool designed = false;
    std::shared_ptr<retro::signart::Image> art;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      auto f = NativeDocument::fields(line);
      if (f[0] == "SIGN_DESIGN" && f.size() >= 10) {
        d.width = std::clamp(number(f[1], 1.6f), .1f, 12.f), d.height = std::clamp(number(f[2], .55f), .1f, 12.f);
        d.title = decoded(f[3]), d.subtitle = decoded(f[4]), d.accent = std::uint32_t(number(f[5], 14238785)) & 0xffffffu;
        d.style = std::clamp(int(number(f[6], 0)), 0, retro::signart::StyleCount - 1);
        d.icon = std::clamp(int(number(f[7], 0)), 0, retro::signart::IconCount - 1);
        d.ink = std::uint32_t(number(f[8], 0)) & 0xffffffu;
        d.artName = decoded(f[9]);
        designed = true;
      } else if (f[0] == "SIGN_ART" && f.size() == 5) {
        art = std::make_shared<retro::signart::Image>();
        if (!retro::signart::unbase64(f[4], int(number(f[2], 0)), int(number(f[3], 0)), *art)) { why = L"The picture in " + file.filename().wstring() + L" is damaged."; return false; }
        if (d.artName.empty()) d.artName = decoded(f[1]);
      }
    }
    if (!designed) { why = file.filename().wstring() + L" is not a sign design."; return false; }
    d.art = art;
    if (!d.art) d.artName.clear();
    return true;
  }
  // A picture file (PNG, JPEG, BMP, TGA) as sign art, shrunk to the SIGN_ART limits.
  bool readSignPicture(const fs::path &file, std::shared_ptr<const retro::signart::Image> &out, std::wstring &why) const {
    std::ifstream in(file, std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(in), {});
    if (bytes.empty() || bytes.size() > 64u * 1024u * 1024u) { why = L"Could not read " + file.filename().wstring(); return false; }
    int w = 0, h = 0, n = 0;
    unsigned char *pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(bytes.data()), int(bytes.size()), &w, &h, &n, 4);
    if (!pixels) { why = file.filename().wstring() + L" is not a picture this builder reads (PNG, JPEG, BMP or TGA)."; return false; }
    retro::signart::Image image;
    image.width = w, image.height = h, image.pixels.resize(size_t(w) * h);
    for (size_t i = 0; i < image.pixels.size(); ++i)
      image.pixels[i] = (std::uint32_t(pixels[i * 4 + 3]) << 24) | (std::uint32_t(pixels[i * 4]) << 16) | (std::uint32_t(pixels[i * 4 + 1]) << 8) | pixels[i * 4 + 2];
    stbi_image_free(pixels);
    out = std::make_shared<retro::signart::Image>(retro::signart::fitArt(image));
    return true;
  }
  // The sign as a PNG (stored deflate: no compressor needed, every viewer reads it).
  static bool writePng(const fs::path &file, const retro::signart::Raster &r) {
    auto crcTable = [] { std::array<std::uint32_t, 256> t{}; for (std::uint32_t n = 0; n < 256; ++n) { std::uint32_t c = n; for (int k = 0; k < 8; ++k) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1; t[n] = c; } return t; }();
    auto be = [](std::string &s, std::uint32_t v) { for (int i = 3; i >= 0; --i) s += char((v >> (i * 8)) & 255); };
    std::string raw;
    raw.reserve(size_t(r.height) * (r.width * 4 + 1));
    for (int y = 0; y < r.height; ++y) {
      raw += char(0);
      for (int x = 0; x < r.width; ++x) { auto p = r.pixels[size_t(y) * r.width + x]; raw += char((p >> 16) & 255); raw += char((p >> 8) & 255); raw += char(p & 255); raw += char(r.transparent ? (p >> 24) & 255 : 255); }
    }
    std::string z = "\x78\x01";
    for (size_t at = 0; at < raw.size() || at == 0;) {
      size_t n = std::min<size_t>(65535, raw.size() - at);
      bool last = at + n >= raw.size();
      z += char(last ? 1 : 0); z += char(n & 255); z += char(n >> 8); z += char(~n & 255); z += char((~n >> 8) & 255);
      z.append(raw, at, n);
      at += n;
      if (last) break;
    }
    std::uint32_t a = 1, b = 0;
    for (unsigned char c : raw) a = (a + c) % 65521, b = (b + a) % 65521;
    be(z, (b << 16) | a);
    auto chunk = [&](std::string &out, const char *type, const std::string &data) {
      be(out, std::uint32_t(data.size()));
      std::string body = std::string(type, 4) + data;
      out += body;
      std::uint32_t c = 0xffffffffu;
      for (unsigned char ch : body) c = crcTable[(c ^ ch) & 255] ^ (c >> 8);
      be(out, c ^ 0xffffffffu);
    };
    std::string png = "\x89PNG\r\n\x1a\n", ihdr;
    be(ihdr, std::uint32_t(r.width)); be(ihdr, std::uint32_t(r.height));
    ihdr += char(8); ihdr += char(6); ihdr += char(0); ihdr += char(0); ihdr += char(0);
    chunk(png, "IHDR", ihdr); chunk(png, "IDAT", z); chunk(png, "IEND", "");
    std::ofstream out(file, std::ios::binary);
    out.write(png.data(), std::streamsize(png.size()));
    return bool(out);
  }
  fs::path signFileDialog(bool save) {
    wchar_t name[MAX_PATH]{};
    std::wstring base = save ? wide(signText(wide(signDesign.title.empty() ? "sign" : signDesign.title))) : L"";
    for (auto &c : base) if (wcschr(L"\\/:*?\"<>|", c)) c = L'-';
    wcsncpy_s(name, base.c_str(), _TRUNCATE);
    std::error_code ec;
    fs::create_directories(signFolder(), ec);
    auto folder = signFolder().wstring();
    OPENFILENAMEW dialog{sizeof dialog};
    dialog.hwndOwner = signDesigner ? signDesigner : window;
    dialog.lpstrFile = name;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrInitialDir = folder.c_str();
    if (save) {
      dialog.lpstrFilter = L"Sign design (*.rmsign)\0*.rmsign\0PNG picture of the sign (*.png)\0*.png\0";
      dialog.lpstrDefExt = L"rmsign";
      dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
      if (!GetSaveFileNameW(&dialog)) return {};
      fs::path out = name;
      if (dialog.nFilterIndex == 2 && out.extension() != L".png") out.replace_extension(L".png");
      return out;
    }
    dialog.lpstrFilter = L"Sign designs and pictures\0*.rmsign;*.png;*.jpg;*.jpeg;*.bmp;*.tga\0Sign design (*.rmsign)\0*.rmsign\0Pictures\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return {};
    return name;
  }
  bool importSign(const fs::path &file, std::wstring &why) {
    auto ext = file.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
    if (ext == L".rmsign") {
      SignDesign d;
      if (!readSignFile(file, d, why)) return false;
      signDesign = d;
      why = L"Imported the design " + file.stem().wstring() + L".";
      return true;
    }
    std::shared_ptr<const retro::signart::Image> art;
    if (!readSignPicture(file, art, why)) return false;
    signDesign.art = art;
    std::string name;
    for (char c : utf8(file.stem().wstring())) name += std::isalnum(static_cast<unsigned char>(c)) ? char(std::tolower(static_cast<unsigned char>(c))) : '_';
    signDesign.artName = name.empty() ? "picture" : name;
    // A plain plate becomes a picture sign, at the picture's proportions.
    if (signDesign.style == retro::signart::Plate && signDesign.icon == 0) {
      signDesign.style = retro::signart::Picture;
      signDesign.height = std::clamp(signDesign.width * art->height / float(art->width), .15f, 4.f);
    }
    why = L"Picture " + file.filename().wstring() + L" (" + std::to_wstring(art->width) + L" x " + std::to_wstring(art->height) + L") is on the sign.";
    return true;
  }
  bool exportSign(const fs::path &file, std::wstring &why) {
    if (file.extension() == L".png") {
      auto raster = retro::signart::draw(artDesign(signDesign), signDesign.width / signDesign.height);
      if (!writePng(file, raster)) { why = L"Could not write " + file.wstring(); return false; }
      why = L"Saved a picture of the sign: " + file.filename().wstring();
      return true;
    }
    if (!writeSignFile(file, why)) return false;
    why = L"Saved " + file.filename().wstring() + L". It is in My signs and can be imported into any campaign.";
    return true;
  }
  void signCommand(int id, int notification) {
    std::wstring why;
    switch (id) {
    case SignTitle: case SignSubtitle: case SignWidth: case SignHeight:
      if (notification == EN_CHANGE) signChanged(id);
      if (notification == EN_KILLFOCUS) signBurst = -1;
      return;
    case SignStyle: case SignIcon:
      if (notification == CBN_SELCHANGE) signChanged(id);
      return;
    case SignAccent: case SignInk: {
      static COLORREF custom[16]{RGB(0xf2, 0xbc, 0x24), RGB(0xc4, 0x22, 0x1c), RGB(0x1c, 0x80, 0x48), RGB(0x1f, 0x5f, 0x9e), RGB(0xa7, 0xa7, 0x66), RGB(0xd8, 0xd4, 0xc8)};
      std::uint32_t &value = id == SignAccent ? signDesign.accent : signDesign.ink;
      CHOOSECOLORW pick{sizeof pick};
      pick.hwndOwner = signDesigner;
      pick.rgbResult = RGB((value >> 16) & 255, (value >> 8) & 255, value & 255);
      pick.lpCustColors = custom;
      pick.Flags = CC_FULLOPEN | CC_RGBINIT;
      if (!ChooseColorW(&pick)) return;
      value = (std::uint32_t(GetRValue(pick.rgbResult)) << 16) | (std::uint32_t(GetGValue(pick.rgbResult)) << 8) | GetBValue(pick.rgbResult);
      if (id == SignInk && value == 0) value = 0x010101; // black ink, not "the style's own"
      fillSignControls();
      applySignToPicked(id * 1000 + int(GetTickCount64() & 0xfff));
      return;
    }
    case SignInkAuto:
      signDesign.ink = 0;
      fillSignControls();
      applySignToPicked(id * 1000 + int(GetTickCount64() & 0xfff));
      return;
    case SignSnap:
      signWallSnap = SendMessageW(signSnapBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
      return;
    case SignLibrary:
      if (notification == LBN_SELCHANGE) signLibraryPicked();
      return;
    case SignPlace:
      selectRecord(-1);
      SendMessageW(window, WM_COMMAND, ToolBase + 7, 0);
      showSignLink();
      signSay(L"Click a wall on the plan to put this sign up. It faces the room.");
      return;
    case SignSave: {
      std::error_code ec;
      fs::create_directories(signFolder(), ec);
      std::wstring stem = wide(signDesign.title.empty() ? (signDesign.subtitle.empty() ? std::string("sign") : signDesign.subtitle) : signDesign.title);
      for (auto &c : stem) if (wcschr(L"\\/:*?\"<>|", c)) c = L'-';
      auto file = signFolder() / (stem + L".rmsign");
      for (int n = 2; fs::exists(file, ec); ++n) file = signFolder() / (stem + L" " + std::to_wstring(n) + L".rmsign");
      if (exportSign(file, why)) fillSignLibrary();
      signSay(why);
      return;
    }
    case SignImport: {
      auto file = signFileDialog(false);
      if (file.empty()) return;
      if (importSign(file, why)) { fillSignControls(); applySignToPicked(id * 1000 + int(GetTickCount64() & 0xfff)); }
      signSay(why);
      return;
    }
    case SignExport: {
      auto file = signFileDialog(true);
      if (file.empty()) return;
      if (exportSign(file, why) && file.extension() == L".rmsign") fillSignLibrary();
      signSay(why);
      return;
    }
    case SignNoPicture:
      signDesign.art = nullptr, signDesign.artName.clear();
      if (signDesign.style == retro::signart::Picture) signDesign.style = retro::signart::Plate;
      fillSignControls();
      applySignToPicked(id * 1000 + int(GetTickCount64() & 0xfff));
      signSay(L"The picture is off this design.");
      return;
    case SignDelete: {
      auto at = SendMessageW(signLibraryList, LB_GETCURSEL, 0, 0);
      auto data = at < 0 ? -1 : SendMessageW(signLibraryList, LB_GETITEMDATA, at, 0);
      if (data < 0 || size_t(data) >= signItems.size() || signItems[size_t(data)].file.empty()) { signSay(L"Pick one of My signs to delete it."); return; }
      auto file = signItems[size_t(data)].file;
      if (MessageBoxW(signDesigner, (L"Move " + file.filename().wstring() + L" to the Recycle Bin?").c_str(), L"Delete sign design", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
      std::wstring from = file.wstring() + L'\0';
      SHFILEOPSTRUCTW op{};
      op.hwnd = signDesigner, op.wFunc = FO_DELETE, op.pFrom = from.c_str(), op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
      SHFileOperationW(&op);
      fillSignLibrary();
      signSay(L"Moved to the Recycle Bin: " + file.filename().wstring());
      return;
    }
    }
  }
  // The preview: the sign as the engine draws it, on a concrete wall, at its proportions.
  void paintSignPreview(HDC dc, RECT c) {
    HBRUSH wall = CreateSolidBrush(RGB(0x5a, 0x58, 0x54));
    FillRect(dc, &c, wall);
    DeleteObject(wall);
    for (int y = c.top; y < c.bottom; y += 3) { // a little board-formed concrete grain
      HPEN p = CreatePen(PS_SOLID, 1, ((y / 3) % 7) ? RGB(0x5e, 0x5c, 0x58) : RGB(0x52, 0x50, 0x4c));
      auto o = SelectObject(dc, p); MoveToEx(dc, c.left, y, nullptr); LineTo(dc, c.right, y); SelectObject(dc, o); DeleteObject(p);
    }
    if (signRasterStale) signRaster = retro::signart::draw(artDesign(signDesign), signDesign.width / signDesign.height), signRasterStale = false;
    const auto &r = signRaster;
    if (r.width <= 0) return;
    float aspect = signDesign.width / signDesign.height;
    int availW = c.right - c.left - 36, availH = c.bottom - c.top - 40;
    int w = availW, h = int(w / aspect);
    if (h > availH) h = availH, w = int(h * aspect);
    int x = c.left + (c.right - c.left - w) / 2, y = c.top + (c.bottom - c.top - 18 - h) / 2;
    // Composite over the wall (a painted-on sign lets it through), then scale.
    std::vector<std::uint32_t> shown(r.pixels.size());
    for (size_t i = 0; i < shown.size(); ++i) {
      auto p = r.pixels[i];
      if (!r.transparent) { shown[i] = p | 0xff000000u; continue; }
      float a = float(p >> 24) / 255.f;
      auto ch = [&](int s, int under) { return std::uint32_t(((p >> s) & 255) * a + under * (1 - a)); };
      shown[i] = 0xff000000u | (ch(16, 0x5a) << 16) | (ch(8, 0x58) << 8) | ch(0, 0x54);
    }
    if (!r.transparent) { // a soft shadow under the plate
      HBRUSH shade = CreateSolidBrush(RGB(0x3c, 0x3a, 0x37));
      RECT s{x + 4, y + 5, x + w + 4, y + h + 5};
      FillRect(dc, &s, shade);
      DeleteObject(shade);
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER), info.bmiHeader.biWidth = r.width, info.bmiHeader.biHeight = -r.height;
    info.bmiHeader.biPlanes = 1, info.bmiHeader.biBitCount = 32, info.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    StretchDIBits(dc, x, y, w, h, 0, 0, r.width, r.height, shown.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    wchar_t size[64];
    swprintf_s(size, L"%.2f m wide x %.2f m high", signDesign.width, signDesign.height);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0xe6, 0xe0, 0xd4));
    auto old = SelectObject(dc, smallFont ? smallFont : font);
    RECT t{c.left, c.bottom - 20, c.right, c.bottom - 2};
    DrawTextW(dc, size, -1, &t, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, old);
  }
  static LRESULT CALLBACK signPreviewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(GetParent(h), GWLP_USERDATA));
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT && e) {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      RECT c;
      GetClientRect(h, &c);
      HDC mem = CreateCompatibleDC(dc);
      HBITMAP bmp = CreateCompatibleBitmap(dc, c.right, c.bottom);
      auto old = SelectObject(mem, bmp);
      e->paintSignPreview(mem, c);
      BitBlt(dc, 0, 0, c.right, c.bottom, mem, 0, 0, SRCCOPY);
      SelectObject(mem, old);
      DeleteObject(bmp);
      DeleteDC(mem);
      EndPaint(h, &ps);
      return 0;
    }
    return DefWindowProcW(h, m, w, l);
  }
  static LRESULT CALLBACK signDesignerProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto *e = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!e) return DefWindowProcW(h, m, w, l);
    switch (m) {
    case WM_COMMAND:
      try { e->signCommand(LOWORD(w), HIWORD(w)); } catch (const std::exception &error) { e->signSay(wide(error.what())); }
      return 0;
    case WM_DRAWITEM: { // the two colour swatches
      auto *d = reinterpret_cast<DRAWITEMSTRUCT *>(l);
      bool ink = d->CtlID == SignInk;
      std::uint32_t v = ink ? e->signDesign.ink : e->signDesign.accent;
      HBRUSH edge = CreateSolidBrush(ui::FieldEdge);
      FillRect(d->hDC, &d->rcItem, edge);
      DeleteObject(edge);
      RECT in = d->rcItem;
      InflateRect(&in, -2, -2);
      HBRUSH b = CreateSolidBrush(ui::Field);
      FillRect(d->hDC, &in, b);
      DeleteObject(b);
      RECT swatch{in.left + 4, in.top + 4, in.left + 4 + (in.bottom - in.top - 8), in.bottom - 4};
      if (!ink || v) {
        HBRUSH s = CreateSolidBrush(RGB((v >> 16) & 255, (v >> 8) & 255, v & 255));
        FillRect(d->hDC, &swatch, s);
        DeleteObject(s);
      } else
        for (int y = swatch.top; y < swatch.bottom; y += 4)
          for (int x = swatch.left; x < swatch.right; x += 4) {
            RECT q{x, y, std::min<LONG>(x + 4, swatch.right), std::min<LONG>(y + 4, swatch.bottom)};
            HBRUSH s = CreateSolidBrush(((x - swatch.left) / 4 + (y - swatch.top) / 4) & 1 ? RGB(200, 200, 200) : RGB(250, 250, 250));
            FillRect(d->hDC, &q, s);
            DeleteObject(s);
          }
      RECT t{swatch.right + 6, in.top, in.right, in.bottom};
      SetBkMode(d->hDC, TRANSPARENT);
      SetTextColor(d->hDC, ui::Ink);
      SelectObject(d->hDC, e->font);
      DrawTextW(d->hDC, ink ? (v ? L"Lettering" : L"Lettering: auto") : L"Colour", -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
      return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
      SetBkColor(reinterpret_cast<HDC>(w), ui::Paper);
      SetTextColor(reinterpret_cast<HDC>(w), reinterpret_cast<HWND>(l) == e->signStatus ? ui::InkSoft : ui::Ink);
      static HBRUSH paper = CreateSolidBrush(ui::Paper);
      return reinterpret_cast<LRESULT>(paper);
    }
    case WM_CLOSE:
      e->closeSignDesigner();
      return 0;
    }
    return DefWindowProcW(h, m, w, l);
  }
  void closeSignDesigner() {
    if (!signDesigner) return;
    RECT r;
    GetWindowRect(signDesigner, &r);
    signDesignerAt = {r.left, r.top};
    DestroyWindow(signDesigner);
    signDesigner = nullptr;
    if (window) SetActiveWindow(window);
  }
  POINT signDesignerAt{LONG_MIN, LONG_MIN};
  void openSignDesigner() {
    if (signDesigner) { ShowWindow(signDesigner, SW_SHOWNOACTIVATE); fillSignControls(); return; }
    if (!window || backgroundTest) return;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
      WNDCLASSW wc{};
      wc.lpfnWndProc = signDesignerProc;
      wc.hInstance = instance;
      wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      wc.hbrBackground = CreateSolidBrush(ui::Paper);
      wc.lpszClassName = L"RawMetalSignDesigner";
      RegisterClassW(&wc);
      WNDCLASSW pv{};
      pv.lpfnWndProc = signPreviewProc;
      pv.hInstance = instance;
      pv.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      pv.lpszClassName = L"RawMetalSignPreview";
      RegisterClassW(&pv);
      registered = true;
    }
    const int width = 560, height = 800;
    RECT owner{};
    GetWindowRect(window, &owner);
    POINT at = signDesignerAt.x != LONG_MIN ? signDesignerAt : POINT{owner.right - width - 40, owner.top + 120};
    signDesigner = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT, L"RawMetalSignDesigner", L"Sign designer",
                                   WS_POPUP | WS_CAPTION | WS_SYSMENU, at.x, at.y, width, height, window, nullptr, instance, nullptr);
    if (!signDesigner) return;
    SetWindowLongPtrW(signDesigner, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    RECT c{};
    GetClientRect(signDesigner, &c);
    const int pad = 14, full = c.right - pad * 2;
    auto child = [&](const wchar_t *kind, const wchar_t *text, DWORD style, int id, int x, int y, int w, int h, HFONT face = nullptr) {
      HWND made = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, signDesigner,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
      SendMessageW(made, WM_SETFONT, reinterpret_cast<WPARAM>(face ? face : font), TRUE);
      return made;
    };
    int y = pad;
    signPreview = child(L"RawMetalSignPreview", L"", 0, -1, pad, y, full, 190);
    y += 198;
    signLink = child(L"STATIC", L"", 0, -1, pad, y, full, 20, smallFont);
    y += 26;
    child(L"STATIC", L"Big words", 0, -1, pad, y + 5, 110, 22);
    signTitleBox = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, SignTitle, pad + 116, y, full - 116, 28);
    y += 34;
    child(L"STATIC", L"Small words", 0, -1, pad, y + 5, 110, 22);
    signSubBox = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, SignSubtitle, pad + 116, y, full - 116, 28);
    y += 38;
    int half = (full - 10) / 2;
    signStyleList = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, SignStyle, pad, y, half, 300);
    for (auto name : retro::signart::styleNames) SendMessageW(signStyleList, CB_ADDSTRING, 0, (LPARAM)wide(name).c_str());
    signIconList = child(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, SignIcon, pad + half + 10, y, half, 300);
    for (auto name : retro::signart::iconNames) SendMessageW(signIconList, CB_ADDSTRING, 0, (LPARAM)wide(name).c_str());
    y += 36;
    signAccentButton = child(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, SignAccent, pad, y, 120, 30);
    signInkButton = child(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, SignInk, pad + 126, y, 146, 30);
    child(L"BUTTON", L"Auto", WS_TABSTOP, SignInkAuto, pad + 278, y, 48, 30, smallFont);
    child(L"STATIC", L"Size (m)", 0, -1, pad + 336, y + 6, 56, 22, smallFont);
    signWidthBox = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, SignWidth, pad + 392, y + 1, 60, 28);
    child(L"STATIC", L"x", 0, -1, pad + 457, y + 6, 10, 22, smallFont);
    signHeightBox = child(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, SignHeight, pad + 470, y + 1, full - 470, 28);
    y += 38;
    signSnapBox = child(L"BUTTON", L"Snap new signs to the nearest wall", BS_AUTOCHECKBOX | WS_TABSTOP, SignSnap, pad, y, 300, 24, smallFont);
    child(L"BUTTON", L"No picture", WS_TABSTOP, SignNoPicture, pad + full - 110, y - 2, 110, 28, smallFont);
    y += 32;
    child(L"STATIC", L"Library: click one to use it (on the picked sign too)", 0, -1, pad, y, full, 20, smallFont);
    y += 22;
    signLibraryList = child(L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_TABSTOP, SignLibrary, pad, y, full,
                            c.bottom - y - 112);
    y = c.bottom - 104;
    int bw = (full - 4 * 8) / 5;
    child(L"BUTTON", L"Place on wall", WS_TABSTOP, SignPlace, pad, y, bw, 34);
    child(L"BUTTON", L"Save to mine", WS_TABSTOP, SignSave, pad + (bw + 8), y, bw, 34);
    child(L"BUTTON", L"Import...", WS_TABSTOP, SignImport, pad + (bw + 8) * 2, y, bw, 34);
    child(L"BUTTON", L"Export...", WS_TABSTOP, SignExport, pad + (bw + 8) * 3, y, bw, 34);
    child(L"BUTTON", L"Delete mine", WS_TABSTOP, SignDelete, pad + (bw + 8) * 4, y, bw, 34);
    signStatus = child(L"STATIC", L"", 0, -1, pad, y + 44, full, 44, smallFont);
    fillSignLibrary();
    fillSignControls();
    ShowWindow(signDesigner, SW_SHOWNOACTIVATE);
  }
  // Headless: snapping, a design with a picture applied and validated by the
  // engine, the .rmsign and PNG round trips, and clean-up of unused pictures.
  bool signDesignerTest(std::string &report) {
    auto fail = [&](const std::string &why) { report += "FAIL: " + why + "\n"; return false; };
    doc.open(root / L"custom maps" / L"Starter Map.rmc");
    path.clear(), map = 0, layer = 0, record = -1;
    auto walls = barriers();
    int cx = -1, cy = -1;
    for (int y = 1; y < 23 && cx < 0; ++y)
      for (int x = 2; x < 22; ++x)
        if (walls[size_t(y * 24 + x)] == 0 && walls[size_t((y - 1) * 24 + x)] == 1 && walls[size_t(y * 24 + x - 1)] == 0 && walls[size_t(y * 24 + x + 1)] == 0 &&
            walls[size_t((y + 1) * 24 + x)] == 0) { cx = x, cy = y; break; }
    if (cx < 0) return fail("no open cell under a north wall on the Starter Map");
    float x = cx + .5f, y = cy + .3f, yaw = 9;
    if (!snapSign(x, y, yaw, .5f, false) || std::abs(y - (cy + .05f)) > 1e-4f || std::abs(yaw) > 1e-4f)
      return fail("north-wall snap gave " + std::to_string(x) + "," + std::to_string(y) + " yaw " + std::to_string(yaw));
    report += "snap: north wall face at y=" + std::to_string(cy) + " -> sign y " + std::to_string(y) + ", facing south\n";
    // Clicking the wall itself puts the sign on its open face.
    x = cx + .5f, y = cy - .2f, yaw = 9;
    if (!snapSign(x, y, yaw, .5f, false) || std::abs(y - (cy + .05f)) > 1e-4f || std::abs(yaw) > 1e-4f)
      return fail("click on the wall did not reach its open face");
    // A designed sign with a picture.
    retro::signart::Image picture;
    picture.width = 40, picture.height = 20, picture.pixels.resize(800);
    for (int i = 0; i < 800; ++i) picture.pixels[size_t(i)] = 0xff000000u | ((i % 40) * 6 << 16) | ((i / 40) * 12 << 8) | 0x40;
    signDesign = preset("DANGER", "TEST SIGN", 0xf2bc24, retro::signart::Hazard, retro::signart::Electric, 1.2f, .6f);
    signDesign.art = std::make_shared<retro::signart::Image>(picture), signDesign.artName = "test art";
    tool = 7;
    chooseKind("SIGN");
    remember();
    int added = authorKind("SIGN", cx + .5f, cy + .3f, cx + .5f, cy + .3f);
    if (added < 0) return fail("authorKind made no sign");
    record = added;
    auto f = NativeDocument::fields(doc.lines[size_t(added)]);
    if (f.size() != 15 || f[11] != "1" || f[12] != "2" || decoded(f[14]) != "test art") return fail("sign record fields: " + doc.lines[size_t(added)]);
    if (!signArtLine("test art")) return fail("SIGN_ART record missing");
    if (!validateDocument()) return fail("engine rejected the designed sign: " + utf8(notice));
    report += "designed sign with picture validates: " + doc.lines[size_t(added)].substr(0, 90) + "...\n";
    // Restyle the picked sign through the designer path.
    signDesign.style = retro::signart::Stencil, signDesign.art = nullptr, signDesign.artName.clear();
    applySignToPicked(SignStyle);
    if (signArtLine("test art")) return fail("unused SIGN_ART was not removed");
    f = NativeDocument::fields(doc.lines[size_t(record)]);
    if (f[0] != "SIGN" || f.size() < 12 || f[11] != "3") return fail("restyle did not apply: " + doc.lines[size_t(record)]);
    if (std::abs(number(f[3], 0) - (cy + .01f)) > 1e-3f) return fail("painted sign is not flush with the wall: y " + f[3]);
    if (!validateDocument()) return fail("engine rejected the restyled sign");
    // Files.
    fs::path folder = root / L".build" / L"sign-designer-test";
    std::error_code ec;
    fs::create_directories(folder, ec);
    signDesign = preset("EXIT", "LEVEL 2", 0x1c8048, retro::signart::Route, retro::signart::ArrowRight, 1.4f, .45f, 0xfafafa);
    signDesign.art = std::make_shared<retro::signart::Image>(picture), signDesign.artName = "logo";
    std::wstring why;
    if (!exportSign(folder / L"exit.rmsign", why)) return fail(utf8(why));
    SignDesign back;
    if (!readSignFile(folder / L"exit.rmsign", back, why)) return fail(utf8(why));
    if (back.title != "EXIT" || back.subtitle != "LEVEL 2" || back.style != 4 || back.icon != 4 || back.ink != 0xfafafa || !back.art ||
        back.art->pixels != picture.pixels || std::abs(back.width - 1.4f) > 1e-4f)
      return fail(".rmsign round trip changed the design");
    if (!exportSign(folder / L"exit.png", why)) return fail(utf8(why));
    auto raster = retro::signart::draw(artDesign(signDesign), 1.4f / .45f);
    {
      std::ifstream in(folder / L"exit.png", std::ios::binary);
      std::string bytes(std::istreambuf_iterator<char>(in), {});
      int w = 0, h = 0, n = 0;
      unsigned char *png = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(bytes.data()), int(bytes.size()), &w, &h, &n, 4);
      if (!png) return fail("exported PNG does not decode");
      bool same = w == raster.width && h == raster.height;
      for (size_t i = 0; same && i < raster.pixels.size(); ++i) {
        auto p = raster.pixels[i];
        same = png[i * 4] == ((p >> 16) & 255) && png[i * 4 + 1] == ((p >> 8) & 255) && png[i * 4 + 2] == (p & 255);
      }
      stbi_image_free(png);
      if (!same) return fail("exported PNG differs from the engine's raster");
    }
    report += "rmsign and PNG round trips match (" + std::to_string(raster.width) + "x" + std::to_string(raster.height) + ")\n";
    // Importing a picture turns a plain plate into a picture sign at its proportions.
    signDesign = SignDesign{};
    if (!importSign(folder / L"exit.png", why) || signDesign.style != retro::signart::Picture || !signDesign.art) return fail("picture import: " + utf8(why));
    // Every style and symbol renders, and the legacy plate keeps its 512 x 144 size.
    for (int s = 0; s < retro::signart::StyleCount; ++s)
      for (int i = 0; i < retro::signart::IconCount; ++i) {
        SignDesign d = preset("TITLE", "SMALL WORDS", 0x3c8cd8, s, i, 1.5f, .5f);
        auto r = retro::signart::draw(artDesign(d), 3.f);
        if (r.width <= 0 || r.pixels.size() != size_t(r.width) * r.height) return fail("style " + std::to_string(s) + " symbol " + std::to_string(i) + " drew nothing");
      }
    auto plain = retro::signart::draw(artDesign(preset("PLAIN", "SIGN", 0x3c8cd8, 0, 0, 1.6f, .55f)), 2.9f);
    if (plain.width != 512 || plain.height != 144) return fail("the original plate changed size");
    report += "all 7 styles x 11 symbols draw; the original plate stays 512x144\n";
    history.clear(), future.clear();
    return true;
  }
