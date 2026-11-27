  // The lines in this window are views of the same records the game reads.
  // An event joins its "when", "only if", and ordered "then" blocks by name.
  HWND aiManagerWindow = nullptr;
  HFONT aiFont = nullptr;
  int aiManagerFocus = 0;
  int aiHeldBlock = -1, aiSnapRow = -1, aiRenameLine = -1;
  int aiPadTyping = 0, aiPadCharacter = 0;
  int aiChipLine = -1, aiChipField = -1;
  int aiPadChip = 0;
  std::vector<std::string> aiChipOptions;
  bool aiChipListed = false;   // the setting open now is picked from a fixed list (aiChoices)
  unsigned long long aiTraceSerial = 0;
  std::uint32_t aiTraceEvent = 0;
  std::map<int, ULONGLONG> aiFlashUntil;
  std::wstring aiLog;
  std::array<bool, 6> aiCategoryOpen{true, true, true, true, true, true};
  std::map<int, std::wstring> aiEntityUsage;
  enum { AiOwners = 1, AiRules, AiSteps, AiEntities, AiPalette, AiAddDirector,
         AiAddRule, AiAddStep, AiEdit, AiRemoveStep, AiUndo, AiRedo, AiDone,
         AiSearch, AiName, AiRename, AiTest, AiChipValue = 21, AiChipApply, AiRuleTitle,
         AiFocusText, AiUsedBy, AiPlayLog, AiTarget, AiLinkedDirector };
  inline static constexpr const wchar_t *aiBlockNames[] = {
    L"◆ Doors  /  Switch is on", L"◆ Doors  /  Breaker restored",
    L"◷ Pacing  /  Wait, then set a switch", L"✦ Lights & sound  /  Change the lights",
    L"✦ Lights & sound  /  Clank in the walls", L"▤ Messages  /  Show a message",
    L"◆ Doors  /  Close picked door", L"◆ Doors  /  Open picked door",
    L"▲ Creatures  /  Send in a machine", L"▲ Creatures  /  Wake caged robots in order",
    L"▲ Creatures  /  Wake the stalker", L"◆ Doors  /  All breakers on",
    L"◷ Pacing  /  Fight, quiet, fight", L"◆ Doors  /  Lock behind player",
    L"✦ Lights & sound  /  Fake voice", L"● Player  /  When everyone enters"
  };

  HWND aiControl(int id) const { return GetDlgItem(aiManagerWindow, id); }
  std::wstring aiRowText(int id, int row = -1) const {
    HWND list = aiControl(id);
    if (row < 0) row = int(SendMessageW(list, LB_GETCURSEL, 0, 0));
    const auto length = SendMessageW(list, LB_GETTEXTLEN, row, 0);
    if (length < 0) return {};
    std::wstring label(size_t(length) + 1, L'\0');
    SendMessageW(list, LB_GETTEXT, row, reinterpret_cast<LPARAM>(label.data()));
    label.resize(size_t(length));
    return label;
  }
  void aiShowFocus(int id, int row = -1) {
    auto title = id <= AiPalette ? aiRowText(id, row) : aiText(aiControl(id));
    if (id == AiSteps) {
      auto list = aiControl(AiSteps);
      const auto line = row >= 0 ? int(SendMessageW(list, LB_GETITEMDATA, row, 0)) : aiListRecord(AiSteps);
      if (line >= 0 && size_t(line) < doc.lines.size())
        for (const auto &chip : aiChips(size_t(line))) title += L"   [" + chip.text + L"]";
    }
    SetWindowTextW(aiControl(AiFocusText), title.c_str());
  }
  static LRESULT CALLBACK aiHoverProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
    auto &editor = *reinterpret_cast<Editor *>(ref);
    if (msg == WM_MOUSEMOVE && GetCapture() != h) {
      const auto row = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
      if (!HIWORD(row)) editor.aiShowFocus(GetDlgCtrlID(h), LOWORD(row));
    }
    return DefSubclassProc(h, msg, w, l);
  }
  static LRESULT CALLBACK aiSearchProc(HWND h, UINT msg, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
    const auto result = DefSubclassProc(h, msg, w, l);
    if (msg == WM_PAINT && GetFocus() != h && GetWindowTextLengthW(h) == 0) {
      auto &editor = *reinterpret_cast<Editor *>(ref);
      HDC dc = GetDC(h);
      RECT rect{};
      GetClientRect(h, &rect);
      rect.left += 4;
      SelectObject(dc, editor.aiFont);
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, RGB(173, 166, 151));
      DrawTextW(dc, L"Find...", 7, &rect, DT_SINGLELINE | DT_VCENTER);
      ReleaseDC(h, dc);
    }
    if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS) InvalidateRect(h, nullptr, TRUE);
    return result;
  }
  void aiShowUsage() {
    const auto it = aiEntityUsage.find(aiListRecord(AiEntities));
    auto text = L"Used by: " + (it == aiEntityUsage.end() ? std::wstring(L"None") : it->second);
    SetWindowTextW(aiControl(AiUsedBy), text.c_str());
  }
  static std::uint32_t aiScriptId(std::string_view name) {
    std::uint32_t hash = 2166136261u;
    for (unsigned char c : name) hash = (hash ^ c) * 16777619u;
    hash &= 0x7fffffu;
    return hash ? hash : 1u;
  }
  void aiReadTrace() { aiConsumeTrace(channelText(status)); }
  // Test it starts a fresh run, where nothing a rule waits for has happened yet. This is what must be set for
  // the rule to run: each switch it requires, to a value that meets it. "id value" lines for the preview, and the
  // same in words for the play log.
  std::pair<std::string, std::wstring> aiTestSetup(const std::string &event) const {
    std::string states;
    std::wstring words;
    for (auto line : doc.records("SCRIPT_REQUIRE", map)) {
      const auto f = NativeDocument::fields(doc.lines[line]);
      if (f.size() < 6 || f[2] != event || f[3].empty() || f[3][0] == '@') continue;   // @health, @kills: the game's own
      const auto name = decoded(f[3]);
      const int want = int(number(f[5], 0));
      const auto &op = f[4];
      const int value = op == "!=" || op == ">" ? want + 1 : op == "<" ? want - 1 : want;
      const std::uint32_t id = name[0] == '#' ? std::uint32_t(std::strtoul(name.c_str() + 1, nullptr, 10)) : aiScriptId(name);
      states += std::to_string(id) + ' ' + std::to_string(value) + '\n';
      std::wstring label = name.ends_with("_beat") ? L"Director beat"
                           : name.starts_with("eng_breaker_") ? L"Breaker " + wide(name.substr(12))
                                                              : aiStateName(f[3]);
      words += (words.empty() ? L"" : L", ") + label + L" = " +
               (value == 1 && !name.ends_with("_beat") ? std::wstring(L"On") : std::to_wstring(value));
    }
    return {states, words};
  }
  void aiConsumeTrace(const std::string &snapshot) {
    std::istringstream input(snapshot);
    std::string line;
    std::getline(input, line);
    bool changed = false;
    while (std::getline(input, line)) {
      if (!line.starts_with("AI_TRACE ")) continue;
      std::istringstream entry(line.substr(9));
      unsigned long long serial;
      std::uint32_t event;
      int action;
      if (!(entry >> serial >> event >> action) || serial <= aiTraceSerial) continue;
      aiTraceSerial = serial;
      if (!aiTraceEvent) continue;
      HWND graph = aiControl(AiSteps);
      for (int row = 0, index = 0, count = int(SendMessageW(graph, LB_GETCOUNT, 0, 0)); row < count; ++row) {
        const auto at = size_t(SendMessageW(graph, LB_GETITEMDATA, row, 0));
        if (at >= doc.lines.size()) continue;
        const auto fields = NativeDocument::fields(doc.lines[at]);
        if(fields.size()<3||aiScriptId(decoded(fields[2]))!=event)continue;
        const bool isAction = fields[0] == "SCRIPT_ACTION";
        if ((isAction && index++ == action) || (!isAction && action == -1)) {
          aiFlashUntil[row] = GetTickCount64() + 750;
          if (isAction||fields[0]=="SCRIPT_EVENT") {
            auto caption=aiRowText(AiSteps,row);
            const auto target=aiNamed(at,"editortarget");
            if(!target.empty())caption+=L": "+wide(target);
            else if(fields[0]=="SCRIPT_EVENT")for(const auto &chip:aiChips(at))caption+=L" "+chip.text;
            aiLog+=caption+L"\r\n";
            if(aiLog.size()>3000)aiLog.erase(0,aiLog.find(L"\n")+1);
          }
          changed = true;
        }
      }
    }
    if (changed) {
      HWND log=aiControl(AiPlayLog);
      SetWindowTextW(log,aiLog.c_str());
      SendMessageW(log,EM_SETSEL,aiLog.size(),aiLog.size());
      SendMessageW(log,EM_SCROLLCARET,0,0);
    }
    const auto now = GetTickCount64();
    changed |= std::erase_if(aiFlashUntil, [now](const auto &entry) { return entry.second <= now; }) > 0;
    if (changed) InvalidateRect(aiControl(AiSteps), nullptr, FALSE);
  }
  void aiRefreshPalette(int selected) {
    static const wchar_t *names[] = {L"● Player", L"◆ Doors & switches", L"✦ Lights & sound",
                                     L"▲ Creatures", L"◷ Pacing", L"▤ Messages"};
    static const int blocks[][7] = {{15,-1}, {0,1,6,7,11,13,-1}, {3,-1},
                                   {8,9,10,-1}, {2,12,4,14,-1}, {5,-1}};
    SendMessageW(aiControl(AiPalette), LB_RESETCONTENT, 0, 0);
    for (int group = 0; group < 6; ++group) {
      aiListRow(AiPalette, std::wstring(aiCategoryOpen[group] ? L"− " : L"+ ") + names[group], -100 - group);
      if (!aiCategoryOpen[group]) continue;
      for (int item : blocks[group]) {
        if (item < 0) break;
        std::wstring_view title = aiBlockNames[item];
        aiListRow(AiPalette, L"    " + std::wstring(title.substr(title.find(L" / ") + 3)), item);
      }
    }
    aiRestoreRow(AiPalette, selected);
  }
  void aiToggleCategory(int choice) {
    const int group = -100 - choice;
    if (group < 0 || group >= int(aiCategoryOpen.size())) return;
    aiCategoryOpen[group] = !aiCategoryOpen[group];
    aiRefreshPalette(choice);
  }
  std::wstring aiLabel(size_t line, std::wstring fallback) const {
    for (const auto &[key, value] : NativeDocument::named(doc.lines[line]))
      if (key == "editorlabel" && !value.empty()) return wide(decoded(value));
    return fallback;
  }
  std::string aiNamed(size_t line,std::string_view key) const {
    for (const auto &[name,value] : NativeDocument::named(doc.lines[line]))
      if (name==key) return decoded(value);
    return {};
  }
  std::string aiRuleGroup() const {
    const int line=aiListRecord(AiRules);
    return line>=0&&size_t(line)<doc.lines.size()?aiNamed(size_t(line),"editorgroup"):std::string{};
  }
  static std::wstring aiText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(size_t(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(size_t(length));
    return text;
  }
  int aiListRecord(int id) const {
    HWND list = aiControl(id);
    auto row = SendMessageW(list, LB_GETCURSEL, 0, 0);
    return row == LB_ERR ? -1 : int(SendMessageW(list, LB_GETITEMDATA, row, 0));
  }
  void aiListRow(int id, const std::wstring &label, int line) {
    HWND list = aiControl(id);
    auto row = SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    if (row < 0) return;
    SendMessageW(list, LB_SETITEMDATA, row, LPARAM(line));
    HDC dc = GetDC(list);
    const auto old = SelectObject(dc, aiFont ? aiFont : font);
    SIZE extent{};
    GetTextExtentPoint32W(dc, label.c_str(), int(label.size()), &extent);
    SelectObject(dc, old);
    ReleaseDC(list, dc);
    if (id != AiSteps && extent.cx + 8 > SendMessageW(list, LB_GETHORIZONTALEXTENT, 0, 0))
      SendMessageW(list, LB_SETHORIZONTALEXTENT, WPARAM(extent.cx + 8), 0);
  }
  void aiRestoreRow(int id, int line) {
    HWND list = aiControl(id);
    const int count = int(SendMessageW(list, LB_GETCOUNT, 0, 0));
    if (!count) return;
    for (int row = 0; row < count; ++row) {
      if (int(SendMessageW(list, LB_GETITEMDATA, row, 0)) == line) {
        SendMessageW(list, LB_SETCURSEL, row, 0);
        return;
      }
    }
    SendMessageW(list, LB_SETCURSEL, 0, 0);
  }
  std::string aiRuleName() const {
    const int index = aiListRecord(AiRules);
    if (index < 0 || size_t(index) >= doc.lines.size()) return {};
    auto fields = NativeDocument::fields(doc.lines[size_t(index)]);
    return fields.size() > 2 && fields[0] == "SCRIPT_EVENT" &&
        fields[1] == std::to_string(map) ? fields[2] : std::string{};
  }
  std::string aiOwnerPrefix() const {
    const int index = aiListRecord(AiOwners);
    if (index < 0 || size_t(index) >= doc.lines.size()) return {};
    const auto fields = NativeDocument::fields(doc.lines[size_t(index)]);
    if (fields.size() < 3 || fields[1] != std::to_string(map)) return {};
    if (fields[0] == "AI") return fields[2] + "_rule_";
    if (fields[0] == "ACTOR" && fields.size() >= 8)
      return "squad_" + fields[1] + "_" + fields[7] + "_rule_";
    return {};
  }
  void refreshAiSteps() {
    const int previous = aiListRecord(AiSteps);
    const int choice = aiListRecord(AiRules);
    const auto title = choice < 0 ? std::wstring(L"Choose a rule") : aiLabel(size_t(choice),aiRowText(AiRules));
    SetWindowTextW(aiControl(AiRuleTitle), title.c_str());
    SendMessageW(aiControl(AiSteps), LB_RESETCONTENT, 0, 0);
    auto rule = aiRuleName();
    if (rule.empty()) return;
    std::vector<std::string> included{rule};
    const auto group = aiRuleGroup();
    if (!group.empty()) for (auto line : doc.records("SCRIPT_EVENT",map)) {
      const auto fields=NativeDocument::fields(doc.lines[line]);
      if (fields.size()>2&&fields[2]!=rule&&aiNamed(line,"editorgroup")==group) included.push_back(fields[2]);
    }
    for (const auto &event : included)
    for (const auto *tag : {"SCRIPT_EVENT", "SCRIPT_TRIGGER", "SCRIPT_REQUIRE", "SCRIPT_ACTION"}) {
      for (auto line : doc.records(tag, map)) {
        const auto fields = NativeDocument::fields(doc.lines[line]);
        if (fields.size() < 3 || fields[2] != event) continue;
        std::wstring label = L"When the player enters";
        if (fields[0]=="SCRIPT_EVENT" && aiWholeMap(fields)) label=L"At any time, in";
        if (fields[0]=="SCRIPT_EVENT" && event!=rule) label=L"Next step: "+aiLabel(line,L"Follow-up");
        if (fields[0] == "SCRIPT_TRIGGER") label = L"Where it runs";
        if (fields[0] == "SCRIPT_REQUIRE") label = L"Only if";
        if (fields[0] == "SCRIPT_ACTION" && fields.size() > 3) {
          const auto &action = fields[3];
          label = action == "STATE" ? L"Set" : action == "LATER" ? L"Wait, then set" :
                  action == "LIGHTS" ? L"Change the lights" : action == "NOISE" ? L"Play a sound" :
                  action == "MESSAGE" ? L"Show a message" : L"Do an action";
        }
        if (fields[0] == "SCRIPT_REQUIRE" && fields.size() > 3) {
          const auto &state = fields[3];
          label = state.ends_with("_beat") ? L"Only after" :
                  state.starts_with("eng_breaker_") ? L"Breaker " + wide(state.substr(12)) + L" is restored" :
                  state.starts_with("breaker_") ? L"Breaker " + wide(state.substr(8)) + L" is restored" :
                  L"Only if";
        } else if (fields[0] == "SCRIPT_ACTION" && fields.size() > 4) {
          if (fields[3] == "MESSAGE") label += L"   [" + wide(decoded(fields[4])) + L"]";
          if (fields[3] == "RAW" && fields.size() > 4)
            label = fields[4] == "4" ? L"Open the picked door" : fields[4] == "5" ? L"Close the picked door" : L"Send in a creature";
          if (fields[3] == "NOISE") label = fields[4].starts_with("pa_system") ? L"Sound the alarm" :
            fields[4].starts_with("pa_") ? L"Play a fake voice" : L"Clank in the walls";
        }
        if (fields[0]=="SCRIPT_ACTION" && aiNamed(line,"editortarget")=="Vent fans") label=L"Switch off";
        aiListRow(AiSteps, fields[0] == "SCRIPT_EVENT" ? label : aiLabel(line, label), int(line));
      }
    }
    aiRestoreRow(AiSteps, previous);
  }
  void refreshAiManager() {
    if (!aiManagerWindow) return;
    int owner = aiListRecord(AiOwners), rule = aiListRecord(AiRules);
    int entity = aiListRecord(AiEntities);
    for (int id : {AiOwners, AiRules, AiEntities})
      SendMessageW(aiControl(id), LB_RESETCONTENT, 0, 0);
    int directorNumber = 0;
    for (auto line : doc.records("AI", map)) {
      const auto fields = NativeDocument::fields(doc.lines[line]);
      if (fields.size() >= 4) {
        ++directorNumber;
        aiListRow(AiOwners, aiLabel(line, L"Director " + std::to_wstring(directorNumber)), int(line));
      }
    }
    for (auto line : doc.records("ACTOR", map)) {
      const auto fields = NativeDocument::fields(doc.lines[line]);
      // Cleanup, Engineer, Sharpshooter, Heavy, Medic and Rusher are actor visuals 4 through 9.
      if (fields.size() >= 8 && fields[3].size() == 1 && fields[3][0] >= '4' && fields[3][0] <= '9')
        aiListRow(AiOwners, aiLabel(line, L"Cleanup Squad member"), int(line));
    }
    aiListRow(AiOwners, L"Map rules", -1);
    aiRestoreRow(AiOwners, owner);
    const auto ownerPrefix = aiOwnerPrefix();
    auto search = aiText(aiControl(AiSearch));
    if (!search.empty()) CharLowerBuffW(search.data(), DWORD(search.size()));
    std::map<std::string, std::wstring> ruleNames;
    int ruleNumber = 0;
    for (auto line : doc.records("SCRIPT_EVENT", map)) {
      const auto fields = NativeDocument::fields(doc.lines[line]);
      if (fields.size() < 3) continue;
      auto title = aiLabel(line, L"Rule " + std::to_wstring(++ruleNumber));
      const auto group = aiNamed(line,"editorgroup");
      const auto caption = group=="Lockdown"?L"Hall entry":group=="Cages wake"?L"Robot cages":
                           group=="Stalker"?L"Skin-suit robot":group=="Breakers"?L"Room power":
                           group=="Pacing"?L"Fight / quiet / fight":nullptr;
      if ((ownerPrefix.empty() || fields[2].starts_with(ownerPrefix)) &&
          aiNamed(line,"editorhelper")!="1")
        aiListRow(AiRules, caption?caption:L"Rule "+std::to_wstring(ruleNumber), int(line));
      ruleNames.emplace(fields[2], std::move(title));
    }
    std::map<std::string, std::set<std::wstring>> usedBy;
    for (auto line : doc.records("SCRIPT_ACTION", map)) {
      const auto fields = NativeDocument::fields(doc.lines[line]);
      if (fields.size() < 6 || !ruleNames.contains(fields[2])) continue;
      if (fields.size() > 7 && fields[3] == "RAW" && (fields[4] == "4" || fields[4] == "5"))
        usedBy["door:" + fields[7]].insert(ruleNames.at(fields[2]));
      if (fields[3] == "STATE" || fields[3] == "LATER")
        usedBy["switch:" + fields[4]].insert(ruleNames.at(fields[2]));
    }
    for (auto line : doc.records("SCRIPT_REQUIRE", map)) {
      const auto fields = NativeDocument::fields(doc.lines[line]);
      if (fields.size() > 3 && ruleNames.contains(fields[2]))
        usedBy["switch:" + fields[3]].insert(ruleNames.at(fields[2]));
    }
    std::map<std::string, int> entityNumbers;
    aiEntityUsage.clear();
    auto placed=doc.onMap(map);
    std::stable_sort(placed.begin(),placed.end(),[&](size_t left,size_t right) {
      std::string_view a,b;
      NativeDocument::field(doc.lines[left],0,a);NativeDocument::field(doc.lines[right],0,b);
      return a<b;
    });
    std::string activeKind;
    for (auto line : placed) {
      const auto fields = NativeDocument::fields(doc.lines[line]);
      if (fields.size() < 2 || fields[0].starts_with("#") || fields[0] == "ROW" ||
          fields[0] == "MAP" || fields[0] == "LAYER") continue;
      if (fields[0].starts_with("SCRIPT_") || fields[0]=="AI") continue;
      if (schemaFor(fields) < 0 && fields[0] != "AI") continue;
      const int ordinal = entityNumbers[fields[0]]++;
      auto label = plainKind(fields[0]) + L" " + std::to_wstring(ordinal + 1);
      for (const auto &[key, value] : NativeDocument::named(doc.lines[line]))
        if (key == "name") { label = wide(decoded(value)); break; }
      label = aiLabel(line, label);
      std::string usage;
      if (fields[0] == "DOOR") usage = "door:" + std::to_string(ordinal);
      if (fields[0] == "CREATURE" && fields.size() > 8 && !fields[8].empty())
        usage = "switch:" + fields[8];
      if (fields[0] == "TERMINAL" && fields.size() > 9 && !fields[9].empty())
        usage = "switch:" + fields[9];
      if (auto linked = usedBy.find(usage); linked != usedBy.end())
        for (const auto &rule : linked->second) {
          auto &text = aiEntityUsage[int(line)];
          if (!text.empty()) text += L", ";
          text += rule;
        }
      for (auto action : doc.records("SCRIPT_ACTION",map)) {
        if(aiNamed(action,"editortarget")!=utf8(label))continue;
        const auto f=NativeDocument::fields(doc.lines[action]);
        if(f.size()<3||!ruleNames.contains(f[2]))continue;
        auto &text=aiEntityUsage[int(line)];const auto &title=ruleNames.at(f[2]);
        if(text.find(title)==std::wstring::npos){if(!text.empty())text+=L", ";text+=title;}
      }
      auto folded = label;
      if (!folded.empty()) CharLowerBuffW(folded.data(), DWORD(folded.size()));
      if (folded.find(search) != std::wstring::npos) {
        if(activeKind!=fields[0]){
          activeKind=fields[0];aiListRow(AiEntities,L"-- "+plainKind(activeKind)+L" --",-2);
        }
        aiListRow(AiEntities, (fields[0] == "DOOR" ? L"◆  " : fields[0] == "CREATURE" ? L"▲  " :
                  fields[0] == "LIGHT" ? L"●  " : fields[0] == "AMBIENT" ? L"♪  " :
                  fields[0] == "TERMINAL" ? L"¤  " : L"■  ") + label, int(line));
      }
    }
    aiRestoreRow(AiRules, rule);
    aiRestoreRow(AiEntities, entity);
    if (aiListRecord(AiEntities) < 0) SendMessageW(aiControl(AiEntities), LB_SETCURSEL, 1, 0);
    aiShowUsage();
    refreshAiSteps();
  }
  void aiShowRename(bool shown) {
    for (int id : {AiName, AiRename}) ShowWindow(aiControl(id), shown ? SW_SHOW : SW_HIDE);
  }
  void aiManagerChanged() {
    aiRenameLine = -1;
    aiShowRename(false);
    aiChipLine = -1;
    ShowWindow(aiControl(AiChipValue),SW_HIDE);
    ShowWindow(aiControl(AiTarget),SW_HIDE);
    ShowWindow(aiControl(AiChipApply),SW_HIDE);
    publish(true, false);
    refreshAiManager();
  }
  void aiDropBlock() {
    if (aiHeldBlock < 0) return;
    SendMessageW(aiControl(AiPalette), LB_SETCURSEL, aiHeldBlock, 0);
    aiHeldBlock = -1;
    SendMessageW(aiManagerWindow, WM_COMMAND, AiAddStep, 0);
    aiSnapRow = -1;
    InvalidateRect(aiControl(AiSteps), nullptr, TRUE);
  }
  struct AiChip { int field; std::wstring text; };
  const char* aiTargetKind(const std::vector<std::string>& f,std::string_view label) const {
    if (f.size()>=8&&f[3]=="RAW"&&(f[4]=="4"||f[4]=="5")) return "DOOR";
    if (f.size()>=6&&(f[3]=="STATE"||f[3]=="LATER")) {
      if (label.starts_with("Cage row")||label=="Robotics hall doors") return "DOOR";
      if (label=="Skin-suit robot") return "CREATURE";
      if (label=="Vent fans") return "AMBIENT";
      if (label.starts_with("Breaker")) return "TERMINAL";
    }
    if (f.size()>=6&&f[0]=="SCRIPT_REQUIRE"&&f[3].starts_with("eng_breaker_")) return "TERMINAL";
    return nullptr;
  }
  static bool aiWholeMap(const std::vector<std::string>& f) {
    return f.size()>6&&f[3]=="0"&&f[4]=="0"&&f[5]=="24"&&f[6]=="24";
  }
  // The name the map gives a switch: whatever an action that sets it points at.
  std::wstring aiStateName(const std::string& state) const {
    for (auto line : doc.records("SCRIPT_ACTION", map)) {
      const auto f = NativeDocument::fields(doc.lines[line]);
      if (f.size() < 5 || (f[3] != "STATE" && f[3] != "LATER") || f[4] != state) continue;
      if (const auto name = aiNamed(line, "editortarget"); !name.empty()) return wide(name);
    }
    auto name = wide(state);
    std::replace(name.begin(), name.end(), L'_', L' ');
    return name;
  }
  // The settings that can only be one of a known set are picked, never typed: what a lamp does, which
  // sound plays (the bank's one-off sounds; a loop would never end) and where it comes from.
  using AiChoice = std::pair<std::string, std::wstring>;   // what the record holds, what the list says
  std::vector<AiChoice> aiChoices(const std::vector<std::string> &f, int field) const {
    if (f.size() < 7 || f[0] != "SCRIPT_ACTION") return {};
    if (f[3] == "LIGHTS" && field == 5)
      return {{"normal", L"Normal"}, {"flicker", L"Flicker"}, {"dark", L"Dark"}, {"red", L"Emergency red"}};
    if (f[3] == "NOISE" && field == 6)
      return {{"behind", L"Behind the player"}, {"ahead", L"Ahead of the player"},
              {"lead", L"Where you were last heard"}, {"haunt", L"Where you are heard most"}};
    if (f[3] == "NOISE" && field == 4) {
      std::vector<AiChoice> sounds;
      const auto current = decoded(f[4]);
      for (const auto &row : bankSounds)
        if (!row.loop || row.name == current) sounds.push_back({row.name, row.label});
      return sounds;
    }
    return {};
  }
  std::wstring aiChoiceLabel(const std::vector<std::string> &f, int field) const {
    for (const auto &[value, label] : aiChoices(f, field))
      if (value == decoded(f[size_t(field)])) return label;
    return wide(decoded(f[size_t(field)]));
  }
  std::vector<AiChip> aiChips(size_t line) const {
    const auto f = NativeDocument::fields(doc.lines[line]);
    if (f.empty()) return {};
    if (f[0] == "SCRIPT_EVENT") {
      for (const auto &[key, value] : NativeDocument::named(doc.lines[line]))
        if (key == "editorarea") return {{-2, wide(decoded(value))}};
      return {{-2, aiWholeMap(f) ? L"Whole level" : L"Entry area"}};
    }
    const auto target = aiNamed(line,"editortarget");
    if (f.size()>=6&&f[0]=="SCRIPT_REQUIRE"&&f[3].starts_with("eng_breaker_"))
      return {{-3,L"Breaker "+wide(f[3].substr(12))}};
    if (f.size()>=6&&f[0]=="SCRIPT_REQUIRE") {
      if (f[3].ends_with("_beat")) return {{-4,L"Director beat"},{-4,wide(f[4]+" "+f[5])}};
      return {{-4,aiStateName(f[3])},{-4,f[4]=="="&&f[5]=="1"?L"is On":wide(f[4]+" "+f[5])}};
    }
    if (f.size() < 6 || f[0] != "SCRIPT_ACTION") return {};
    if (f[3] == "NOISE" && f.size() > 6) return {{4, aiChoiceLabel(f, 4)}, {6, aiChoiceLabel(f, 6)}};
    if (!target.empty()) {
      const int field = aiTargetKind(f,target) ? -3 : -4;
      if (f[3]=="LATER"&&f.size()>6) return {{field,wide(target)},{6,wide(f[6])+L" s"}};
      if (f[3]=="STATE"&&target!="Vent fans") return {{field,wide(target)},{5,f[5]=="1"?L"On":L"Off"}};
      if (f[3]=="LIGHTS"&&f.size()>6) return {{field,wide(target)},{5,aiChoiceLabel(f,5)}};
      return {{field,wide(target)}};
    }
    if (f[3] == "STATE") {
      std::wstring target = L"Switch";
      if (f[4].starts_with("breaker_")) target = L"Breaker " + wide(f[4].substr(8));
      return {{4, target}, {5, f[5] == "1" ? L"On" : f[5] == "0" ? L"Off" : wide(f[5])}};
    }
    if (f[3] == "MESSAGE") return {{4, wide(decoded(f[4]))}, {5, wide(f[5]) + L" s"}};
    if (f[3] == "LATER" && f.size() > 6) return {{6, wide(f[6]) + L" s"}};
    if (f[3] == "LIGHTS" && f.size() > 6) return {{5, aiChoiceLabel(f, 5)}};
    return {};
  }
  // Chips sit on the block's one line, straight after its sentence; drawing and clicking share these.
  std::vector<RECT> aiChipRects(HDC dc, const RECT &item, const std::wstring &label,
                                const std::vector<AiChip> &chips) const {
    auto wide_of = [dc](const std::wstring &text) {
      SIZE size{};
      GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
      return LONG(size.cx);
    };
    std::vector<RECT> rects;
    LONG left = item.left + 16 + wide_of(label) + 10;
    for (size_t index = 0; index < chips.size(); ++index) {
      const LONG room = item.right - 12 - left - LONG(chips.size() - 1 - index) * 80;
      const LONG width = std::clamp(wide_of(chips[index].text) + 16, 40L, std::max(40L, room));
      rects.push_back({left, item.top + 5, left + width, item.bottom - 5});
      left += width + 6;
    }
    return rects;
  }
  void aiBeginChip(int column = 0) {
    const int line = aiListRecord(AiSteps);
    if (line < 0 || size_t(line) >= doc.lines.size()) return;
    auto chips = aiChips(size_t(line));
    if (chips.empty()) {
      aiManagerCommand(AiEdit, 0);
      return;
    }
    const auto &chip = chips[size_t(std::clamp(column, 0, int(chips.size()) - 1))];
    aiChipLine = line;
    aiChipField = chip.field;
    aiChipListed = false;
    const auto fields = NativeDocument::fields(doc.lines[size_t(line)]);
    if (chip.field == -4) { aiManagerCommand(AiEdit,0);return; }
    ShowWindow(aiControl(AiChipApply),SW_SHOW);
    if (const auto choices = chip.field >= 0 ? aiChoices(fields, chip.field) : std::vector<AiChoice>{}; !choices.empty()) {
      HWND combo = aiControl(AiTarget);
      SendMessageW(combo, CB_RESETCONTENT, 0, 0);
      aiChipOptions.clear();
      int selected = 0;
      for (const auto &[value, label] : choices) {
        if (value == decoded(fields[size_t(chip.field)])) selected = int(aiChipOptions.size());
        aiChipOptions.push_back(value);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
      }
      SendMessageW(combo, CB_SETCURSEL, selected, 0);
      aiChipListed = true;
      ShowWindow(aiControl(AiChipValue), SW_HIDE);
      ShowWindow(combo, SW_SHOW);
      SetFocus(combo);
      return;
    }
    if (chip.field == -3) {
      const auto* kind=aiTargetKind(fields,aiNamed(size_t(line),"editortarget"));
      HWND combo=aiControl(AiTarget);
      SendMessageW(combo, CB_RESETCONTENT,0,0);
      aiChipOptions.clear();
      if (!kind) return;
      const auto records=doc.records(kind,map);
      int at=0, selected=0;
      for (auto record:records) {
        const auto target=NativeDocument::fields(doc.lines[record]);
        const bool raw=fields[3]=="RAW";
        const size_t flag=std::string_view(kind)=="DOOR"?11:std::string_view(kind)=="CREATURE"?8:
                          std::string_view(kind)=="TERMINAL"?9:target.size()>=2?target.size()-2:0;
        if (!raw&&(flag>=target.size()||target[flag].empty())) {++at;continue;}
        const auto label=aiLabel(record,plainKind(kind)+L" "+std::to_wstring(at+1));
        if (label==chip.text) selected=int(aiChipOptions.size());
        aiChipOptions.push_back(raw?std::to_string(at):target[flag]);
        SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
        ++at;
      }
      SendMessageW(combo,CB_SETCURSEL,selected,0);
      ShowWindow(aiControl(AiChipValue),SW_HIDE);
      ShowWindow(combo,SW_SHOW);
      SetFocus(combo);
      return;
    }
    ShowWindow(aiControl(AiTarget),SW_HIDE);
    ShowWindow(aiControl(AiChipValue),SW_SHOW);
    auto value = chip.field < 0 || (fields[3] == "STATE" && chip.field == 5) ?
      chip.text : wide(decoded(fields[size_t(chip.field)]));
    SetWindowTextW(aiControl(AiChipValue), value.c_str());
    SetFocus(aiControl(AiChipValue));
    SendMessageW(aiControl(AiChipValue), EM_SETSEL, 0, -1);
  }
  void aiApplyChip() {
    if (aiChipLine < 0 || size_t(aiChipLine) >= doc.lines.size()) return;
    if (aiChipListed) {
      const auto index = SendMessageW(aiControl(AiTarget), CB_GETCURSEL, 0, 0);
      if (index < 0 || size_t(index) >= aiChipOptions.size()) return;
      auto fields = NativeDocument::fields(doc.lines[size_t(aiChipLine)]);
      const auto &value = aiChipOptions[size_t(index)];
      if (size_t(aiChipField) >= fields.size() || fields[size_t(aiChipField)] == value) return;
      fields[size_t(aiChipField)] = value;
      remember();
      doc.replace(size_t(aiChipLine), fields);
      aiManagerChanged();
      return;
    }
    if (aiChipField==-3) {
      auto combo=aiControl(AiTarget);
      const auto index=SendMessageW(combo,CB_GETCURSEL,0,0);
      if(index<0||size_t(index)>=aiChipOptions.size())return;
      auto fields=NativeDocument::fields(doc.lines[size_t(aiChipLine)]);
      const size_t field=fields[0]=="SCRIPT_REQUIRE"?3:fields[3]=="RAW"?7:4;
      const auto length=SendMessageW(combo,CB_GETLBTEXTLEN,index,0);
      if(length<0)return;
      std::wstring label(size_t(length)+1,L'\0');
      SendMessageW(combo,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(label.data()));label.resize(size_t(length));
      auto named=NativeDocument::named(doc.lines[size_t(aiChipLine)]);
      std::erase_if(named,[](const auto& pair){return pair.first=="editortarget";});
      named.emplace_back("editortarget",utf8(label));
      fields[field]=aiChipOptions[size_t(index)];
      remember();doc.replaceNamed(size_t(aiChipLine),fields,named);aiManagerChanged();return;
    }
    auto text = aiText(aiControl(AiChipValue));
    if (text.empty() || text.find_first_of(L"|%\r\n") != std::wstring::npos)
      throw std::runtime_error("Enter a value without pipes, percent signs or line breaks.");
    const auto line = size_t(aiChipLine);
    auto fields = NativeDocument::fields(doc.lines[line]);
    auto encoded = utf8(text);
    if (aiChipField == -2) {
      auto named = NativeDocument::named(doc.lines[line]);
      std::erase_if(named, [](const auto &pair) { return pair.first == "editorarea"; });
      named.emplace_back("editorarea", encoded);
      remember();
      doc.replaceNamed(line, fields, named);
    } else {
      const auto &action = fields[3];
      if (action == "STATE" && aiChipField == 5) {
        if (encoded == "On" || encoded == "on") encoded = "1";
        if (encoded == "Off" || encoded == "off") encoded = "0";
        size_t tail = 0;
        const int value = std::stoi(encoded, &tail);
        if (tail != encoded.size() || value < -100000 || value > 100000)
          throw std::runtime_error("Use On, Off or a whole number.");
      } else if (action == "STATE") {
        if (!std::all_of(encoded.begin(), encoded.end(), [](unsigned char c) {
              return std::isalnum(c) || c == '_';
            })) throw std::runtime_error("Use letters, numbers and underscores for this switch.");
      } else if (action == "LIGHTS") {
        if (encoded != "normal" && encoded != "flicker" && encoded != "dark" && encoded != "red")
          throw std::runtime_error("Use normal, flicker, dark or red.");
      } else if (action == "NOISE") {
        if (encoded != "behind" && encoded != "ahead") throw std::runtime_error("Use behind or ahead.");
      } else if (action == "LATER" || (action == "MESSAGE" && aiChipField == 5)) {
        size_t tail = 0;
        const float seconds = std::stof(encoded, &tail);
        if (tail != encoded.size() || !(seconds >= (action == "LATER" ? .1f : .5f) &&
            seconds <= (action == "LATER" ? 3600.f : 15.f)))
          throw std::runtime_error("Use a valid number of seconds.");
      } else if (action == "MESSAGE" && encoded.size() > 96) {
        throw std::runtime_error("Use at most 96 characters.");
      }
      if (fields[size_t(aiChipField)] == encoded) return;
      fields[size_t(aiChipField)] = encoded;
      remember();
      doc.replace(line, fields);
    }
    aiManagerChanged();
  }
  static LRESULT CALLBACK aiPaletteProc(HWND h, UINT msg, WPARAM w, LPARAM l,
                                        UINT_PTR, DWORD_PTR ref) {
    auto &e = *reinterpret_cast<Editor *>(ref);
    if (msg == WM_LBUTTONDOWN) {
      const auto row = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
      if (!HIWORD(row)) {
        SetFocus(h);
        SendMessageW(h, LB_SETCURSEL, LOWORD(row), 0);
        const int choice = e.aiListRecord(AiPalette);
        if (choice <= -100) { e.aiToggleCategory(choice); return 0; }
        e.aiHeldBlock = LOWORD(row);
        SetCapture(h);
        return 0;
      }
    }
    if ((msg == WM_MOUSEMOVE || msg == WM_LBUTTONUP) && GetCapture() == h && e.aiHeldBlock >= 0) {
      POINT pos{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
      MapWindowPoints(h, e.aiControl(AiSteps), &pos, 1);
      RECT area{};
      GetClientRect(e.aiControl(AiSteps), &area);
      const auto hit = SendMessageW(e.aiControl(AiSteps), LB_ITEMFROMPOINT, 0, MAKELPARAM(pos.x, pos.y));
      e.aiSnapRow = PtInRect(&area, pos) ? LOWORD(hit) : -1;
      InvalidateRect(e.aiControl(AiSteps), nullptr, FALSE);
      if (msg == WM_LBUTTONUP) {
        ReleaseCapture();
        if (e.aiSnapRow >= 0) e.aiDropBlock();
        else e.aiHeldBlock = -1;
      }
      return 0;
    }
    return DefSubclassProc(h, msg, w, l);
  }
  static LRESULT CALLBACK aiGraphProc(HWND h, UINT msg, WPARAM w, LPARAM l,
                                      UINT_PTR, DWORD_PTR ref) {
    auto &editor = *reinterpret_cast<Editor *>(ref);
    if (msg == WM_LBUTTONUP) {
      RECT area{};
      GetClientRect(h, &area);
      const auto item = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
      RECT row{};
      SendMessageW(h, LB_GETITEMRECT, LOWORD(item), reinterpret_cast<LPARAM>(&row));
      int column = -1;
      if (!HIWORD(item)) {
        const auto line = size_t(SendMessageW(h, LB_GETITEMDATA, LOWORD(item), 0));
        if (line < editor.doc.lines.size()) {
          HDC dc = GetDC(h);
          const auto old = SelectObject(dc, editor.aiFont);
          const auto rects = editor.aiChipRects(dc, row, editor.aiRowText(AiSteps, LOWORD(item)), editor.aiChips(line));
          SelectObject(dc, old);
          ReleaseDC(h, dc);
          const POINT at{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
          for (size_t i = 0; i < rects.size(); ++i)
            if (PtInRect(&rects[i], at)) column = int(i);
        }
      }
      if (column >= 0) {
        DefSubclassProc(h, msg, w, l);
        SendMessageW(h, LB_SETCURSEL, LOWORD(item), 0);
        try { editor.aiBeginChip(column); }
        catch (const std::exception &error) { SetWindowTextW(editor.aiControl(20), wide(error.what()).c_str()); }
        return 0;
      }
    }
    return DefSubclassProc(h, msg, w, l);
  }
  void aiManagerCommand(int id, int notice) {
    if(id==AiLinkedDirector){
      if(!confirmSave())return;
      const CampaignEntry entry{180,2,L"Deep Research Services",L"Engineering / Service Spine",L"Engineering",true};
      std::wstring why;
      if(!openCampaignMap(entry,why))throw std::runtime_error(utf8(why));
      refreshAiManager();
      ShowWindow(aiControl(AiLinkedDirector),SW_HIDE);
      return;
    }
    if (id == AiChipApply) { aiApplyChip(); return; }
    if (id == AiRuleTitle && notice == EN_KILLFOCUS) {
      const int line=aiListRecord(AiRules);
      if(line<0||size_t(line)>=doc.lines.size())return;
      const auto title=aiText(aiControl(AiRuleTitle));
      if(title.empty()||title.find_first_of(L"|%\r\n")!=std::wstring::npos||
         title==aiLabel(size_t(line),aiRowText(AiRules)))return;
      auto fields=NativeDocument::fields(doc.lines[size_t(line)]);
      auto named=NativeDocument::named(doc.lines[size_t(line)]);
      std::erase_if(named,[](const auto& pair){return pair.first=="editorlabel";});
      named.emplace_back("editorlabel",utf8(title));
      remember();doc.replaceNamed(size_t(line),fields,named);aiManagerChanged();return;
    }
    if (id <= AiPalette && notice == LBN_SELCHANGE) aiShowFocus(id);
    if (id == AiEntities && notice == LBN_SELCHANGE) aiShowUsage();
    if (id == AiSearch && notice == EN_CHANGE) { refreshAiManager(); return; }
    if (id == AiTest) {
      const int rule = aiListRecord(AiRules);
      if (rule < 0 || size_t(rule) >= doc.lines.size()) return;
      const auto fields = NativeDocument::fields(doc.lines[size_t(rule)]);
      if (fields.size() < 9) return;
      const float x = (number(fields[3], 0) + number(fields[5], 24)) * .5f;
      const float y = (number(fields[4], 0) + number(fields[6], 24)) * .5f;
      aiTraceEvent = aiScriptId(decoded(fields[2]));
      aiReadTrace(); // ignore anything the previous play session already reported
      aiFlashUntil.clear();
      aiLog.clear();
      const auto [setup, words] = aiTestSetup(fields[2]);
      channelPut(control.parent_path() / L"testrule.txt", setup);
      if (!words.empty()) aiLog = L"Set first, so this rule can run: " + words + L"\r\n";
      aiLog += L"You cannot be hurt while testing.\r\n";
      SetWindowTextW(aiControl(AiPlayLog), aiLog.c_str());
      ShowWindow(aiControl(AiPlayLog), SW_SHOW);
      SetTimer(aiManagerWindow, 1, 60, nullptr);
      SetWindowPos(aiManagerWindow, HWND_TOPMOST, 20, 70, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
      startPlay(3, x, y);
      return;
    }
    if (id == AiDone) { SendMessageW(aiManagerWindow, WM_CLOSE, 0, 0); return; }
    if (id == AiOwners && notice == LBN_SELCHANGE) { refreshAiManager(); return; }
    if (id == AiRules && notice == LBN_SELCHANGE) { refreshAiSteps(); return; }
    if (id == AiUndo || id == AiRedo) {
      aiRenameLine = -1;
      aiChipLine = -1;
      SendMessageW(window, WM_COMMAND, id == AiUndo ? Undo : Redo, 0);
      refreshAiManager();
      return;
    }
    int step = aiListRecord(AiSteps);
    if ((id == AiOwners || id == AiRules) && notice == LBN_DBLCLK) {
      aiRenameLine = aiListRecord(id);
      if (aiRenameLine < 0) return;
      SetWindowTextW(aiControl(AiName), aiLabel(size_t(aiRenameLine), L"").c_str());
      aiShowRename(true);
      SetFocus(aiControl(AiName));
      return;
    }
    if (id == AiRename) {
      auto name = aiText(aiControl(AiName));
      if (aiRenameLine < 0 || size_t(aiRenameLine) >= doc.lines.size() || name.empty()) return;
      if (name.find_first_of(L"|%\r\n") != std::wstring::npos)
        throw std::runtime_error("Use a name without pipes, percent signs or line breaks.");
      auto fields = NativeDocument::fields(doc.lines[size_t(aiRenameLine)]);
      auto named = NativeDocument::named(doc.lines[size_t(aiRenameLine)]);
      std::erase_if(named, [](const auto &pair) { return pair.first == "editorlabel"; });
      named.emplace_back("editorlabel", utf8(name));
      remember();
      doc.replaceNamed(size_t(aiRenameLine), fields, named);
      aiRenameLine = -1;
      aiManagerChanged();
      return;
    }
    if (id == AiEdit || ((id == AiSteps || id == AiEntities) && notice == LBN_DBLCLK)) {
      const int line = id == AiEdit ? step : aiListRecord(id);
      if (line >= 0 && size_t(line) < doc.lines.size()) {
        selectRecord(line);
        if (id == AiEntities) {
          const auto fields = NativeDocument::fields(doc.lines[size_t(line)]);
          const int schema = schemaFor(fields);
          if (schema >= 0 && plan) {
            if (auto point = nativeHandle(schemas[size_t(schema)].fields, fields)) {
              const auto view = planView();
              RECT area{};
              GetClientRect(plan, &area);
              planPanX += area.right * .5f - (view.ox + point->x * view.cell);
              planPanY += area.bottom * .5f - (view.oy + point->y * view.cell);
              InvalidateRect(plan, nullptr, FALSE);
            }
          }
        }
        ShowWindow(aiManagerWindow, SW_HIDE);
        SetForegroundWindow(window);
      }
      return;
    }
    const auto mi = std::to_string(map);
    if (id == AiAddDirector) {
      if (doc.records("AI", map).size() >= 16) throw std::runtime_error("This map already has sixteen Directors.");
      auto name = uniqueName("AI", 2, "director_" + mi + "_");
      remember();
      doc.add({"AI", mi, name, "director", "0", "0", "24", "24", "-100", "100"});
      aiManagerChanged();
      return;
    }
    if (id == AiAddRule) {
      auto directors = doc.records("AI", map);
      const int owner = aiListRecord(AiOwners);
      std::string director;
      if (owner >= 0 && std::find(directors.begin(), directors.end(), size_t(owner)) != directors.end())
        director = NativeDocument::fields(doc.lines[size_t(owner)])[2];
      if (doc.records("SCRIPT_EVENT", map).size() >= 128) throw std::runtime_error("This map has no room for more rules.");
      auto prefix = aiOwnerPrefix();
      auto name = uniqueName("SCRIPT_EVENT", 2, prefix.empty() ? "map_" + mi + "_rule_" : prefix);
      remember();
      doc.add({"SCRIPT_EVENT", mi, name, "0", "0", "24", "24", "-100", "100", "", "1", "0"});
      if (!director.empty())
        doc.add({"SCRIPT_REQUIRE", mi, name, director + "_beat", ">=", "1"});
      doc.add({"SCRIPT_ACTION", mi, name, "MESSAGE", "SOMETHING STIRS.", "2"});
      aiManagerChanged();
      const auto rules = doc.records("SCRIPT_EVENT", map);
      aiRestoreRow(AiRules, int(rules.back()));
      refreshAiSteps();
      return;
    }
    const auto rule = aiRuleName();
    if (rule.empty()) return;
    if (id == AiRemoveStep && step >= 0 && size_t(step) < doc.lines.size()) {
      const auto fields = NativeDocument::fields(doc.lines[size_t(step)]);
      if (fields.size() < 3 || fields[2] != rule ||
          (fields[0] != "SCRIPT_REQUIRE" && fields[0] != "SCRIPT_ACTION")) return;
      if (fields[0] == "SCRIPT_ACTION") {
        int count = 0;
        for (auto line : doc.records("SCRIPT_ACTION", map))
          if (NativeDocument::fields(doc.lines[line])[2] == rule) ++count;
        if (count <= 1) throw std::runtime_error("Keep at least one action in the rule.");
      }
      remember();
      doc.eraseRecord(size_t(step));
      record = -1;
      selectedRecords.clear();
      aiManagerChanged();
      return;
    }
    if (id != AiAddStep) return;
    const auto choice = aiListRecord(AiPalette);
    if (choice == 15) {
      for (auto line : doc.records("SCRIPT_TRIGGER", map)) {
        const auto fields = NativeDocument::fields(doc.lines[line]);
        if (fields.size() < 6 || fields[2] != rule) continue;
        auto named = NativeDocument::named(doc.lines[line]);
        std::erase_if(named, [](const auto &pair) { return pair.first == "players"; });
        named.emplace_back("players", "all");
        remember();
        doc.replaceNamed(line, fields, named);
        aiManagerChanged();
        return;
      }
      remember();
      doc.ensureFormat2();
      doc.add({"SCRIPT_TRIGGER", mi, rule, "box", "all", "0", "players=all"});
      aiManagerChanged();
      return;
    }
    std::vector<std::string> block{"SCRIPT_ACTION", mi, rule};
    std::vector<std::vector<std::string>> extra;
    int changedEntity = -1;
    std::vector<std::string> entityFields;
    auto action = [&](std::initializer_list<std::string> args) {
      auto row = block;
      row.insert(row.end(), args);
      return row;
    };
    switch (choice) {
    case 0: {
      int count = 0;
      for (auto line : doc.records("SCRIPT_REQUIRE", map))
        if (NativeDocument::fields(doc.lines[line])[2] == rule) ++count;
      if (count >= 8) throw std::runtime_error("A rule can have eight conditions.");
      block = {"SCRIPT_REQUIRE", mi, rule, "switch_1", "=", "1"};
      break;
    }
    case 1: block.insert(block.end(), {"STATE", "switch_1", "1"}); break;
    case 2: block.insert(block.end(), {"LATER", "switch_1", "1", "2"}); break;
    case 3: {
      auto directors = doc.records("AI", map);
      const int owner = aiListRecord(AiOwners);
      if (owner < 0 || std::find(directors.begin(), directors.end(), size_t(owner)) == directors.end())
        throw std::runtime_error("Pick the Director whose lights should change.");
      block.insert(block.end(), {"LIGHTS", NativeDocument::fields(doc.lines[size_t(owner)])[2], "flicker", "2"});
      break;
    }
    case 4: block.insert(block.end(), {"NOISE", "metal_drag", "1", "behind", "10"}); break;
    case 5: block.insert(block.end(), {"MESSAGE", "SOMETHING STIRS.", "2"}); break;
    case 6: case 7: case 13: {
      auto doors = doc.records("DOOR", map);
      const int picked = aiListRecord(AiEntities);
      const auto it = picked < 0 ? doors.end() : std::find(doors.begin(), doors.end(), size_t(picked));
      if (it == doors.end()) throw std::runtime_error("Pick a door from Placed things first.");
      const auto index = std::to_string(it - doors.begin());
      const bool open = choice == 7;
      block = action({"RAW", open ? "4" : "5", "", "0", index, "0", "0", "0", "-999", "0", "0", ""});
      auto door = NativeDocument::fields(doc.lines[size_t(picked)]);
      if (choice == 13) {
        if (!editableRecord(size_t(picked))) throw std::runtime_error("Unlock the door's floor first.");
        door.resize(std::max<size_t>(door.size(), 13));
        if (!door[11].empty()) throw std::runtime_error("This door already has a switch lock.");
        door[11] = "ai_seal_" + mi + "_" + index;
        door[12] = "0";
        extra.push_back(action({"STATE", door[11], "1"}));
        changedEntity = picked;
        entityFields = std::move(door);
      } else if (open && door.size() > 11 && !door[11].empty()) {
        extra.push_back(action({"STATE", door[11], door.size() > 12 && !door[12].empty() ? door[12] : "1"}));
      }
      break;
    }
    case 8:
      block.insert(block.end(), {"RAW", "8", "", "0", "0", "8", "12", "12", "-999", "1", "0", ""});
      break;
    case 9: case 10: {
      const int picked = aiListRecord(AiEntities);
      if (picked < 0 || size_t(picked) >= doc.lines.size() || !editableRecord(size_t(picked)))
        throw std::runtime_error("Pick a creature on an unlocked floor first.");
      entityFields = NativeDocument::fields(doc.lines[size_t(picked)]);
      if (entityFields.size() < 6 || entityFields[0] != "CREATURE" ||
          (choice == 9 && entityFields[2] != "8"))
        throw std::runtime_error(choice == 9 ? "Pick a placed machine first." : "Pick a placed creature first.");
      entityFields.resize(std::max<size_t>(entityFields.size(), choice == 9 ? 9 : 10));
      if (!entityFields[8].empty() || (choice == 10 && !entityFields[9].empty()))
        throw std::runtime_error("This creature already has a wake or chase assignment.");
      const auto wake = "ai_wake_" + mi + "_" + std::to_string(picked) + "_" +
                        std::to_string(doc.records("SCRIPT_ACTION", map).size());
      entityFields[8] = wake;
      if (choice == 10) entityFields[9] = wake + "_hunter";
      changedEntity = picked;
      if (choice == 9) {
        int delayed = 0;
        for (auto line : doc.records("SCRIPT_ACTION", map)) {
          auto previous = NativeDocument::fields(doc.lines[line]);
          if (previous.size() >= 7 && previous[2] == rule && previous[3] == "LATER" &&
              previous[4].starts_with("ai_wake_")) ++delayed;
        }
        block = action({"LATER", wake, "1", std::to_string(2 + 2 * delayed)});
      } else {
        extra.push_back(action({"NOISE", "metal_drag", "1", "behind", "0"}));
        block = action({"STATE", wake, "1"});
      }
      break;
    }
    case 11: {
      int conditions = 0;
      for (auto line : doc.records("SCRIPT_REQUIRE", map))
        if (NativeDocument::fields(doc.lines[line])[2] == rule) ++conditions;
      if (conditions > 5) throw std::runtime_error("This rule has room for fewer than three conditions.");
      for (int i = 1; i <= 2; ++i)
        extra.push_back({"SCRIPT_REQUIRE", mi, rule, "breaker_" + std::to_string(i), "=", "1"});
      block = {"SCRIPT_REQUIRE", mi, rule, "breaker_3", "=", "1"};
      break;
    }
    case 12: {
      const auto phase = rule + "_fight";
      const auto quiet = rule + "_quiet";
      extra.push_back(action({"STATE", phase, "1"}));
      extra.push_back(action({"LATER", phase, "0", "8"}));
      extra.push_back(action({"LATER", quiet, "1", "8"}));
      extra.push_back(action({"LATER", quiet, "0", "16"}));
      block = action({"LATER", phase, "1", "16"});
      break;
    }
    case 14:
      block = action({"NOISE", "pa_evacuate", "0.7", "behind", "0"});
      break;
    default: return;
    }
    remember();
    if (changedEntity >= 0) doc.replace(size_t(changedEntity), entityFields);
    for (const auto &row : extra) doc.add(row);
    const int snapLine = aiSnapRow >= 0 ?
      int(SendMessageW(aiControl(AiSteps), LB_GETITEMDATA, aiSnapRow, 0)) : -1;
    doc.add(block);
    if (snapLine >= 0 && size_t(snapLine) < doc.lines.size() && extra.empty()) {
      const auto target = NativeDocument::fields(doc.lines[size_t(snapLine)]);
      if (target.size() > 2 && target[0] == block[0] && target[2] == rule) {
        auto records = doc.records(block[0], map);
        const size_t last = records.back();
        const size_t before = size_t(snapLine) - (doc.recordLayer(size_t(snapLine)) ? 1 : 0);
        if (before < last) {
          std::rotate(doc.lines.begin() + before, doc.lines.begin() + last, doc.lines.begin() + last + 1);
          doc.touch();
        }
      }
    }
    aiManagerChanged();
    aiRestoreRow(AiSteps, int(doc.records(block[0], map).back()));
  }

  static LRESULT CALLBACK aiManagerProc(HWND h, UINT message, WPARAM w, LPARAM l) {
    auto *editor = reinterpret_cast<Editor *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      editor = static_cast<Editor *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
      SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(editor));
    }
    if (!editor) return DefWindowProcW(h, message, w, l);
    if (message == WM_CLOSE) { KillTimer(h, 1); ShowWindow(h, SW_HIDE); return 0; }
    if (message == WM_TIMER && w == 1) {
      editor->aiReadTrace();
      if (!editor->playing && editor->aiFlashUntil.empty()) {
        SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        KillTimer(h, 1);
      }
      return 0;
    }
    if (message == WM_MEASUREITEM && w == AiSteps) {
      reinterpret_cast<MEASUREITEMSTRUCT *>(l)->itemHeight = 30;
      return TRUE;
    }
    if (message == WM_DRAWITEM && w != AiSteps) {
      auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(l);
      if (item.CtlType != ODT_BUTTON) return FALSE;
      const bool pressed = (item.itemState & ODS_SELECTED) != 0;
      auto rect = item.rcItem;
      SetDCBrushColor(item.hDC, pressed ? RGB(49,47,45) : RGB(68,66,62));
      FillRect(item.hDC, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
      DrawEdge(item.hDC, &rect, pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);
      if (item.itemState & ODS_FOCUS) {
        InflateRect(&rect, -3, -3);
        SetDCBrushColor(item.hDC, RGB(255, 218, 106));
        FrameRect(item.hDC, &rect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
      }
      if (pressed) OffsetRect(&rect, 1, 1);
      SelectObject(item.hDC, editor->aiFont);
      SetBkMode(item.hDC, TRANSPARENT);
      SetTextColor(item.hDC, RGB(244,232,209));
      auto label = aiText(item.hwndItem);
      DrawTextW(item.hDC, label.c_str(), int(label.size()), &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
      return TRUE;
    }
    if (message == WM_DRAWITEM && w == AiSteps) {
      auto &item = *reinterpret_cast<DRAWITEMSTRUCT *>(l);
      FillRect(item.hDC, &item.rcItem, editor->aiDarkBrush);
      if (item.itemID == UINT(-1)) return TRUE;
      const auto line = size_t(item.itemData);
      if (line >= editor->doc.lines.size()) return TRUE;
      const auto fields = NativeDocument::fields(editor->doc.lines[line]);
      COLORREF color = RGB(86, 167, 107);
      if (fields[0] == "SCRIPT_EVENT" || fields[0] == "SCRIPT_TRIGGER") color = RGB(218, 180, 78);
      if (fields[0] == "SCRIPT_REQUIRE") color = RGB(110, 163, 212);
      if (fields.size() > 3 && fields[0] == "SCRIPT_ACTION" && fields[3] == "LATER") color = RGB(164, 169, 174);
      if (editor->aiFlashUntil.contains(int(item.itemID))) color = RGB(255, 237, 150);
      auto brush = CreateSolidBrush(color);
      RECT body = item.rcItem;
      InflateRect(&body, -7, -3);
      FillRect(item.hDC, &body, brush);
      RECT tab{body.left + 16, body.bottom, body.left + 39, body.bottom + 3};
      FillRect(item.hDC, &tab, brush);
      DrawEdge(item.hDC, &body, EDGE_RAISED, BF_RECT);
      DeleteObject(brush);
      if (int(item.itemID) == editor->aiSnapRow || (item.itemState & ODS_SELECTED)) {
        SetDCBrushColor(item.hDC, RGB(255, 222, 118));
        FrameRect(item.hDC, &body, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        if (int(item.itemID) == editor->aiSnapRow) {
          RECT slot{body.left, body.top - 3, body.right, body.top + 3};
          FillRect(item.hDC, &slot, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        }
      }
      const auto length = SendMessageW(item.hwndItem, LB_GETTEXTLEN, item.itemID, 0);
      if (length < 0) return TRUE;
      std::wstring label(size_t(length) + 1, L'\0');
      SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID, reinterpret_cast<LPARAM>(label.data()));
      InflateRect(&body, -9, 0);
      SelectObject(item.hDC, editor->aiFont ? editor->aiFont : editor->font);
      SetBkMode(item.hDC, TRANSPARENT);
      SetTextColor(item.hDC, RGB(21, 22, 24));
      const auto chips = editor->aiChips(line);
      label.resize(size_t(length));
      RECT sentence = body;
      DrawTextW(item.hDC, label.c_str(), int(length), &sentence, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
      const auto rects = editor->aiChipRects(item.hDC, item.rcItem, label, chips);
      for (size_t index = 0; index < chips.size(); ++index) {
        RECT chip = rects[index];
        DrawEdge(item.hDC, &chip, EDGE_SUNKEN, BF_RECT);
        DrawTextW(item.hDC, chips[index].text.c_str(), int(chips[index].text.size()),
                  &chip, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
      }
      return TRUE;
    }
    if (message == WM_COMMAND) {
      try { editor->aiManagerCommand(LOWORD(w), HIWORD(w)); }
      catch (const std::exception &error) {
        SetWindowTextW(GetDlgItem(h, 20), wide(error.what()).c_str());
      }
      return 0;
    }
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORLISTBOX || message == WM_CTLCOLOREDIT) {
      auto dc = reinterpret_cast<HDC>(w);
      SetTextColor(dc, RGB(239, 226, 200));
      SetBkColor(dc, RGB(38, 36, 33));
      return reinterpret_cast<LRESULT>(editor->aiDarkBrush);
    }
    return DefWindowProcW(h, message, w, l);
  }
  HBRUSH aiDarkBrush = CreateSolidBrush(RGB(38, 36, 33));
  void openAiManager() {
    if (!aiManagerWindow) {
      static const wchar_t *className = L"RawMetalAiEntityManager";
      WNDCLASSW wc{};
      wc.hInstance = GetModuleHandleW(nullptr);
      wc.lpfnWndProc = aiManagerProc;
      wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      wc.hbrBackground = aiDarkBrush;
      wc.lpszClassName = className;
      RegisterClassW(&wc);
      aiManagerWindow = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT, className,
        L"AI & Entity Manager", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1166, 680, window, nullptr, wc.hInstance, this);
      if (!aiManagerWindow) throw std::runtime_error("Could not open the AI & Entity Manager.");
      aiFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                          FIXED_PITCH | FF_MODERN, L"Fixedsys");
      auto child = [&](int id, const wchar_t *cls, const wchar_t *label, DWORD style,
                       int x, int y, int width, int height) {
        if (std::wstring_view(cls) == L"BUTTON") style |= BS_OWNERDRAW;
        HWND control = CreateWindowExW(0, cls, label, WS_CHILD | WS_VISIBLE | style,
          x, y, width, height, aiManagerWindow, reinterpret_cast<HMENU>(INT_PTR(id)), wc.hInstance, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(aiFont ? aiFont : font), FALSE);
      };
      const wchar_t *headers[] = {L"AIs", L"When", L"Rule", L"Placed things", L"Add a block"};
      const int lists[] = {AiOwners, AiRules, AiSteps, AiEntities, AiPalette};
      const int x[] = {14, 14, 238, 870, 14};
      const int widths[] = {212, 212, 620, 264, 212};
      const int tops[] = {12, 130, 12, 12, 336};
      const int drop[] = {23, 23, 51, 51, 23};
      const int heights[] = {82, 134, 432, 398, 216};
      for (int i = 0; i < 5; ++i) {
        const int y = tops[i];
        child(0, L"STATIC", headers[i], 0, x[i], y, widths[i], 20);
        child(lists[i], L"LISTBOX", L"", WS_BORDER | WS_TABSTOP | WS_VSCROLL | (i == 2 ? 0 : WS_HSCROLL) | LBS_NOTIFY |
              (i == 2 ? LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT : 0),
              x[i], y + drop[i], widths[i], heights[i]);
      }
      child(AiRuleTitle, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 238, 34, 620, 24);
      SendMessageW(aiControl(AiRuleTitle),EM_LIMITTEXT,80,0);
      child(AiSearch, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 870, 34, 264, 25);
      SetWindowSubclass(aiControl(AiSearch), aiSearchProc, 1, reinterpret_cast<DWORD_PTR>(this));
      child(AiUsedBy, L"STATIC", L"Used by: None", SS_NOPREFIX, 870, 456, 264, 34);
      child(AiLinkedDirector,L"BUTTON",L"Open Service Spine Director",WS_TABSTOP,14,301,212,28);
      child(AiPlayLog, L"EDIT", L"", WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 870, 494, 264, 138);
      child(AiChipValue, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 238, 538, 462, 27);
      child(AiTarget, L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 238, 538, 462, 260);
      SendMessageW(aiControl(AiChipValue), EM_SETCUEBANNER, TRUE, LPARAM(L"Select a setting to edit"));
      child(AiChipApply, L"BUTTON", L"Apply setting", WS_TABSTOP, 710, 536, 148, 30);
      for (int id : {AiChipValue,AiTarget,AiChipApply}) ShowWindow(aiControl(id),SW_HIDE);
      child(AiName, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 238, 538, 500, 27);
      SendMessageW(aiControl(AiName), EM_LIMITTEXT, 80, 0);
      SendMessageW(aiControl(AiName), EM_SETCUEBANNER, TRUE, LPARAM(L"Friendly name"));
      child(AiRename, L"BUTTON", L"Rename", WS_TABSTOP, 748, 536, 110, 30);
      child(AiAddDirector, L"BUTTON", L"Add Director", WS_TABSTOP, 238, 500, 100, 30);
      child(AiAddRule, L"BUTTON", L"Add rule", WS_TABSTOP, 342, 500, 90, 30);
      child(AiAddStep, L"BUTTON", L"Snap block in", WS_TABSTOP, 436, 500, 118, 30);
      child(AiEdit, L"BUTTON", L"Edit block", WS_TABSTOP, 558, 500, 100, 30);
      child(AiRemoveStep, L"BUTTON", L"Remove block", WS_TABSTOP, 662, 500, 112, 30);
      child(AiTest, L"BUTTON", L"Test it", WS_TABSTOP, 778, 500, 80, 30);
      child(AiUndo, L"BUTTON", L"Undo", WS_TABSTOP, 238, 574, 90, 30);
      child(AiRedo, L"BUTTON", L"Redo", WS_TABSTOP, 334, 574, 90, 30);
      child(AiDone, L"BUTTON", L"Done", WS_TABSTOP, 430, 574, 90, 30);
      child(AiFocusText, L"STATIC", L"", SS_NOPREFIX, 530, 578, 328, 28);
      child(20, L"STATIC", L"◀ ▶ Panels   ▲ ▼ Rows   Ⓐ Pick / snap   Ⓑ Back",
            0, 14, 614, 844, 22);
      aiShowRename(false);
      ShowWindow(aiControl(AiPlayLog), SW_HIDE);
      aiCategoryOpen = {false, true, false, false, false, false};
      aiRefreshPalette(-100);
      SetWindowSubclass(aiControl(AiPalette), aiPaletteProc, 1, reinterpret_cast<DWORD_PTR>(this));
      SetWindowSubclass(aiControl(AiSteps), aiGraphProc, 1, reinterpret_cast<DWORD_PTR>(this));
      for (int id : lists)
        SetWindowSubclass(aiControl(id), aiHoverProc, 2, reinterpret_cast<DWORD_PTR>(this));
    }
    refreshAiManager();
    ShowWindow(aiControl(AiLinkedDirector),campaignLevel()==181?SW_SHOW:SW_HIDE);
    aiManagerFocus = 0;
    aiPadTyping = 0;
    RECT owner{};
    GetWindowRect(window, &owner);
    SetWindowPos(aiManagerWindow, HWND_TOP, owner.left + 50, owner.top + 85, 0, 0,
                 SWP_NOSIZE | SWP_SHOWWINDOW);
    SetFocus(aiControl(AiOwners));
  }
  void verifyAiManager(const fs::path &folder) {
    doc.newCampaign("AI Manager");
    doc.addRoom("Engineering");
    map = 0;
    const auto empty = doc.text();
    history.clear();
    future.clear();
    SendMessageW(window, WM_COMMAND, AiManager, 0);
    if (!aiManagerWindow) throw std::runtime_error("The Builder command did not open the manager");
    aiManagerCommand(AiAddDirector, 0);
    const auto directorLine = doc.records("AI", 0).front();
    doc.replaceNamed(directorLine, NativeDocument::fields(doc.lines[directorLine]), {{"calm", "2"}, {"peak", "5"}});
    refreshAiManager();
    SendMessageW(aiControl(AiOwners), LB_SETCURSEL, 0, 0);
    aiManagerCommand(AiAddRule, 0);
    aiCategoryOpen.fill(true);
    aiRefreshPalette(-100);
    aiToggleCategory(-103);
    if (aiCategoryOpen[3] || aiListRecord(AiPalette) != -103)
      throw std::runtime_error("The Creatures category did not collapse in place");
    aiToggleCategory(-103);
    aiRestoreRow(AiPalette, 3);
    aiManagerCommand(AiAddStep, 0);
    aiRestoreRow(AiPalette, 1);
    aiHeldBlock = int(SendMessageW(aiControl(AiPalette), LB_GETCURSEL, 0, 0));
    aiSnapRow = 2;
    aiDropBlock();
    aiRestoreRow(AiPalette, 2);
    aiManagerCommand(AiAddStep, 0);
    aiBeginChip();
    SetWindowTextW(aiControl(AiChipValue), L"1.0");
    aiApplyChip();
    aiManagerCommand(AiOwners, LBN_DBLCLK);
    SetWindowTextW(aiControl(AiName), L"Engineering Director");
    aiManagerCommand(AiRename, BN_CLICKED);
    aiManagerCommand(AiRules, LBN_DBLCLK);
    SetWindowTextW(aiControl(AiName), L"Lockdown rule");
    aiManagerCommand(AiRename, BN_CLICKED);
    const auto built = doc.text();
    if (doc.records("AI", 0).size() != 1 || doc.records("SCRIPT_REQUIRE", 0).size() != 1 ||
        doc.records("SCRIPT_ACTION", 0).size() != 4)
      throw std::runtime_error("The Director rule did not join its condition and actions");
    auto file = folder / L"director-graph.rmc";
    doc.save(file);
    NativeDocument reopened;
    reopened.open(file);
    if (reopened.text() != built) throw std::runtime_error("The graph changed after reopening");
    if (aiText(aiControl(AiRuleTitle)) != L"Lockdown rule")
      throw std::runtime_error("The graph title did not follow the selected rule");
    aiShowFocus(AiRules);
    SendMessageW(aiControl(AiSteps), LB_SETTOPINDEX, 0, 0);
    SendMessageW(aiControl(AiPalette), LB_SETTOPINDEX, 0, 0);
    captureEditorWindow(folder / L"director-graph.bmp", aiManagerWindow);
    aiTraceEvent = aiScriptId(decoded(aiRuleName()));
    aiTraceSerial = 0;
    aiConsumeTrace("READY\nAI_TRACE 1 " + std::to_string(aiTraceEvent) + " -1\nAI_TRACE 2 " +
                   std::to_string(aiTraceEvent) + " 0\n");
    if (aiFlashUntil.size() < 2 || aiText(aiControl(AiPlayLog)).find(L"Set")==std::wstring::npos)
      throw std::runtime_error("The playtest status did not highlight the rule and action");
    captureEditorWindow(folder / L"trace-display.bmp", aiManagerWindow);
    aiFlashUntil.clear();
    for (int i = 0; i < 8; ++i) aiManagerCommand(AiUndo, 0);
    if (doc.text() != empty) throw std::runtime_error("Undo did not restore the original map");
    for (int i = 0; i < 8; ++i) aiManagerCommand(AiRedo, 0);
    if (doc.text() != built) throw std::runtime_error("Redo did not restore the graph");
    verifyAiPickLists(folder);
  }
  // The lamp and sound settings are picked from lists: every choice a list offers is one the engine loads.
  void verifyAiPickLists(const fs::path &folder) {
    const auto before = doc.text();
    const std::string event = NativeDocument::fields(doc.lines[doc.records("SCRIPT_EVENT", 0).front()])[2];
    const std::string director = NativeDocument::fields(doc.lines[doc.records("AI", 0).front()])[2];
    doc.add({"SCRIPT_ACTION", "0", event, "LIGHTS", director, "flicker", "2"});
    doc.add({"SCRIPT_ACTION", "0", event, "NOISE", "metal_drag", "1", "behind", "10"});
    aiManagerChanged();
    struct Pick { const char *kind; int field; const wchar_t *choose; const char *wrote; const wchar_t *shot; };
    const Pick picks[] = {{"LIGHTS", 5, L"Emergency red", "red", L"pick-lamp.bmp"},
                          {"NOISE", 4, nullptr, nullptr, L"pick-sound.bmp"},
                          {"NOISE", 6, L"Ahead of the player", "ahead", L"pick-where.bmp"}};
    for (const auto &pick : picks) {
      size_t line = 0;
      for (auto record : doc.records("SCRIPT_ACTION", 0))
        if (NativeDocument::fields(doc.lines[record])[3] == pick.kind) line = record;
      auto selectRow = [&] {
        const int rows = int(SendMessageW(aiControl(AiSteps), LB_GETCOUNT, 0, 0));
        for (int row = 0; row < rows; ++row) {
          SendMessageW(aiControl(AiSteps), LB_SETCURSEL, row, 0);
          if (aiListRecord(AiSteps) == int(line)) return;
        }
        throw std::runtime_error(std::string("No graph row for the block ") + pick.kind);
      };
      selectRow();
      const auto chips = aiChips(line);
      int column = -1;
      for (int i = 0; i < int(chips.size()); ++i) if (chips[size_t(i)].field == pick.field) column = i;
      if (column < 0) throw std::runtime_error(std::string("No setting to pick on ") + pick.kind);
      aiBeginChip(column);
      HWND combo = aiControl(AiTarget);
      const int count = int(SendMessageW(combo, CB_GETCOUNT, 0, 0));
      if (!aiChipListed || !IsWindowVisible(combo) || IsWindowVisible(aiControl(AiChipValue)) || count < 2)
        throw std::runtime_error(std::string("No pick list for ") + pick.kind);
      // Each choice the list offers must give a map the engine accepts.
      for (const auto &value : std::vector<std::string>(aiChipOptions)) {
        const auto kept = doc.text();
        auto fields = NativeDocument::fields(doc.lines[line]);
        fields[size_t(pick.field)] = value;
        doc.replace(line, fields);
        const bool loads = validateDocument();
        doc.loadText(kept);
        if (!loads) throw std::runtime_error("The engine refused the choice " + value);
      }
      refreshAiManager();
      selectRow();
      aiBeginChip(column);
      int choose = pick.choose ? int(SendMessageW(combo, CB_FINDSTRINGEXACT, WPARAM(-1), reinterpret_cast<LPARAM>(pick.choose)))
                               : (int(SendMessageW(combo, CB_GETCURSEL, 0, 0)) + 1) % count;
      if (choose < 0) throw std::runtime_error(std::string("A choice is missing for ") + pick.kind);
      SendMessageW(combo, CB_SETCURSEL, choose, 0);
      const std::string expected = pick.wrote ? std::string(pick.wrote) : aiChipOptions[size_t(choose)];
      captureEditorWindow(folder / pick.shot, aiManagerWindow);
      // The list itself, dropped open, for the same choice.
      SendMessageW(combo, CB_SHOWDROPDOWN, TRUE, 0);
      if (COMBOBOXINFO info{sizeof(info)}; GetComboBoxInfo(combo, &info) && info.hwndList)
        captureEditorWindow(folder / (std::wstring(L"list-") + pick.shot), info.hwndList);
      SendMessageW(combo, CB_SHOWDROPDOWN, FALSE, 0);
      SendMessageW(combo, CB_SETCURSEL, choose, 0);
      const auto unchanged = doc.text();
      aiApplyChip();
      if (doc.text() == unchanged || NativeDocument::fields(doc.lines[line])[size_t(pick.field)] != expected)
        throw std::runtime_error("Picking did not write " + expected);
      std::ofstream(folder / L"pick-lists.txt", std::ios::app)
          << pick.kind << " field " << pick.field << ": " << count << " choices, every one loads; picked " << expected << '\n';
      aiManagerCommand(AiUndo, 0);
      if (doc.text() != unchanged) throw std::runtime_error("Undo did not restore the setting");
    }
    doc.loadText(before);
    refreshAiManager();
  }
  // --ai-live-test: Test it pressed for real on every Engineering rule group, with the engine playing behind the
  // window. Each rule's own trace lines say whether it ran; a picture of the manager is kept after each.
  void verifyAiLive(const fs::path &folder) {
    const CampaignEntry entry{180, 2, L"Deep Research Services", L"Engineering / Service Spine", L"Engineering", true};
    std::wstring why;
    if (!openCampaignMap(entry, why)) throw std::runtime_error(utf8(why));
    startEngine();
    auto pump = [&](int ms, auto done) {
      const auto until = GetTickCount64() + ULONGLONG(ms);
      while (GetTickCount64() < until) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        if (done()) return true;
        Sleep(15);
      }
      return false;
    };
    auto engineReady = [&] { return channelText(status).starts_with("READY " + std::to_string(revision) + " "); };
    if (!pump(60000, engineReady)) throw std::runtime_error("The engine never showed the map");
    openAiManager();
    aiRestoreRow(AiOwners, int(doc.records("AI", map).front()));
    refreshAiManager();
    std::ofstream out(folder / L"live-rules.txt");
    const int count = int(SendMessageW(aiControl(AiRules), LB_GETCOUNT, 0, 0));
    int ran = 0;
    for (int row = 0; row < count; ++row) {
      SendMessageW(aiControl(AiRules), LB_SETCURSEL, row, 0);
      refreshAiSteps();
      const auto title = utf8(aiText(aiControl(AiRuleTitle)));
      aiManagerCommand(AiTest, BN_CLICKED);
      const auto before = aiLog;   // what Test it set up, if anything
      const bool fired = pump(20000, [&] { aiReadTrace(); return aiLog != before; });
      const bool delayed = title == "Cages wake";
      pump(delayed ? 46000 : 1500, [&] { aiReadTrace(); return false; });
      if (delayed)
        for (const wchar_t *step : {L"Cage row 1", L"Cage row 2", L"Cage row 3"})
          if (aiLog.find(step) == std::wstring::npos)
            throw std::runtime_error("A delayed step never showed in the play log: " + utf8(step) + "\nLOG: " + utf8(aiLog) +
                                     "\nSTATUS: " + channelText(status).substr(0, 600));
      out << (fired ? "RAN     " : "SILENT  ") << title << '\n';
      if (fired) { ++ran; std::istringstream lines(utf8(aiLog)); for (std::string l; std::getline(lines, l);) { while (!l.empty() && l.back() == '\r') l.pop_back(); out << "          " << l << '\n'; } }
      out.flush();
      captureEditorWindow(folder / (L"live-" + std::to_wstring(row + 1) + L".bmp"), aiManagerWindow);
      if (playing) stopPlay();
      KillTimer(aiManagerWindow, 1);
      pump(800, [] { return false; });
    }
    out << ran << " of " << count << " rule groups ran when tested\n";
    if (ran != count) throw std::runtime_error(std::to_string(count - ran) + " rule groups never ran under Test it");
  }
  void verifyEngineeringManager(const fs::path& folder) {
    const CampaignEntry entry{180,2,L"Deep Research Services",L"Engineering / Service Spine",L"Engineering",true};
    std::wstring why;
    if(!openCampaignMap(entry,why))throw std::runtime_error(utf8(why));
    openAiManager();
    const auto owners=doc.records("AI",map);
    if(owners.size()!=1||aiLabel(owners.front(),L"")!=L"Engineering Director")
      throw std::runtime_error("The exported Service Spine has no named Director");
    aiRestoreRow(AiOwners,int(owners.front()));refreshAiManager();
    const int count=int(SendMessageW(aiControl(AiRules),LB_GETCOUNT,0,0));
    if(count!=5)throw std::runtime_error("Expected five Engineering rule groups, saw "+std::to_string(count));
    const wchar_t* names[]={L"Lockdown",L"Cages wake",L"Stalker",L"Breakers",L"Pacing"};
    const wchar_t* shots[]={L"01-lockdown.bmp",L"02-cages.bmp",L"03-stalker.bmp",L"04-breakers.bmp",L"05-pacing.bmp"};
    for(int row=0;row<count;++row){
      SendMessageW(aiControl(AiRules),LB_SETCURSEL,row,0);refreshAiSteps();
      SendMessageW(aiControl(AiSteps),LB_SETTOPINDEX,0,0);
      if(aiText(aiControl(AiRuleTitle))!=names[row])throw std::runtime_error("The campaign rule title was not preserved");
      captureEditorWindow(folder/shots[row],aiManagerWindow);
    }
    SendMessageW(aiControl(AiRules),LB_SETCURSEL,0,0);refreshAiSteps();
    for(int row=0,rows=int(SendMessageW(aiControl(AiSteps),LB_GETCOUNT,0,0));row<rows;++row){
      SendMessageW(aiControl(AiSteps),LB_SETCURSEL,row,0);
      const int line=aiListRecord(AiSteps);
      if(line<0)continue;
      const auto f=NativeDocument::fields(doc.lines[size_t(line)]);
      if(f.size()<8||f[0]!="SCRIPT_ACTION"||f[3]!="RAW"||f[4]!="5")continue;
      const auto original=doc.text();
      aiBeginChip();
      auto combo=aiControl(AiTarget);
      const int next=int(SendMessageW(combo,CB_FINDSTRINGEXACT,-1,reinterpret_cast<LPARAM>(L"Cage row 2")));
      if(next<0)throw std::runtime_error("The typed door picker does not contain Cage row 2");
      SendMessageW(combo,CB_SETCURSEL,next,0);
      aiApplyChip();
      if(doc.text()==original)throw std::runtime_error("The placed door picker did not change the rule");
      aiManagerCommand(AiUndo,0);
      if(doc.text()!=original)throw std::runtime_error("Undo did not restore the authored target");
      break;
    }
    doc.save(folder/L"service-spine-180.rmc");
  }
