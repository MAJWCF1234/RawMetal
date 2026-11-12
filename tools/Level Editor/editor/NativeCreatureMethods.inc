// Creature quarry and hunter name: CREATURE field 9 (hunter) and 10 (prey),
// read by the engine in src/world/CustomCampaign.cpp and played by
// src/game/ai/enemy/Hunter.cpp. Included inside struct Editor.
//
// prey is a person on the creature's own map, by actor index: the creature
// comes out for them first. hunter names a recurring animal: it cannot die,
// it runs and is gone, and spawns sharing the name anywhere in the campaign
// are the same animal coming back.
struct MapPerson {
  int index = -1;
  std::wstring name;
  float x = 0, y = 0;
  bool placed = false;
};
// The people on a map, by actor index, named the way the Person tool names
// them (plus the name they speak under, when they talk) and where they stand.
std::vector<MapPerson> mapPeople(int mapIndex) const {
  std::vector<MapPerson> out;
  for (auto i : doc.records("ACTOR", mapIndex)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() < 8)
      continue;
    MapPerson p;
    p.index = int(number(f[7], -1));
    int visual = int(number(f[3], -1));
    p.name = L"Person";
    for (auto &role : people)
      if (role.visual == visual && role.mode != 3) {
        p.name = role.label;
        break;
      }
    out.push_back(p);
  }
  auto find = [&](const std::string &text) -> MapPerson * {
    int index = int(number(text, -1));
    for (auto &p : out)
      if (p.index == index)
        return &p;
    return nullptr;
  };
  std::map<std::string, std::string> speakers; // dialogue node -> speaker
  for (auto i : doc.records("DIALOGUE", mapIndex)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() > 3 && !f[3].empty())
      speakers.emplace(f[2], decoded(f[3]));
  }
  for (auto i : doc.records("ACTOR_AI", mapIndex)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() > 3 && f[3] == "3")
      if (auto *p = find(f[2]); p && p->name == L"Cleanup")
        p->name = L"Security guard";
  }
  for (auto i : doc.records("ACTOR_TALK", mapIndex)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() > 3)
      if (auto *p = find(f[2]); p && speakers.count(f[3]))
        p->name += L" “" + wide(speakers[f[3]]) + L"”";
  }
  for (auto i : doc.records("ACTOR_KEY", mapIndex)) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() > 5)
      if (auto *p = find(f[2]); p && !p->placed)
        p->x = number(f[4], 0), p->y = number(f[5], 0), p->placed = true;
  }
  return out;
}
std::wstring personLabel(const MapPerson &p) const {
  return std::to_wstring(p.index) + L"  " + p.name +
         (p.placed ? L"  at " + wide(trimmed(p.x)) + L", " + wide(trimmed(p.y)) : std::wstring{});
}
bool creaturePicked() const {
  return record >= 0 && record < int(doc.lines.size()) && selectedRecords.size() <= 1 &&
         NativeDocument::fields(doc.lines[size_t(record)])[0] == "CREATURE";
}
std::string creatureField(size_t index) const {
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  return index < f.size() ? f[index] : std::string{};
}
// Hunter names already given to a creature anywhere in the campaign.
std::vector<std::string> hunterNames() const {
  std::set<std::string> names;
  for (auto i : doc.records("CREATURE")) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() > 9 && !f[9].empty())
      names.insert(decoded(f[9]));
  }
  return {names.begin(), names.end()};
}
// Fills the quarry and hunter boxes for the picked creature.
void syncCreatureControls() {
  if (!preyList || !hunterList || !creaturePicked())
    return;
  auto f = NativeDocument::fields(doc.lines[size_t(record)]);
  int mapIndex = f.size() > 1 ? int(number(f[1], 0)) : map;
  std::string prey = f.size() > 10 ? f[10] : "";
  SendMessageW(preyList, CB_RESETCONTENT, 0, 0);
  SendMessageW(preyList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Nobody: it hunts the player"));
  SendMessageW(preyList, CB_SETITEMDATA, 0, LPARAM(-1));
  LRESULT chosen = 0;
  for (auto &p : mapPeople(mapIndex)) {
    auto n = SendMessageW(preyList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(personLabel(p).c_str()));
    SendMessageW(preyList, CB_SETITEMDATA, n, LPARAM(p.index));
    if (!prey.empty() && int(number(prey, -2)) == p.index)
      chosen = n;
  }
  if (!prey.empty() && chosen == 0) { // a quarry that is not on this map (yet)
    auto n = SendMessageW(preyList, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>((L"Person " + wide(prey) + L" (not on this map)").c_str()));
    SendMessageW(preyList, CB_SETITEMDATA, n, LPARAM(int(number(prey, -1))));
    chosen = n;
  }
  SendMessageW(preyList, CB_SETCURSEL, chosen, 0);
  // A name being typed is left alone until the box is left.
  HWND focus = GetFocus();
  if (focus && (focus == hunterList || GetParent(focus) == hunterList))
    return;
  SendMessageW(hunterList, CB_RESETCONTENT, 0, 0);
  for (auto &name : hunterNames())
    SendMessageW(hunterList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(name).c_str()));
  setControlText(hunterList, wide(f.size() > 9 ? decoded(f[9]) : std::string{}).c_str());
}
// Writes the picked creature's hunter (9) or prey (10) field as one undoable edit.
void setCreatureField(size_t index, const std::string &value) {
  if (!creaturePicked())
    return;
  if (!editableRecord(size_t(record))) {
    message(L"Unlock and show this floor before editing its content.");
    return;
  }
  auto before = NativeDocument::fields(doc.lines[size_t(record)]);
  auto f = before;
  if (f.size() <= index)
    f.resize(index + 1);
  f[index] = value;
  while (f.size() > 6 && f.back().empty())
    f.pop_back();
  if (f == before)
    return;
  remember();
  doc.replace(size_t(record), f);
  publish();
  fillFields(false);
  syncCreatureControls();
  InvalidateRect(plan, nullptr, FALSE);
  if (index == 10)
    message(value.empty() ? L"It hunts the player." : L"It comes out for them first. The plan draws the line. Ctrl+Z takes it back.");
  else
    message(value.empty() ? L"An ordinary creature again: it can die." : L"A recurring animal: it cannot die, it runs and comes back. Ctrl+Z takes it back.");
}
void creatureControlChanged(int id, int notification) {
  if (id == PreyList && notification == CBN_SELCHANGE) {
    auto n = SendMessageW(preyList, CB_GETCURSEL, 0, 0);
    int index = n < 0 ? -1 : int(SendMessageW(preyList, CB_GETITEMDATA, n, 0));
    setCreatureField(10, index < 0 ? std::string{} : std::to_string(index));
  }
  if (id == HunterList && (notification == CBN_SELCHANGE || notification == CBN_KILLFOCUS)) {
    std::wstring text;
    if (notification == CBN_SELCHANGE) {
      auto n = SendMessageW(hunterList, CB_GETCURSEL, 0, 0);
      if (n >= 0) {
        text.resize(size_t(std::max<LRESULT>(0, SendMessageW(hunterList, CB_GETLBTEXTLEN, n, 0))) + 1);
        SendMessageW(hunterList, CB_GETLBTEXT, n, reinterpret_cast<LPARAM>(text.data()));
        text.resize(wcslen(text.c_str()));
      }
    } else
      text = label(hunterList);
    auto name = utf8(text);
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
      name.pop_back();
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
      name.erase(name.begin());
    setCreatureField(9, encoded(name));
  }
}
