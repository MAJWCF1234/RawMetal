// The 3D view's light: lit as the game draws it, the light probes' fill alone
// (the engine's --detail-probes), the finish (--detail-finish: crystal red,
// metal blue, polish grey) or no probes (--no-probes, the lamps' flat bounce);
// Rebake, which bakes every map's probes again from the map as it stands; the
// air's clock between edits; and play over Claire's shoulder. They go to the
// picture in look.txt beside its control file (src/platform/EditorPreview.cpp),
// which answers in look-status.txt. Probes need no record: the engine bakes them
// from the lamps, the room and its crystals. A crystal's glow is its model's and
// its size's, so the inspector only says what it is.
// Included inside struct Editor.
int lightView = 0, rebakeSerial = 0;
bool airMoves = true, thirdPersonPlay = false;
unsigned probeBakesSeen = 0;
bool rebakeAsked = false;
std::string lookLast;
static constexpr const wchar_t *lightViewNames[] = {L"lit", L"probe fill", L"finish", L"no probes"};
std::string lookText() const {
  return "LOOK " + std::to_string(lightView) + ' ' + std::to_string(int(thirdPersonPlay)) + ' ' +
         std::to_string(rebakeSerial) + ' ' + std::to_string(int(airMoves)) + '\n';
}
fs::path lookFile() const { return control.parent_path() / L"look.txt"; }
fs::path lookStatusFile() const { return control.parent_path() / L"look-status.txt"; }
void writeLook() {
  if (control.empty())
    return;
  auto text = lookText();
  if (text == lookLast && engineLive)
    return;
  lookLast = text;
  try {
    channelPut(lookFile(), text);
  } catch (...) {
  }
  if (HWND b = GetDlgItem(window, LightViewButton))
    SetWindowTextW(b, (L"☀ Light: " + std::wstring(lightViewNames[std::clamp(lightView, 0, 3)]) + L" ▾").c_str());
  if (HWND b = GetDlgItem(window, ThirdPersonToggle))
    SetWindowTextW(b, thirdPersonPlay ? L"Play: over shoulder" : L"Play: first person");
}
void setLightView(int view) {
  lightView = std::clamp(view, 0, 3);
  writeLook();
  static constexpr const wchar_t *said[] = {
      L"Lit: the picture as the game draws it.",
      L"Probe fill: only the light the probes carry, the lamps' bounce and the crystals' glow, as the game bakes it.",
      L"Finish: crystal red, metal blue, polish grey. Black takes no shine.",
      L"No probes: the lamps' flat room bounce, as before the probes. Compare with Lit."};
  message(said[lightView]);
}
void rebakeLight() {
  ++rebakeSerial;
  rebakeAsked = true;
  writeLook();
  message(L"Baking the light probes again from the map as it stands...");
}
void setThirdPersonPlay(bool on) {
  thirdPersonPlay = on;
  writeLook();
  message(on ? L"Play walks Claire with the camera over her shoulder. F5 to play."
             : L"Play is first person again.");
}
// The menu under the Light button.
void chooseLightView() {
  HMENU menu = CreatePopupMenu();
  for (int i = 0; i < 4; ++i) {
    static constexpr const wchar_t *items[] = {L"Lit (as the game)", L"Probe fill only", L"Finish: crystal, metal, polish",
                                               L"No probes (flat bounce)"};
    AppendMenuW(menu, MF_STRING | (lightView == i ? MF_CHECKED : 0), LightMenuBase + i, items[i]);
  }
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, LightMenuBase + 4, L"Rebake light probes now");
  AppendMenuW(menu, MF_STRING | (airMoves ? MF_CHECKED : 0), LightMenuBase + 5, L"Air moves between edits");
  RECT r{};
  GetWindowRect(GetDlgItem(window, LightViewButton), &r);
  int chosen = int(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN, r.left, r.bottom, 0, window, nullptr));
  DestroyMenu(menu);
  lightMenuChosen(chosen);
}
void lightMenuChosen(int chosen) {
  if (chosen >= LightMenuBase && chosen < LightMenuBase + 4)
    setLightView(chosen - LightMenuBase);
  else if (chosen == LightMenuBase + 4)
    rebakeLight();
  else if (chosen == LightMenuBase + 5) {
    airMoves = !airMoves;
    writeLook();
    message(airMoves ? L"The air moves in the picture between edits." : L"The air holds still between edits.");
  }
}
// The picture's answer (the status timer): a rebake that landed is said once.
struct LookStatus {
  int mode = 0, third = 0, playing = 0;
  unsigned bakes = 0;
  double bakeMs = 0;
  int sources = 0, particles = 0;
  float clock = 0;
  int outside = 0, rebake = 0;
  bool read = false;
};
LookStatus lookStatus() const {
  LookStatus s;
  if (control.empty())
    return s;
  std::istringstream in(channelText(lookStatusFile()));
  std::string tag;
  if (in >> tag >> s.mode >> s.third >> s.playing >> s.bakes >> s.bakeMs >> s.sources >> s.particles >> s.clock >> s.outside >>
          s.rebake &&
      tag == "LOOK")
    s.read = true;
  return s;
}
void readLook() {
  if (!rebakeAsked)
    return;
  auto s = lookStatus();
  if (!s.read || s.rebake != rebakeSerial)
    return;
  if (probeBakesSeen == 0)
    probeBakesSeen = s.bakes;
  if (s.bakes > probeBakesSeen) {
    probeBakesSeen = s.bakes;
    rebakeAsked = false;
    std::wostringstream said;
    said.setf(std::ios::fixed);
    said.precision(0);
    said << L"Light probes baked again (" << s.bakeMs << L" ms). Lamps, crystals and walls are in the fill.";
    message(said.str());
  }
}
// A resonance crystal fixture (res_crystal_l/m/s/wall) glows by its model and its size
// (src/renderer/LightProbes.cpp crystalEmitters): what that gives, for the inspector.
std::wstring crystalGlow() const {
  if (record < 0 || record >= int(doc.lines.size()) || selectedRecords.size() > 1)
    return {};
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  if (f.size() < 9 || f[0] != "FIXTURE")
    return {};
  int model = int(number(f[2], -1));
  namespace m = retro::model;
  if (model < m::res_crystal_l || model > m::res_crystal_wall)
    return {};
  float nominal = model < int(std::size(m::size)) ? m::size[model][2] : 1.f;
  float scale = std::clamp(number(f[8], nominal) / std::max(.1f, nominal), .3f, 2.f);
  float power = model == m::res_crystal_l ? 4.f : model == m::res_crystal_m ? 2.f : model == m::res_crystal_s ? .8f : 1.6f;
  float reach = model == m::res_crystal_l ? 6.f : model == m::res_crystal_m ? 4.5f : model == m::res_crystal_s ? 2.8f : 3.6f;
  std::wostringstream out;
  out.setf(std::ios::fixed);
  out.precision(1);
  out << L"Cyan glow " << power * scale << L", reaches " << reach * std::sqrt(scale) << L" m";
  return out.str();
}
