"""Refresh the existing temporary Ash custom map with a stitched squad arena."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
evidence = root / "diagnostics/kill-squad-20261011"
editor = evidence / "editor-clean-head"
editor.mkdir(parents=True, exist_ok=True)
for name in ("server.py", "test_map_payload.py"):
    (editor/name).write_bytes(subprocess.check_output(["git", "show", "HEAD:tools/level-editor/"+name], cwd=root))
for name in ("runtime_records.py", "test_runtime_records.py"):
    shutil.copyfile(root/"tools/level-editor"/name, editor/name)
sys.path.insert(0, str(editor))
import server

path = root / "custom maps/RawMetal-Horror-OpenWorld.json"
project = json.loads(path.read_text(encoding="utf-8-sig"))
project["chunks"] = [project["chunks"][0]]
rows = project["chunks"][0]["layers"][0]["rows"]
for y in range(18,23):
    rows[y] = rows[y].ljust(24, "_")[:23] + "."
records = [r for r in project["runtimeRecords"] if r.split("|")[1]=="0"]
records = [r.replace("ACTOR|0|ash_guard_clock|0|1.6|0|0|1", "ACTOR|0|ash_guard_clock|4|1.6|0|0|1") for r in records]

def area(index, gx, gy, name):
    rows = [list("#"*24)] + [list("#"+"."*22+"#") for _ in range(22)] + [list("#"*24)]
    if index==1:
        for y in range(18,23): rows[y][0]="."
        for x in range(17,21): rows[23][x]="."
    else:
        for x in range(17,21): rows[0][x]="."
    lid=f"cleanup-floor-{index}"
    chunk={"id":f"ash-cleanup-{index}","number":index+1,"name":name,"gx":gx,"gy":gy,
           "layers":[{"id":lid,"name":"Industrial floor","z":0,"ceilingHeight":4,"thickness":0,"rows":["".join(r) for r in rows]}],"objects":[]}
    chunk["layers"].append({"id":f"cleanup-roof-{index}","name":"Supported concrete roof","z":4,"ceilingHeight":.2,"thickness":.2,"rows":["="*24 for _ in range(24)]})
    def obj(kind, name, x,y,z=0,**props):
        item={"id":f"cleanup-{index}-{len(chunk['objects'])}","type":kind,"name":name,"layerId":lid,"x":x,"y":y,"z":z,"rotation":0};item.update(props);chunk["objects"].append(item)
    for x,y in ((7,7),(16,7),(7,17),(17,17)):
        obj("asset","Supported ceiling light",x,y,3.82,model="src/assets/facility/source/ceiling_lamp_fps_1.fbx",w=1,d=.30,h=.18,scale=1)
        obj("light","Warm industrial light",x,y,3.7)
    for x,y in ((10,16),(16,8),(7,20)) if index==1 else ((8,8),(14,12),(19,17)):
        obj("box","Cargo cover",x,y,w=.9,d=.9,h=.85)
    obj("box","Full-height escape cover",4.7 if index==1 else 17,19 if index==1 else 8,w=.30,d=2.5,h=1.85)
    obj("label","ENTER WEST / activate cleanup squad / retreat through south passage" if index==1 else "SOUTH REFUGE / physical pursuit and lost-sight search",3,20)
    project["chunks"].append(chunk)
    return obj

obj=area(1,1,0,"Cleanup / cargo ambush")
area(2,1,1,"Cleanup / pursuit refuge")
records += ["TIMED_SEQUENCE|1|cleanup_start|cleanup_enabled|1|18|3.5|22|-0.2|3|1000|100|0|0|0|0|1|0",
            "TIMED_SEQUENCE|1|cleanup_clock|cleanup_clock_done|0|0|24|24|-0.2|4|60000|60000|0|0|0|0|1|1"]
for index,(x,y) in enumerate(((8.5,12.5),(14.5,11.5),(19.5,12.5),(11.5,8.5),(15.5,6.5),(20.5,16.5))):
    visual, speed, aim, recovery, damage, role = (
        (5, 1.45, 700, 1100, 6, "Engineer / terminal, pistol and five-drone support"),
        (4, 1.75, 550, 900, 8, "Assault / flanker"),
        (6, 1.55, 900, 1400, 12, "Sharpshooter / ranged anchor"),
        (7, 1.25, 650, 1000, 10, "Heavy / suppression anchor"),
        (8, 1.55, 800, 1200, 6, "Medic / finite squad treatment"),
        (9, 2.1, 450, 850, 6, "Rusher / close assault"),
    )[index]
    obj("label",f"CLEANUP {index+1} / {role}",x,y)
    records += [f"ACTOR|1|cleanup_clock|{visual}|1.6|0|0|{index}",f"ACTOR_KEY|1|{index}|0|{x}|{y}|0|0|0|0",
                f"ACTOR_KEY|1|{index}|60000|{x}|{y}|0|0|0|1",f"ACTOR_HEALTH|1|{index}|90|cleanup_damage_{index}|cleanup_dead_{index}",
                f"ACTOR_AI|1|{index}|3|cleanup_enabled|{speed}|2|6",f"ACTOR_GUARD|1|{index}|18|0.3|600|5000|cleanup_alert",
                f"ACTOR_PATROL|1|{index}|{x}|{y}|0|1000",f"ACTOR_WEAPON|1|{index}|{2 if index in (0,4) else 1}|{aim}|{recovery}|{12 if index==0 else 18}|{damage}",
                f"ACTOR_FACTION|1|{index}|4|depthworks_cleanup",f"ACTOR_TACTICS|1|{index}|{1 if index==3 else 0}|{2 if index==0 else 0}",
                f"ACTOR_ROLE|1|{index}|{(2,1,3,4,5,6)[index]}"]
project["runtimeRecords"]=records
payload,warnings=server.build_map_payload(project,project["chunks"][0]["id"],1,project["name"],"CUSTOM")
assert not warnings,warnings
path.write_text(json.dumps(project,indent=2)+"\n",encoding="utf-8")
(root/"custom maps/RawMetal-Horror-OpenWorld.txt").write_text(payload,encoding="utf-8")
message=f"PASS: matching clean HEAD editor export, {len(records)} records, three stitched experimental chunks, zero warnings.\n"
(evidence/"editor-export.txt").write_text(message,encoding="utf-8")
print(message)
