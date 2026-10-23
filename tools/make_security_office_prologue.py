"""Generate the Security Office prologue custom map (test of an opening cutscene).

Beat sheet: night shift at the security desk -> the friend messages from her
Lab 4 terminal -> readings spike, doors sealing -> facility alarm -> "RUN, the
service lift" -> run to the lift -> cable fails, the cab drops -> impact ->
hand off to the main campaign's first map (ENTER_CAMPAIGN).
"""
import sys

HALF, PI = 1.5707963, 3.1415927


def f(v):
    if isinstance(v, str):
        return v
    if isinstance(v, int):
        return str(v)
    s = f"{v:.7f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


out = []


def rec(*fields):
    out.append("|".join(f(x) for x in fields))


def note(text):
    out.append("# " + text)


# Walkable cells at office level. Everything else in the base layer is wall.
walk = set()
def area(x1, y1, x2, y2):
    for y in range(y1, y2 + 1):
        for x in range(x1, x2 + 1):
            walk.add((x, y))
area(2, 2, 9, 9)      # security office
area(10, 5, 10, 6)    # office doorway
area(11, 5, 18, 6)    # corridor east
area(16, 7, 18, 13)   # corridor south
area(13, 12, 20, 14)  # lift lobby
shaft = {(x, y) for x in range(15, 19) for y in range(15, 19)}

base = [["." if (x, y) in walk or (x, y) in shaft else "#" for x in range(24)] for y in range(24)]
# The row in front of the cab is a gap that the threshold step fills (as Freight).
threshold = {(x, 14) for x in range(15, 19)}
deck = [["=" if (x, y) in walk and (x, y) not in threshold else "." for x in range(24)] for y in range(24)]

out += ["META_LEVEL_ID: 1", "META_LEVEL_NAME: SECURITY OFFICE PROLOGUE", "META_DEFAULT_TARGET: CUSTOM",
        "META_CAMPAIGN_NAME: SECURITY OFFICE PROLOGUE", "", "--- CUSTOM_CAMPAIGN_DATA_START ---"]
note("Main campaign prologue (embedded, resource 398; New Game starts here): night shift at the security desk on the upper level. The")
note("friend messages from her Lab 4 terminal, the alarm goes off, you run to")
note("the service lift, its cable fails and the cab drops nine metres. Impact")
note("hands off to the main campaign's first map through ENTER_CAMPAIGN.")
rec("CAMPAIGN", "SECURITY OFFICE PROLOGUE", 0)
# Interior, absolute ceiling 3.0, office carpet floor, facing the workstation.
rec("MAP", 0, "Depthworks / Security Office", 0, 0, 4.6, 5.75, 0.0, 0, 3.0, 0.30, "industrial_night",
    0, 0, 0, 0, 0, -1, 1, 0, -HALF, -14, 1)
rec("LAYER", 0, 0, "Shaft floor / sub-level", -9.0, 0.0)
for y, row in enumerate(base):
    rec("ROW", 0, 0, y, "".join(row))
rec("LAYER", 0, 1, "Security level", 0.0, 0.22)
for y, row in enumerate(deck):
    rec("ROW", 0, 1, y, "".join(row))

note("Fixture bases and terminal heights are relative to the bottom layer (-9),")
note("so props on the office floor (z 0) use base 9.0. OFFICE pack models 24-38.")
DESK, CORNER, CHAIR, CRT, KEYS, TOWER, TV, CAB_WIDE, CAB_TALL, PHONE, MUG, COOLER, COFFEE, BOOKS, BINDER = range(24, 39)
F = 9.0            # office floor, relative to the bottom layer
TOP = F + 0.891    # desk top
def fix(model, x, y, base, w, d, h, yaw, solid):
    rec("FIXTURE", 0, model, x, y, base, w, d, h, yaw, solid)
note("Room finish: block walls, drop ceiling and carpet laid inside the shell.")
for x1, y1, x2, y2 in ((2.0, 2.0, 10.0, 2.06), (2.0, 9.94, 10.0, 10.0), (2.0, 2.0, 2.06, 10.0), (9.94, 2.0, 10.0, 5.0), (9.94, 7.0, 10.0, 10.0)):
    rec("STRUCT", 0, x1, y1, x2, y2, 0.0, 3.0, 0, 21)
rec("STRUCT", 0, 2.0, 2.0, 10.0, 10.0, 2.88, 3.0, 0, 22)
rec("STRUCT", 0, 2.06, 2.06, 9.94, 9.94, 0.0, 0.012, 0, 23)
note("CCTV bank: four CRT sets on wide filing cabinets along the north wall.")
for cx in (3.3, 5.9):
    fix(CAB_WIDE, cx, 2.45, F, 2.4, 0.7, 0.706, 0, 1)
for tx in (2.75, 3.85, 5.35, 6.45):
    fix(TV, tx, 2.42, F + 0.706, 0.834, 0.629, 0.913, 0, 0)
