#pragma once
#include "NativeReferences.h"

// CLI actions edit exactly the same native document as the interactive tools.
// ADD carries a complete engine record; its interpretation belongs to RawMetal.
inline void runNativeCommands(NativeDocument &doc,
                              const std::filesystem::path &script) {
  std::ifstream input(script);
  if (!input)
    throw std::runtime_error("Cannot open editor command script");
  std::string line;
  int number = 0;
  while (std::getline(input, line)) {
    ++number;
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty() || line[0] == '#')
      continue;
    auto f = NativeDocument::fields(line);
    try {
      if (f[0] == "NEW" && f.size() == 2)
        doc.newCampaign(f[1]);
      else if (f[0] == "OPEN" && f.size() == 2) {
        doc.open(std::filesystem::u8path(f[1]));
        repairRetiredEditorPeople(doc);
      }
      else if (f[0] == "SAVE" && f.size() == 2)
        doc.save(std::filesystem::u8path(f[1]));
      else if (f[0] == "ROOM" && f.size() == 2)
        doc.addRoom(f[1]);
      else if (f[0] == "ADD" && f.size() > 2)
        doc.add({f.begin() + 1, f.end()});
      else if (f[0] == "PAINT" && f.size() == 6 && f[5].size() == 1)
        doc.paint(std::stoi(f[1]), std::stoi(f[2]), std::stoi(f[3]),
                  std::stoi(f[4]), f[5][0]);
      else if (f[0] == "SET" && f.size() == 6) {
        auto matches = doc.records(f[1], std::stoi(f[2]));
        auto index = matches.at(std::stoul(f[3]));
        auto values = NativeDocument::fields(doc.lines[index]);
        auto field = std::stoul(f[4]);
        if (field >= values.size())
          values.resize(field + 1);
        values[field] = f[5];
        doc.replace(index, values);
      } else if (f[0] == "REMOVE" && f.size() == 4)
        eraseNativeObject(doc, doc.records(f[1], std::stoi(f[2])).at(std::stoul(f[3])));
      else if (f[0] == "CLEAR" && f.size() == 2) {
        const auto &map = f[1];
        std::erase_if(doc.lines, [&](const std::string &record) {
          auto fields = NativeDocument::fields(record);
          if (fields.size() < 2 || fields[1] != map)
            return false;
          auto &kind = fields[0];
          return kind != "MAP" && kind != "LAYER" && kind != "ROW" &&
                 kind != "CAMPAIGN" && kind != "TERRAIN" && kind != "GROUND" &&
                 kind != "HORIZON" && kind != "TERRAIN_FLAT";
        });
      } else
        throw std::runtime_error("Unknown command or incorrect argument count");
    } catch (const std::exception &error) {
      throw std::runtime_error("Command line " + std::to_string(number) + ": " +
                               error.what());
    }
  }
}
