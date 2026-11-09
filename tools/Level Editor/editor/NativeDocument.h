#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// An editable RMC text document, not a second world/map implementation.
// Raw Metal alone parses, validates, constructs and renders the world.
struct NativeDocument {
  std::vector<std::string> lines;
  bool trailing = true, crlf = false;
  // ---- Query index. records() and tile() used to split every line of the
  // document on every call (Starter Map: 16k lines, and the plan asks for a
  // tile 4600 times a paint). The index files each line once by kind, by
  // kind and map, by map and, for ROW, by map/layer/row; it is rebuilt on
  // the first query after a change. Every change to `lines` calls touch():
  // the methods here do, and so does every direct edit of `lines` elsewhere
  // (NativeReferences.h, the drag cancel). Size and storage address are
  // checked too, so an unmarked insert or erase still rebuilds. With
  // `verify` on (the test modes), every answer is checked against a full scan.
  std::uint64_t version = 1;
  void touch() { ++version; }
  static inline bool verify = false;
  // --perf-baseline: answer every query by the old full scan (timing only).
  static inline bool scanOnly = false;
  struct Index {
    std::uint64_t version = 0;
    size_t count = 0;
    const void *data = nullptr;
    std::unordered_map<std::string, std::vector<size_t>> byKind, byKindMap, byMap, rows;
  };
  mutable Index index;
  // Field k of a line exactly as fields() would cut it (one trailing CR
  // dropped); false when the line has no field k.
  static bool field(std::string_view line, size_t k, std::string_view &out) {
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
    size_t start = 0;
    for (size_t i = 0; i < k; ++i) {
      auto bar = line.find('|', start);
      if (bar == std::string_view::npos)
        return false;
      start = bar + 1;
    }
    auto end = line.find('|', start);
    out = line.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
    return true;
  }
  static std::string key(std::string_view a, std::string_view b) {
    std::string k;
    k.reserve(a.size() + b.size() + 1);
    k.append(a).push_back('\x1f');
    k.append(b);
    return k;
  }
  static std::string rowKey(std::string_view map, std::string_view layer, std::string_view y) {
    return key(key(map, layer), y);
  }
  bool indexed() const { return index.version == version && index.count == lines.size() && index.data == lines.data(); }
  const Index &indexNow() const {
    if (indexed())
      return index;
    Index next;
    next.version = version, next.count = lines.size(), next.data = lines.data();
    for (size_t i = 0; i < lines.size(); ++i) {
      std::string_view kind, map;
      if (!field(lines[i], 0, kind) || !field(lines[i], 1, map))
        continue; // fewer than two fields: never a record
      next.byKind[std::string(kind)].push_back(i);
      next.byKindMap[key(kind, map)].push_back(i);
      next.byMap[std::string(map)].push_back(i);
      std::string_view layer, y;
      if (kind == "ROW" && field(lines[i], 2, layer) && field(lines[i], 3, y))
        next.rows[rowKey(map, layer, y)].push_back(i);
    }
    index = std::move(next);
    return index;
  }
  static const std::vector<size_t> &found(const std::unordered_map<std::string, std::vector<size_t>> &table,
                                          const std::string &k) {
    static const std::vector<size_t> none;
    auto it = table.find(k);
    return it == table.end() ? none : it->second;
  }
  // Every line whose map field reads `map`, in file order (kind not checked).
  std::vector<size_t> onMap(int map) const {
    if (scanOnly) {
      std::vector<size_t> all(lines.size());
      for (size_t i = 0; i < all.size(); ++i)
        all[i] = i;
      return all;
    }
    auto out = found(indexNow().byMap, std::to_string(map));
    if (verify) {
      std::vector<size_t> slow;
      for (size_t i = 0; i < lines.size(); ++i)
        if (auto f = fields(lines[i]); f.size() >= 2 && f[1] == std::to_string(map))
          slow.push_back(i);
      if (slow != out)
        throw std::logic_error("document index out of date (map " + std::to_string(map) + ")");
    }
    return out;
  }
  static std::vector<std::string> fields(std::string line) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
      auto end = line.find('|', start);
      out.push_back(line.substr(start, end - start));
      if (end == std::string::npos)
        break;
      start = end + 1;
    }
    return out;
  }
  static std::string join(const std::vector<std::string> &f) {
    std::string s;
    for (auto &v : f) {
      if (!s.empty())
        s += '|';
      s += v;
    }
    return s;
  }
  void loadText(const std::string &text) {
    touch();
    lines.clear();
    trailing = !text.empty() && text.back() == '\n';
    crlf = text.find("\r\n") != std::string::npos;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
      lines.push_back(line);
  }
  void open(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
      throw std::runtime_error("Cannot open campaign");
    loadText(std::string(std::istreambuf_iterator<char>(in), {}));
  }
  std::string text() const {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
      out += lines[i];
      if (i + 1 < lines.size() || trailing)
        out += '\n';
    }
    return out;
  }
  void replace(size_t at, const std::vector<std::string> &f,bool generatedUpdate=false) {
    bool cr = !lines.at(at).empty() && lines[at].back() == '\r';
    auto next = join(f) + (cr ? "\r" : "");
    if(next!=lines[at]&&!generatedUpdate)claimAutomatic(at);
    // The index files a line by its first four fields only; a change past
    // them (a moved thing, a painted row) keeps it.
    bool keep = indexed();
    for (size_t k = 0; keep && k < 4; ++k) {
      std::string_view a, b;
      bool ha = field(lines[at], k, a), hb = field(next, k, b);
      keep = ha == hb && (!ha || a == b);
    }
    lines[at] = std::move(next);
    touch();
    if (keep)
      index.version = version;
  }
  void add(const std::vector<std::string> &f) {
    auto end = std::find_if(lines.begin(), lines.end(), [](const auto &s) {
      return s.starts_with("--- CUSTOM_CAMPAIGN_DATA_END ---");
    });
    if (end == lines.end())
      throw std::runtime_error("Missing native campaign end marker");
    lines.insert(end, join(f) + (crlf ? "\r" : ""));
    touch();
  }
  // Unmarked records, including every older map, always belong to the user.
  std::optional<int> recordLayer(size_t at) const {
    if(at==0||at>=lines.size())return {};
    auto marker=fields(lines[at-1]),record=fields(lines[at]);
    if(marker.size()!=4||record.size()<2||(marker[0]!="# EDITOR_AUTO"&&marker[0]!="# EDITOR_LAYER")||marker[1]!=record[1]||marker[3]!=record[0])return {};
    char* end=nullptr;long id=strtol(marker[2].c_str(),&end,10);if(end==marker[2].c_str()||*end||id<0||id>31)return {};return int(id);
  }
  void setRecordLayer(size_t at,int layer){if(recordLayer(at)){auto marker=fields(lines[at-1]);marker[2]=std::to_string(layer);lines[at-1]=join(marker)+(crlf?"\r":"");touch();}}
  void addOnLayer(const std::vector<std::string>& f,int layer){add({"# EDITOR_LAYER",f.at(1),std::to_string(layer),f.at(0)});add(f);}
  bool automatic(size_t at,int layer=-1) const {
    if(at==0||at>=lines.size())return false;
    auto marker=fields(lines[at-1]),record=fields(lines[at]);
    return marker.size()==4&&record.size()>1&&marker[0]=="# EDITOR_AUTO"&&marker[1]==record[1]&&
      marker[3]==record[0]&&(layer<0||marker[2]==std::to_string(layer));
  }
  void addAutomatic(const std::vector<std::string>& f,int layer){
    add({"# EDITOR_AUTO",f.at(1),std::to_string(layer),f.at(0)});add(f);
  }
  void claimAutomatic(size_t at){if(automatic(at)){
    auto marker=fields(lines[at-1]);marker[0]="# EDITOR_LAYER";lines[at-1]=join(marker)+(crlf?std::string("\r"):std::string{});touch();}}
  void eraseRecord(size_t at){
    size_t first=recordLayer(at)?at-1:at;
    lines.erase(lines.begin()+std::ptrdiff_t(first),lines.begin()+std::ptrdiff_t(at+1));touch();
  }
  void save(const std::filesystem::path &path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out)
      throw std::runtime_error("Cannot save campaign");
    out << text();
    if (!out)
      throw std::runtime_error("Campaign save failed");
  }
  std::vector<size_t> records(const std::string &kind, int map = -1) const {
    if (scanOnly)
      return recordsByScan(kind, map);
    const auto &table = indexNow();
    auto out = map < 0 ? found(table.byKind, kind) : found(table.byKindMap, key(kind, std::to_string(map)));
    if (verify && out != recordsByScan(kind, map))
      throw std::logic_error("document index out of date (" + kind + ")");
    return out;
  }
  std::vector<size_t> recordsByScan(const std::string &kind, int map = -1) const {
    std::vector<size_t> out;
    for (size_t i = 0; i < lines.size(); ++i) {
      auto f = fields(lines[i]);
      if (f.size() < 2 || f[0] != kind)
        continue;
      if (map < 0 || f[1] == std::to_string(map))
        out.push_back(i);
    }
    return out;
  }
  // The ROW lines that could hold (map, layer, y), in file order.
  const std::vector<size_t> &rowLines(int map, int layer, int y) const {
    return found(indexNow().rows, rowKey(std::to_string(map), std::to_string(layer), std::to_string(y)));
  }
  void newCampaign(const std::string &name) {
    loadText("--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|" + name +
             "|0\n--- CUSTOM_CAMPAIGN_DATA_END ---\n");
  }
  // An empty floor. No walls or props. The start stands under the light and
  // looks down, so the picture is the floor. Starter Map is the finished example.
  void addBlankMap(const std::string &name) {
    int map = int(records("MAP").size());
    addMapShell(map, name, "12", "8", "-70");
    for (int y = 0; y < 24; ++y)
      add({"ROW", std::to_string(map), "0", std::to_string(y),
           std::string(24, '.')});
    add({"LIGHT", std::to_string(map), "12", "12", "4.3", "0", "0", "2", "24",
         "4000"});
  }
  void addRoom(const std::string &name) {
    int map = int(records("MAP").size());
    addMapShell(map, name);
    for (int y = 0; y < 24; ++y)
      add({"ROW", std::to_string(map), "0", std::to_string(y),
           y == 0 || y == 23 ? std::string(24, '#')
                             : "#" + std::string(22, '.') + "#"});
    add({"LIGHT", std::to_string(map), "12", "12", "4.3", "0", "0", "1.2", "18",
         "4000"});
  }
  void addMapShell(int map, const std::string &name, const char *spawnX = "3.5",
                   const char *spawnY = "3.5", const char *pitch = "0") {
    add({"MAP",      std::to_string(map),
         name,       std::to_string(map * 24),
         "0",        spawnX,
         spawnY,     "0",
         "0",        "4.5",
         "0.34",     "industrial_night",
         "0",        "0",
         "0",        "0",
         "0",        "-1",
         "0",        "0",
         "1.570796", pitch,
         "0"});
    add({"LAYER", std::to_string(map), "0", "Ground floor", "0", "0"});
  }
  char tile(int map, int layer, int x, int y) const {
    if (scanOnly)
      return tileByScan(map, layer, x, y);
    char out = '_';
    for (auto at : rowLines(map, layer, y)) {
      std::string_view row;
      if (field(lines[at], 4, row) && x >= 0 && x < int(row.size())) {
        out = row[size_t(x)];
        break;
      }
    }
    if (verify && out != tileByScan(map, layer, x, y))
      throw std::logic_error("document index out of date (tile)");
    return out;
  }
  char tileByScan(int map, int layer, int x, int y) const {
    for (auto index : recordsByScan("ROW", map)) {
      auto f = fields(lines[index]);
      if (f.size() > 4 && f[2] == std::to_string(layer) &&
          f[3] == std::to_string(y) && x >= 0 && x < int(f[4].size()))
        return f[4][size_t(x)];
    }
    return '_';
  }
  void paint(int map, int layer, int x, int y, char value) {
    if (x < 0 || x > 23 || y < 0 || y > 23)
      return;
    if (scanOnly) {
      for (auto index : recordsByScan("ROW", map)) {
        auto f = fields(lines[index]);
        if (f.size() > 4 && f[2] == std::to_string(layer) && f[3] == std::to_string(y) && f[4].size() == 24) {
          f[4][size_t(x)] = value;
          replace(index, f);
          return;
        }
      }
      return;
    }
    if (verify) {
      // The full scan's first fitting row must be the index's first.
      auto all = recordsByScan("ROW", map);
      auto some = rowLines(map, layer, y);
      for (auto index : all) {
        auto f = fields(lines[index]);
        if (f.size() > 4 && f[2] == std::to_string(layer) && f[3] == std::to_string(y) && f[4].size() == 24) {
          if (std::find(some.begin(), some.end(), index) == some.end())
            throw std::logic_error("document index out of date (paint)");
          break;
        }
      }
    }
    for (auto index : std::vector<size_t>(rowLines(map, layer, y))) {
      auto f = fields(lines[index]);
      if (f.size() > 4 && f[2] == std::to_string(layer) &&
          f[3] == std::to_string(y) && f[4].size() == 24) {
        f[4][size_t(x)] = value;
        replace(index, f);
        return;
      }
    }
  }
};