note("Security desk facing the bank. The terminal stands on its front edge, so")
note("the desk's own CRT is the computer the player uses.")
fix(DESK, 4.6, 4.3, F, 2.205, 1.092, 0.891, 0, 1)
fix(CORNER, 7.5, 4.1, F, 2.205, 2.211, 0.891, 0, 1)
fix(CRT, 4.6, 4.15, TOP, 0.602, 0.533, 0.567, 0, 0)
fix(KEYS, 4.6, 4.6, TOP, 0.683, 0.225, 0.041, 0, 0)
fix(CRT, 7.9, 3.6, TOP, 0.602, 0.533, 0.567, -0.45, 0)
fix(TOWER, 5.35, 4.0, F, 0.262, 0.609, 0.609, 0, 0)
fix(PHONE, 3.75, 4.35, TOP, 0.333, 0.333, 0.089, 0.3, 0)
fix(MUG, 5.25, 4.55, TOP, 0.139, 0.137, 0.123, 1.1, 0)
fix(BINDER, 3.35, 4.2, TOP, 0.093, 0.301, 0.424, 0.2, 0)
fix(CHAIR, 3.4, 5.6, F, 0.94, 0.862, 1.096, 2.4, 1)
rec("TERMINAL", 0, 4.6, 4.8, 9.0, 0, "SECURITY DESK 2 / NIGHT SHIFT",
    "DIRECT LINE: LAB 4 IS MESSAGING YOU.", "E: OPEN THE MESSAGE.", "office_chat", 0, "", PI)
note("Files, coffee and water along the side walls.")
fix(CAB_TALL, 9.6, 2.6, F, 0.485, 0.525, 1.409, -HALF, 1)
fix(CAB_TALL, 9.6, 3.2, F, 0.485, 0.525, 1.409, -HALF, 1)
fix(COOLER, 9.65, 8.1, F, 0.378, 0.449, 1.396, -HALF, 1)
fix(COFFEE, 2.75, 9.25, F, 1.254, 1.283, 2.124 * 0.85, 0, 1)
# Bookcase at full size: shelf boards measured at 0.521, 1.015, 1.511 and 2.011 m,
# each gap 0.47 m, so 0.424 m binders stand upright inside the 0.489 m depth.
fix(BOOKS, 2.3, 6.8, F, 1.0, 0.489, 2.5, HALF, 1)
for y in (6.45, 6.56, 6.67, 6.78, 6.89):
    fix(BINDER, 2.3, y, F + 1.015, 0.093, 0.301, 0.424, HALF, 0)
for y in (6.95, 7.06, 7.17):
    fix(BINDER, 2.3, y, F + 0.521, 0.093, 0.301, 0.424, HALF, 0)
rec("SIGN", 0, 6.0, 9.92, 2.2, 2.2, 0.45, PI, "SECURITY / LEVEL 1", "NIGHT SHIFT / DESK 2", 14726704)
rec("LIGHT", 0, 4.6, 5.2, 2.8)
rec("LIGHT", 0, 7.5, 8.0, 2.8)

note("Corridor and lobby. Signs and lamps lead to the service lift.")
for x1, y1, x2, y2 in ((10.0, 5.0, 19.0, 7.0), (16.0, 7.0, 19.0, 12.0)):
    rec("STRUCT", 0, x1, y1, x2, y2, 0.0, 0.012, 0, 24)
for x1, y1, x2, y2 in ((10.0, 5.0, 16.0, 5.06), (10.0, 6.94, 16.0, 7.0), (18.94, 5.0, 19.0, 12.0), (15.94, 7.0, 16.0, 12.0)):
    rec("STRUCT", 0, x1, y1, x2, y2, 0.0, 3.0, 0, 21)
rec("SIGN", 0, 14.0, 4.92, 2.0, 2.0, 0.42, 0, "SERVICE LIFT >", "LOBBY / SOUTH", 6089568)
rec("LIGHT", 0, 13.0, 6.0, 2.85)
rec("LIGHT", 0, 17.0, 9.5, 2.85)
rec("LIGHT", 0, 16.5, 13.0, 2.85)
rec("SIGN", 0, 21.0, 13.0, 2.0, 1.8, 0.42, -HALF, "SERVICE LIFT", "SUB-LEVEL ACCESS", 14726704)
rec("LIGHT", 0, 17.0, 17.0, -6.6)
note("Flush one-step threshold opens the deck guardrail onto the cab (as Freight).")
rec("STAIR", 0, 15, 13, 19, 15, -0.12, 0, 1, 1, 1)

note("Service lift cab: starts at the shaft floor and is called up on arrival,")
note("unseen behind the lobby. Speed 7 m/s, so the release is a drop, not a ride.")
rec("CARGO_LIFT", 0, 15, 15, 19, 19, -9, 0, 7, "office_lift_call", "office_lift_release",
    "office_lift_mm", "office_lift_down", "office_lift_arrived", "office_lift_bottom")

note("Objectives")
rec("OBJECTIVE_TEXT", 0, "office_answer", "Answer the message on your workstation")
rec("OBJECTIVE_TEXT", 0, "office_run", "Get to the service lift")

