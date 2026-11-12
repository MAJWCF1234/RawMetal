#pragma once
#include "NativeDocument.h"

inline void eraseNativeObject(NativeDocument &doc, size_t index) {
  const auto object = NativeDocument::fields(doc.lines.at(index));
  if (object.size() < 2)
    throw std::runtime_error("Select a native object record");
  if (object[0] == "TIMED_SEQUENCE")
    for (auto i : doc.records("ACTOR", std::stoi(object[1])))
      if (auto a = NativeDocument::fields(doc.lines[i]); a.size() > 2 && object.size() > 2 && a[2] == object[2])
        throw std::runtime_error("People walk on this clock. Delete them first.");
  // A person is one ACTOR plus every ACTOR_* keyed by its index. Removing any
  // part removes the person, and later people move down one so indices stay sequential.
  if (object[0] == "ACTOR" || object[0].starts_with("ACTOR_")) {
    if (object.size() < 3) throw std::runtime_error("Malformed person record");
    int gone = std::stoi(object[0] == "ACTOR" ? object.at(7) : object[2]);
    std::vector<std::string> kept;
    for (size_t lineIndex=0;lineIndex<doc.lines.size();++lineIndex) {
      const auto& line=doc.lines[lineIndex];
      if(lineIndex+1<doc.lines.size()&&doc.recordLayer(lineIndex+1)){
        auto next=NativeDocument::fields(doc.lines[lineIndex+1]);
        bool sameMap=next.size()>2&&next[1]==object[1];
        if(sameMap&&((next[0]=="ACTOR"&&next.size()>7&&std::stoi(next[7])==gone)||(next[0].starts_with("ACTOR_")&&std::stoi(next[2])==gone)||(next[0]=="SEQUENCE_SIGHT"&&next.size()>3&&std::stoi(next[3])==gone)))continue;
      }
      auto f = NativeDocument::fields(line);
      if(f.size()>3&&f[1]==object[1]&&f[0]=="SEQUENCE_SIGHT"){
        int target=std::stoi(f[3]);if(target==gone)continue;
        if(target>gone)f[3]=std::to_string(target-1);
        kept.push_back(NativeDocument::join(f)+(!line.empty()&&line.back()=='\r'?"\r":""));continue;
      }
      // A creature's quarry (CREATURE field 10) is a person on its map: it follows the renumbering, and loses a quarry that is gone.
      if(f.size()>10&&f[1]==object[1]&&f[0]=="CREATURE"&&!f[10].empty()){
        int target=std::stoi(f[10]);
        if(target==gone||target>gone){
          if(target==gone)f[10].clear();else f[10]=std::to_string(target-1);
          while(f.size()>6&&f.back().empty())f.pop_back();
          kept.push_back(NativeDocument::join(f)+(!line.empty()&&line.back()=='\r'?"\r":""));continue;
        }
      }
      size_t at =f.size() > 7 && f[0] == "ACTOR" ? 7 : f.size() > 2 && f[0].starts_with("ACTOR_") ? 2 : 0;
      if (!at || f[1] != object[1]) { kept.push_back(line); continue; }
      int i = std::stoi(f[at]);
      if (i == gone) continue;
      if (i > gone) f[at] = std::to_string(i - 1);
      if(f[0]=="ACTOR_KEY"&&f.size()>10){int target=std::stoi(f[10]);
        if(target==gone)f[10]="-1";else if(target>gone)f[10]=std::to_string(target-1);}
      kept.push_back(NativeDocument::join(f) + (!line.empty() && line.back() == '\r' ? "\r" : ""));
    }
    doc.lines = std::move(kept);
    doc.touch();
    return;
  }
  // A trigger owns its conditions, actions and firing rule; leaving them would dangle.
  if (object[0] == "SCRIPT_EVENT" && object.size() > 2) {
    auto id = object[2];
    doc.eraseRecord(index);
    auto children=doc.onMap(std::stoi(object[1]));
    for(auto it=children.rbegin();it!=children.rend();++it){auto f=NativeDocument::fields(doc.lines[*it]);
      if(f.size()>2&&f[2]==id&&(f[0]=="SCRIPT_ACTION"||f[0]=="SCRIPT_REQUIRE"||f[0]=="SCRIPT_TRIGGER"))doc.eraseRecord(*it);
    }
    doc.touch();
    return;
  }
  std::vector<std::pair<size_t, std::vector<std::string>>> updates;
  if (object[0] == "FIXTURE" || object[0] == "CLUTTER") {
    auto matching = doc.records(object[0], std::stoi(object[1]));
    int ordinal = int(std::find(matching.begin(), matching.end(), index) -
                      matching.begin());
    for (size_t i = 0; i < doc.lines.size(); ++i) {
      auto f = NativeDocument::fields(doc.lines[i]);
      if (f.size() < 2 || f[1] != object[1])
        continue;
      int field = -1;
      if (f[0] == "LEAD")
        field = object[0] == "FIXTURE" ? 18 : 13;
      if (object[0] == "FIXTURE" && f[0] == "BREAKABLE" && f.size() == 15)
        field = 13;
      if (field < 0 || size_t(field) >= f.size() || f[size_t(field)].empty())
        continue;
      int reference = std::stoi(f[size_t(field)]);
      if (reference == ordinal)
        throw std::runtime_error(
            "This object has a cable or breakable attachment. Detach its "
            "linked record before deleting it.");
      if (reference > ordinal) {
        f[size_t(field)] = std::to_string(reference - 1);
        updates.emplace_back(i, std::move(f));
      }
    }
  }
  for (const auto &[line, fields] : updates)
    doc.replace(line, fields, true); // Ordinal maintenance is not a user edit.
  doc.eraseRecord(index);
  doc.touch();
}

