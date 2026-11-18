  // The Flooring tool (--flooring-test; --editing-test runs it): a look picked and dragged out
  // in a hall (soft-snapped), a click flooring a whole room, the edge trim painted and changed
  // from the Part list, on the land following the ground and made level; one Ctrl+Z each,
  // save/reopen exact, the engine taking every one. NativeFlooringMethods.inc is what it checks.
  std::vector<std::string> flooringPassed;
  void verifyFlooring() {
    auto require = [](bool okay, const std::string &why) { if (!okay) throw std::runtime_error("Flooring regression: " + why); };
    flooringPassed.clear();
    auto send = [&](HWND list, int id, int index) {
      SendMessageW(list, CB_SETCURSEL, index, 0);
      SendMessageW(window, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    };
    NativeDocument blank;
    blank.newCampaign("Flooring");
    blank.addRoom("Hall");
    adoptDocument(std::move(blank), {}, true);
    autoDressingEnabled = false, autoDetail = false;
    flooringTrim = true, flooringLevel = false;
    // ---- In the hall: a small room to floor with a click, then planks dragged out.
    tool = 1;
    gesture(13.5f, 3.5f, 19.5f, 8.5f);
    SendMessageW(window, WM_COMMAND, ToolBase + FlooringTool, 0);
    layout();
    require(tool == FlooringTool && shownControl(flooringLookList) && shownControl(GetDlgItem(window, FlooringTrimToggle)), "the Flooring tool shows no look list");
    send(flooringLookList, FlooringLookList, 0); // wood planks
    const size_t history0 = history.size();
    gesture(3.12f, 4.9f, 8.93f, 9.07f);
    const auto planks = doc.lines[doc.records("STRUCT", map).back()];
    auto f = NativeDocument::fields(planks);
    require(f[9] == "51" && f[2] == "3" && f[3] == "5" && f[4] == "9" && f[5] == "9" && namedValue(planks, "trim") == "32",
            "a drag from 3.12,4.9 to 8.93,9.07 did not lay planks snapped to 3,5 - 9,9 with a timber trim: " + planks);
    require(history.size() == history0 + 1, "laying it is not one Ctrl+Z");
    // A click in the small room floors all of it (its inside: 14,4 - 19,8).
    send(flooringLookList, FlooringLookList, 1); // floor tiles
    gesture(16.2f, 6.1f, 16.2f, 6.1f);
    const auto tiles = doc.lines[doc.records("STRUCT", map).back()];
    f = NativeDocument::fields(tiles);
    require(f[9] == "50" && f[2] == "14" && f[3] == "4" && f[4] == "19" && f[5] == "8", "a click in the room did not floor it with tiles: " + tiles);
    require(validateDocument(), "the engine refused the flooring: " + utf8(notice));
    flooringPassed.push_back("A look dragged out lays it snapped to the half metre (planks with a timber trim); a click floors a whole room (tiles); one Ctrl+Z each; engine-valid");
    // ---- Changed later: the Part list's Edge trim, and the Paint tool near the edge.
    SendMessageW(window, WM_COMMAND, ToolBase + 0, 0);
    const size_t plankLine = doc.records("STRUCT", map)[doc.records("STRUCT", map).size() - 2];
    selectRecord(int(plankLine));
    layout();
    require(finishKind() == 3 && shownControl(finishPartList), "a picked flooring shows no Part list");
    send(finishPartList, FinishPartList, 1);
    for (LRESULT i = 0; i < SendMessageW(materialList, CB_GETCOUNT, 0, 0); ++i)
      if (SendMessageW(materialList, CB_GETITEMDATA, i, 0) == 33) send(materialList, MaterialList, int(i));
    require(namedValue(doc.lines[plankLine], "trim") == "33", "the Part list did not change the edge trim: " + doc.lines[plankLine]);
    SendMessageW(window, WM_COMMAND, ToolBase + PaintTool, 0);
    choosePaint({"mat-31", materialName(31), 31, 0});
    remember();
    paintRay(6, 5.05f, 1.5f, 0, 0, -1); // straight down, 5 cm in from its north edge
    publish();
    require(namedValue(doc.lines[plankLine], "trim") == "31", "a paint click near the edge did not paint the trim: " + doc.lines[plankLine]);
    paintRay(6, 7, 1.5f, 0, 0, -1); // the middle: the floor itself
    publish();
    require(NativeDocument::fields(doc.lines[plankLine])[9] == "31", "a paint click in the middle did not paint the floor: " + doc.lines[plankLine]);
    flooringPassed.push_back("Changed later: the edge trim from the Part list (stone) and the Paint tool (brick near the edge, the floor in the middle)");
    // ---- On the land: following the ground, and made level.
    joinMapPassage = true;
    addAdjacentMap(3);
    SendMessageW(window, WM_COMMAND, Outdoors, 0);
    require(onTerrain(), "no land east of the hall");
    SendMessageW(window, WM_COMMAND, ToolBase + FlooringTool, 0);
    layout();
    require(shownControl(flooringSlopeList), "on the land the Flooring tool does not ask what to do on a slope");
    send(flooringLookList, FlooringLookList, 7); // brick paving
    send(flooringSlopeList, FlooringSlopeList, 0);
    gesture(8, 3, 10, 15);
    const auto path = doc.lines[doc.records("STRUCT", map).back()];
    require(namedValue(path, "drape") == "1" && namedValue(path, "trim") == "33", "a path on the land does not follow the ground: " + path);
    send(flooringSlopeList, FlooringSlopeList, 1);
    gesture(13, 4, 19, 9);
    const auto patio = doc.lines[doc.records("STRUCT", map).back()];
    require(namedValue(patio, "footing") == "pad" && namedValue(patio, "cap") == "33", "a patio made level is not a pad with a trimmed edge: " + patio);
    auto plan = footingAtLine(doc.records("STRUCT", map).back());
    require(plan && plan->parts.size() == 1, "the patio has no plan");
    require(validateDocument(), "the engine refused the path and patio: " + utf8(notice));
    flooringPassed.push_back("On the land: a brick path that follows the ground (drape=1) and a patio made level (a pad at " + shortNumber(plan->parts.front().surface) + " m), both with a stone edge; engine-valid");
    // ---- Save and reopen.
    auto saved = root / L".build" / L"interaction" / L"flooring-after.rmc";
    fs::create_directories(saved.parent_path());
    doc.save(saved);
    NativeDocument back;
    back.open(saved);
    require(back.text() == doc.text(), "save/reopen changed the flooring");
    flooringPassed.push_back("Saved and reopened byte for byte (.build/interaction/flooring-after.rmc)");
  }
