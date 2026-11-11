  // ---- The Sound tool: places AMBIENT emitters from the engine's sound bank. ----
  // The bank is read from src/audio/SoundBank.h (loadSchemas), so the list on the
  // right is always the engine's. A bed is placed as a loop, any other sound as
  // "now and then"; the inspector's fields change its loudness, reach, timing and
  // the switch it waits on. REVERB (one per map, under Advanced) sets the room's
  // answer. This is the headless check of all of it, through the engine.
  bool soundToolTest(std::string &report) {
    auto fail = [&](const std::string &why) { report += "FAIL: " + why + "\n"; return false; };
    if (bankSounds.size() < 40)
      return fail("the sound bank read " + std::to_string(bankSounds.size()) + " rows from src/audio/SoundBank.h");
    auto index = [&](const char *name) {
      for (size_t i = 0; i < bankSounds.size(); ++i)
        if (bankSounds[i].name == name) return int(i);
      return -1;
    };
    if (index("water_drip") < 0 || index("metal_creak") < 0 || !bankSounds[size_t(index("water_drip"))].loop || bankSounds[size_t(index("metal_creak"))].loop)
      return fail("the bank's rows did not read as a bed and a single sound");
    doc.open(root / L"custom maps" / L"Starter Map.rmc");
    path.clear(), map = 0, layer = 0, record = -1;
    tool = SoundTool;
    chooseKind("AMBIENT");
    remember();
    soundPick = index("water_drip");
    int bed = authorKind("AMBIENT", 6.5f, 12.5f, 6.5f, 12.5f);
    soundPick = index("metal_creak");
    int single = authorKind("AMBIENT", 9.5f, 12.5f, 9.5f, 12.5f);
    soundPick = -1;
    if (bed < 0 || single < 0)
      return fail("the tool made no record");
    auto a = NativeDocument::fields(doc.lines[size_t(bed)]), b = NativeDocument::fields(doc.lines[size_t(single)]);
    if (a.size() < 9 || a[0] != "AMBIENT" || a[2] != "water_drip" || a[8] != "0" || b.size() < 11 || b[2] != "metal_creak" || b[8] != "1")
      return fail("records: " + doc.lines[size_t(bed)] + " / " + doc.lines[size_t(single)]);
    if (!handleFor(size_t(bed)))
      return fail("a placed sound has no handle on the plan");
    doc.add({"REVERB", "0", "0.3", "1.8"});
    if (!validateDocument())
      return fail("the engine rejected the placed sounds: " + utf8(notice));
    report += "placed: " + doc.lines[size_t(bed)] + "\nplaced: " + doc.lines[size_t(single)] + "\n";
    // A name the bank does not have is the engine's to refuse.
    auto good = doc;
    a[2] = "no_such_sound";
    doc.replace(size_t(bed), a);
    bool refused = !validateDocument();
    doc = good;
    if (!refused)
      return fail("the engine accepted a sound the bank does not have");
    report += "bank rows: " + std::to_string(bankSounds.size()) + "; an unknown sound is refused; REVERB validates\n";
    history.clear(), future.clear();
    return true;
  }