note("1. Chat. Started by the workstation; regions are unreachable on purpose.")
rec("TIMED_SEQUENCE", 0, "office_chat", "office_alarm", -2, -2, -1, -1, -100, 100, 21000, 20500, 0, 0, 27, 0.3, 1)
chat = [
    (0, "LAB 4: you still on shift?"),
    (3500, "YOU: always. quiet night up here."),
    (7000, "LAB 4: not down here. readings just went off the scale."),
    (11000, "LAB 4: they're sealing the sub-level doors. nobody says why."),
    (14500, "YOU: want me to call it in?"),
    (17500, "LAB 4: no. something's wrong. really wrong. hold on"),
]
for t, line in chat:
    rec("SEQUENCE_CUE", 0, "office_chat", t, 27, 4.6, 4.15, 0.35, 1.4 if line.startswith("LAB") else 1.1, line)

note("2. Alarm: the klaxon repeats over a facility-wide alarm bed while the friend")
note("tells you to run. Sounds 42 Alarm and 43 AlarmDistant.")
rec("TIMED_SEQUENCE", 0, "office_alarm", "office_run", -2, -2, -1, -1, -100, 100, 60000, 5200, 1900, 60000, 42, 0.8, 1)
rec("SEQUENCE_CUE", 0, "office_alarm", 0, 43, 12.0, 6.0, 0.75, 1, "FACILITY ALARM / CONTAINMENT LOCKDOWN")
rec("SEQUENCE_CUE", 0, "office_alarm", 2200, 27, 4.6, 4.15, 0.4, 1.4, "LAB 4: they're locking everything. get out of there")
rec("SEQUENCE_CUE", 0, "office_alarm", 4800, 27, 4.6, 4.15, 0.45, 1.4, "LAB 4: RUN. the service lift past the lobby. GO")

note("3. Board the cab: doors seal, the cable lets go, the brake fails.")
rec("TIMED_SEQUENCE", 0, "office_drop", "office_lift_release", -2, -2, -1, -1, -100, 100, 4000, 2200, 0, 0, 35, 0.5, 1)
rec("SEQUENCE_CUE", 0, "office_drop", 0, 28, 17.0, 17.0, 0.6, 0.6, "LIFT: DOORS SEALING")
rec("SEQUENCE_CUE", 0, "office_drop", 1000, 36, 17.0, 17.0, 0.9, 0.8, "LIFT: LOAD UNSTABLE")
rec("SEQUENCE_CUE", 0, "office_drop", 2100, 38, 17.0, 17.0, 1.0, 1.1, "CABLE FAILURE")

note("4. Impact, then hand off to the main campaign.")
rec("TIMED_SEQUENCE", 0, "office_crash", "office_prologue_done", -2, -2, -1, -1, -100, 100, 5000, 4200, 0, 0, 27, 0.2, 1)
rec("SEQUENCE_CUE", 0, "office_crash", 0, 34, 17.0, 17.0, 1.0, 0.8, "")
rec("SEQUENCE_CUE", 0, "office_crash", 1400, 27, 17.0, 17.0, 0.3, 0.5, "EMERGENCY POWER / SUB-LEVEL / NO SIGNAL FROM LAB 4")

rec("SCRIPT_EVENT", 0, "office_arrival", 0, 0, 24, 24, -100, 100, "", 1, 0)
rec("SCRIPT_ACTION", 0, "office_arrival", "OBJECTIVE", "office_answer", 1)
rec("SCRIPT_ACTION", 0, "office_arrival", "STATE", "office_lift_call", 1)
rec("SCRIPT_EVENT", 0, "office_run_now", 0, 0, 24, 24, -100, 100, "office_run", 1, 0)
rec("SCRIPT_ACTION", 0, "office_run_now", "OBJECTIVE", "office_answer", 2)
rec("SCRIPT_ACTION", 0, "office_run_now", "OBJECTIVE", "office_run", 1)
rec("SCRIPT_ACTION", 0, "office_run_now", "CHECKPOINT")
rec("SCRIPT_EVENT", 0, "office_board", 15.3, 15.3, 18.7, 18.7, -1, 1, "office_run", 1, 0)
rec("SCRIPT_ACTION", 0, "office_board", "STATE", "office_drop", 1)
rec("SCRIPT_EVENT", 0, "office_impact", 0, 0, 24, 24, -100, 100, "office_lift_bottom", 1, 0)
rec("SCRIPT_ACTION", 0, "office_impact", "SHAKE", 1.6)
rec("SCRIPT_ACTION", 0, "office_impact", "STATE", "office_crash", 1)
rec("SCRIPT_ACTION", 0, "office_impact", "OBJECTIVE", "office_run", 2)
rec("SCRIPT_EVENT", 0, "office_handoff", 0, 0, 24, 24, -100, 100, "office_prologue_done", 1, 0)
rec("SCRIPT_ACTION", 0, "office_handoff", "ENTER_CAMPAIGN")
out.append("--- CUSTOM_CAMPAIGN_DATA_END ---")

open(sys.argv[1], "w", newline="\r\n").write("\n".join(out) + "\n")
print(len(out), "lines")