// The retired C# editor wrote people the engine refuses: on "editor_clock"
// without writing that clock ("Invalid authored actor track"), and guards
// (ACTOR_AI mode 3) with no post to stand ("Invalid guard AI configuration").
// Opening such a file adds what the Person tool writes for them: the clock
// just before the first person who needs it, and a guard's sight record and
// post (where the guard stands) after their AI record. Files whose people are
// complete are left exactly as they are. Returns how many records were added.
inline int repairRetiredEditorPeople(NativeDocument &doc) {
  const std::string cr = doc.crlf ? "\r" : "";
  std::vector<std::pair<std::string, std::string>> clocks, guarded, posted;
  for (const auto &line : doc.lines) {
    auto f = NativeDocument::fields(line);
    if (f.size() > 2 && f[0] == "TIMED_SEQUENCE")
      clocks.emplace_back(f[1], f[2]);
    if (f.size() > 2 && f[0] == "ACTOR_GUARD")
      guarded.emplace_back(f[1], f[2]);
    if (f.size() > 2 && f[0] == "ACTOR_PATROL")
      posted.emplace_back(f[1], f[2]);
  }
  auto has = [](const auto &list, const std::pair<std::string, std::string> &key) {
    return std::find(list.begin(), list.end(), key) != list.end();
  };
  int added = 0;
  for (size_t i = 0; i < doc.lines.size(); ++i) {
    auto f = NativeDocument::fields(doc.lines[i]);
    if (f.size() > 2 && f[0] == "ACTOR" && !f[2].empty() && !has(clocks, {f[1], f[2]})) {
      std::vector<std::string> clock{"TIMED_SEQUENCE", f[1], f[2], f[2] + "_done",
                                     "0", "0", "24", "24", "-100", "100",
                                     "60000", "60000", "0", "0", "0", "0", "1", "1"};
      doc.lines.insert(doc.lines.begin() + std::ptrdiff_t(i), NativeDocument::join(clock) + cr);
      doc.touch();
      clocks.emplace_back(f[1], f[2]);
      ++added, ++i;
      continue;
    }
    if (f.size() < 4 || f[0] != "ACTOR_AI" || f[3] != "3")
      continue;
    std::pair<std::string, std::string> who{f[1], f[2]};
    if (has(posted, who))
      continue;
    std::vector<std::string> key;
    for (const auto &line : doc.lines)
      if (auto k = NativeDocument::fields(line); k.size() > 6 && k[0] == "ACTOR_KEY" && k[1] == f[1] && k[2] == f[2]) {
        key = std::move(k);
        break;
      }
    if (key.empty())
      continue;
    size_t at = i + 1;
    if (!has(guarded, who)) {
      doc.lines.insert(doc.lines.begin() + std::ptrdiff_t(at++),
                       NativeDocument::join({"ACTOR_GUARD", f[1], f[2], "12", "0.5", "600", "4000", ""}) + cr);
      guarded.push_back(who);
      ++added;
    }
    doc.lines.insert(doc.lines.begin() + std::ptrdiff_t(at++),
                     NativeDocument::join({"ACTOR_PATROL", f[1], f[2], key[4], key[5], key[6], "1000"}) + cr);
    posted.push_back(who);
    ++added;
    doc.touch();
    i = at - 1;
  }
  return added;
}
