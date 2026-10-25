"""The Descent: world plan for campaign chunks 34 onward (manifest maps 19-136).

This file is the single source of truth for where every new chunk sits, how it
connects, and what it is for. Run it to regenerate:

  python tools/campaign/plan.py            # check, then write the generated files
  python tools/campaign/plan.py --check    # only check the plan

It writes:
  - the CampaignChunks / CampaignMapNames entries for chunks 34+ between the
    PLAN markers in src/world/WorldDefinition.h;
  - src/world/CampaignChapters.cpp, which registers every chapter file;
  - a placeholder builder file per district (src/world/CampaignChapter<Key>.cpp)
    ONLY when that file does not exist yet. Authored files are never touched;
  - the district/chunk tables at the end of docs/Design/descent-campaign-20261025.md.

Grid: chunk (gx, gy) has its origin at world (169 + 24*gx, 696 + 24*gy), south
of CARGO / DEEP JUNCTION (map 31). x runs east, y runs south.

Edges are the contract between neighbours. Each edge is a passage across one
side of the chunk: side N/S/E/W, the local range along that side, the floor
height z where it is crossed (absolute), and 'seam' (open floor) or 'door'
(an entry door on the north edge / transfer door on the south edge, paired by
world X). A door may require a state. Every edge must be matched by the
neighbour's opposite edge with the same range and z.

Keep every floor and ceiling inside [-30, +28]: the lighting cache, saves and
AI reject heights far outside it. Big descents are cargo lifts and tall halls;
height is won back with stairs in transitional spaces.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OX, OY = 169, 696
FIRST = 34

# Music cues. The new chapter cues fall back to existing loops until their
# tracks are wired into AudioEngine (see NEW_CUES).
NEW_CUES = {'Research': 'Subliminal', 'Clinic': 'DeathClose', 'Mines': 'Heat',
            'Depths': 'Darkness', 'Haven': 'Undercover', 'Boss': 'DeathClose'}
WIRED_CUES = set()  # cues from NEW_CUES that AudioEngine already plays

def E(side, a, b, z, kind='seam', state=None):
    return dict(side=side, a=float(a), b=float(b), z=float(z), kind=kind, state=state)

DISTRICTS = []
FILE = [None]
def district(key, title, chapter, manifest, music, brief):
    FILE[0] = None
    DISTRICTS.append(dict(key=key, title=title, chapter=chapter, manifest=manifest, music=music, brief=brief, chunks=[]))
def part(key):
    """Chunks from here on go to src/world/CampaignChapter<key>.cpp (one author per file)."""
    FILE[0] = key
def chunk(idx, gx, gy, name, base, ceil, group, edges, brief, ambient=.26, music=None, manifest=''):
    DISTRICTS[-1]['chunks'].append(dict(idx=idx, gx=gx, gy=gy, name=name, base=float(base), ceil=float(ceil),
        group=group, edges=edges, brief=brief, ambient=ambient, music=music or DISTRICTS[-1]['music'], manifest=manifest,
        file=FILE[0] or DISTRICTS[-1]['key']))

# =============================================================================
# CHAPTER 1B: POWER, WATER, MAINTENANCE AND THE RESTRICTED PERIMETER (19-44)
# =============================================================================
district('Power', 'POWER DISTRIBUTION', 1, '19-24', 'Subliminal', """
The deep plant's electrical heart, dead since the surge. A Dead Space style loop:
the hub shows three routes and one sealed gate (FEEDER 9 / MACHINE HALL / NO
SUPPLY). The player black-starts the plant: Substation A is dead and points to
Emergency Generation through the Cable Trench; starting the generators
(pwr_gen_online) brings the district's lights back on (switched lamps) and makes
Substation A and the Switch Hall live (state-gated electrical hazards appear on
floors that were safe before). The Switch Hall's catwalks then reach Load
Control, whose three-step load transfer (terminals with prerequisites) sets
pwr_feeder9: the hub gate opens and the mail relay wakes. One queued message
from LAB 4, stamped after the crash: "if you're alive don't come down to the
labs. go up." First high-voltage territory: hazard signage, arc flash
warnings, insulated mats, cages, bus ducts, cable trays everywhere. The
Cleanup Force swept this district: two dead technicians by the substation,
executed, not killed by creatures. Enemies: huntsmen in the trench (dark,
crouch), wasps in the switch hall, a two-man cleanup team at Load Control.""")
chunk(34, 0, 0, 'POWER DISTRIBUTION', -25, -18, 10,
      [E('N', 16.5, 21.5, -25, 'door'), E('S', 9, 15, -25)],
      """Intake gallery behind the Deep Junction blast door (the main line's blast door opens into it).
The freight track ends at a buffer; a technicians' gallery with racked insulated tools, an arc-flash PPE
station, the district's first DANGER HIGH VOLTAGE signage and a dead switchboard. Emergency lamps only
(red-orange, low). Seam south into the hub.""", ambient=.22, manifest='19')
chunk(35, 0, 1, 'DISTRIBUTION / HUB', -25, -16, 10,
      [E('N', 9, 15, -25), E('E', 9, 14, -25), E('W', 9, 14, -25), E('S', 10, 14, -25, 'door', 'pwr_feeder9')],
      """The distribution concourse: a tall hall of bus ducts and a mimic board showing the whole district
(Substation A east, Switch Hall west, Feeder 9 south, all dark). The south gate to the Machine Hall is
sealed: requires pwr_feeder9. Mail relay terminal (wakes after pwr_feeder9 with LAB 4's queued message;
use a state-gated cue/terminal). Main lights switched on by pwr_gen_online; emergency lights before.""", manifest='19')
chunk(36, 1, 1, 'SUBSTATION A', -28, -17, 11,
      [E('W', 9, 14, -25), E('E', 16, 19, -28)],
      """Transformer bays behind steel mesh cages on a -25 deck over a -28 cable basement. Dead at first:
SUBSTATION A / BLACK START REQUIRED / EMERGENCY GENERATION VIA CABLE TRENCH. A stair drops to the basement and
the trench east. After pwr_gen_online the bays energize: arcing floor hazards appear in the basement walk
(enabledFlag pwr_gen_online) so the way back is over the deck; closing the breakers (pwr_sub_a) is needed
before Load Control accepts the transfer. Executed technicians (dead Worker actors) by the breaker panel.""", manifest='20')
chunk(37, -1, 1, 'SWITCH HALL', -28, -10, 12,
      [E('E', 9, 14, -25), E('S', 10, 14, -19)],
      """Vertical chamber: switchgear lines a pit at -28; catwalks at -25 and -19 cross over live equipment
(state-gated arcs after pwr_gen_online). Stairs climb the walls between catwalk levels. The -19 catwalk exits
south to Load Control. Wasps nest in the upper chamber. 18 m of height, cable risers, a cage ladder look.""", manifest='21')
chunk(38, 2, 1, 'CABLE TRENCH', -28, -25.2, 11,
      [E('W', 16, 19, -28), E('S', 14, 18, -28)],
      """Underfloor maintenance routes beneath the substation: narrow trenches between cable racks, a low
2.8 m ceiling with crouch-only sections under ducts, standing water near live cables (Anomaly hum: resonance
pools where metal is wet and still). Pitch dark: the flashlight matters. Huntsmen. Pull boxes, cable drums.""", ambient=.18, manifest='22')
chunk(39, 2, 2, 'EMERGENCY GENERATION', -28, -19, 13,
      [E('N', 14, 18, -28), E('W', 8, 12, -25)],
      """Diesel generator hall with fuel day tanks, piping and battery banks. Black start chain (terminals
with prerequisites): open fuel transfer -> prime -> crank -> sync = pwr_gen_online, which turns on the
district's switched lamps. A sealed west shutter onto the Machine Hall gallery (-25) is a later shortcut:
gate it inside this chunk on pwr_feeder9 (a door across a short north-south jog of the passage).""", manifest='23')
chunk(40, -1, 2, 'LOAD CONTROL', -19, -15.5, 12,
      [E('N', 10, 14, -19)],
      """Control room puzzle overlooking the Machine Hall through a sealed observation window (east wall):
desks of mimic panels and CRTs. Three-step load transfer with prerequisites: SHED NON-ESSENTIAL LOADS ->
TRANSFER TO GENERATION (requires pwr_sub_a) -> ENERGIZE FEEDER 9 (sets pwr_feeder9). A two-man cleanup team
holds the room (Guard actors). Set visibleResidencyGroups to the Machine Hall group so the hall shows.""", manifest='24')

district('MachineHall', 'MACHINE HALL AND AIR', 1, '25-27', 'Heat', """
The largest space so far: a 48 x 48 m cavernous turbine hall (four stitched chunks
that stay resident together), 34 m to the roof. Three dead turbine-generator sets,
an overhead crane runway, gallery catwalks at -25 and -22 and a crane cab. The
way on is up: the south end's catwalk leads into the Ventilation Works, then
the crawlspaces of Air Handling, the quietest and most frightening map so far
(no music cue change needed; sound and darkness do the work).""")
chunk(41, 0, 2, 'MACHINE HALL / NORTH GALLERY', -28, 6, 14,
      [E('N', 10, 14, -25, 'door'), E('E', 1, 23, -28), E('S', 1, 23, -28)],
      """North-west quarter: the entry door from the hub opens onto a -25 gallery with stairs down to the
floor. Turbine set 1 (giant casings from scaled fixtures, pedestal, exciter), the crane runway columns.""", manifest='25')
chunk(42, 1, 2, 'MACHINE HALL / EXCITER BAY', -28, 6, 14,
      [E('W', 1, 23, -28), E('S', 1, 23, -28), E('E', 8, 12, -25)],
      """North-east quarter: exciter bay, the -25 gallery that ends at Emergency Generation's sealed
shutter (east), switchgear for the sets. A fallen crane hook block.""", manifest='25')
chunk(43, 0, 3, 'MACHINE HALL / TURBINE FLOOR', -28, 6, 14,
      [E('N', 1, 23, -28), E('E', 1, 23, -28)],
      """South-west quarter: turbine set 3 opened for overhaul, its rotor on stands, a lay-down area,
oil purifier skid. Mutants among the parts.""", manifest='25')
chunk(44, 1, 3, 'MACHINE HALL / CRANE END', -28, 6, 14,
      [E('N', 1, 23, -28), E('W', 1, 23, -28), E('S', 14, 18, -22)],
      """South-east quarter: the crane cab and its access stair to the -22 catwalk that exits south into
the Ventilation Works. A brute (Scissor Fiend) roams the hall floor.""", manifest='25')
chunk(45, 1, 4, 'VENTILATION WORKS', -22, -12, 15,
      [E('N', 14, 18, -22), E('W', 4, 8, -22)],
      """Large air scrubbers, fan housings (extraction fans scaled up), ductwork as big as corridors and
maintenance shafts. Air handling units roar (machinery). The way on is through a duct access door west.""", manifest='26')
chunk(46, 0, 4, 'AIR HANDLING', -22, -19.4, 15,
      [E('E', 4, 8, -22), E('S', 4, 8, -22)],
      """Quiet atmospheric horror: crawlspaces and vents. Mostly crouch-height ducts (structures making
1.0 m clearances), plenum chambers, filter banks, dampers. A puppet scare (huntsman passing a grille).
Very dark, few lamps, flicker. Exits south into Water Treatment.""", ambient=.16, manifest='27')

district('Water', 'WATER AND COOLING', 1, '28-32', 'PitchBlack', """
The plant's water: treatment, drainage, sump pumps and the cooling tower. Rule
taught here: resonance pools where metal is wet and still; live water hums
before it bites. The cooling-basin crossing is electrified water (Anomaly hum
and an electrical hazard) until the Sump Works isolator is thrown (water_isolated).""")
chunk(47, 0, 5, 'WATER TREATMENT', -25, -15, 16,
      [E('N', 4, 8, -22), E('W', 14, 18, -25), E('E', 8, 12, -25)],
      """Water district hub: clarifier basins (water volumes), filtration vats, a pump gallery and a
chemical dosing room. Stairs down from the Air Handling exit at -22. Status board: COOLING BASIN / LIVE
WATER / ISOLATE AT SUMP WORKS. West to the drainage gallery, east to the cooling tower basin.""", manifest='28')
chunk(48, -1, 5, 'DRAINAGE GALLERY', -25, -20, 17,
      [E('E', 14, 18, -25), E('S', 8, 12, -25)],
      """Partially submerged lower maintenance corridor: long wade through knee-deep water basins between
walkway stubs, drain grates, sluice gates. Creatures in the water. South to the Sump Works.""", ambient=.2, manifest='29')
chunk(49, -1, 6, 'SUMP WORKS', -28, -16, 17,
      [E('N', 8, 12, -25)],
      """Deep pump pits with hazardous rotating drainage machinery (periodic crusher/pressure hazards paired
with geometry, compactor-style screw conveyor). The isolator pulpit sets water_isolated. Dead end, return.""", manifest='30')
chunk(50, 1, 5, 'COOLING TOWERS / BASIN', -28, 20, 18,
      [E('W', 8, 12, -25), E('S', 2, 22, -28)],
      """The base of a hollow cooling tower: a round-feeling basin (structures stepping the circle), fill
packs, the electrified water crossing on a walkway (hazard enabled until water_isolated). 48 m tall, echo.""", manifest='31')
chunk(51, 1, 6, 'COOLING TOWERS / SHELL', -28, 20, 18,
      [E('N', 2, 22, -28), E('E', 6, 10, -19)],
      """The tower shell: a spiral of stairs and catwalks climbing the inside wall to the -19 exit east,
drift eliminators overhead, daylight never; spray headers. Wasps.""", manifest='31')
chunk(52, 2, 6, 'HEAT EXCHANGE', -19, -12, 19,
      [E('W', 6, 10, -19), E('S', 10, 14, -19)],
      """Dense pipe arrays, narrow walkways between shell-and-tube exchangers, high-heat hazards (periodic
steam with emitters, fire). A burnt worker log. Exits south to the maintenance district.""", manifest='32')

district('Maintenance', 'MAINTENANCE DISTRICT', 1, '33-35', 'Undercover', """
A breathing valley and the first direct signs of workers: break rooms, lockers,
a first-aid outpost (preliminary medical), workshops. Survivors who hid here
since shift change. One is alive and can follow (Escort Worker). The Cleanup
Force patrols Central Stores (stealth practice, distraction, takedowns). The
way on is the Operations shutter, jammed: fabricate a shear pin in the
Fabrication Shop (lathe -> pin -> install = maint_shutter_pin).""")
chunk(53, 2, 7, 'MAINTENANCE DISTRICT', -19, -15, 20,
      [E('N', 10, 14, -19), E('W', 9, 14, -19), E('E', 9, 14, -19), E('S', 10, 14, -19)],
      """Hub street of the maintenance level: break room with vending machines and a coffee machine,
locker rooms, a shift board (who clocked in, who did not). Barricades the survivors built.""", manifest='33')
chunk(54, 1, 7, 'CENTRAL STORES', -19, -12, 21,
      [E('E', 9, 14, -19)],
      """Bulk logistics: tall shelving aisles, stock cages, abandoned supplies. A cleanup patrol (Guard
actors with patrol routes); stealth with distraction. Health/ammo caches.""", manifest='34')
chunk(55, 2, 8, 'FABRICATION SHOP', -19, -13, 22,
      [E('N', 10, 14, -19), E('S', 10, 14, -19, 'door', 'maint_shutter_pin')],
      """Lathes, CNC machines, tooling bays and repair lines. Puzzle: the Operations shutter (south door)
is jammed: terminals FABRICATE SHEAR PIN (lathe) -> INSTALL AT SHUTTER DRIVE sets maint_shutter_pin.""", manifest='35')
chunk(56, 3, 7, 'MAINTENANCE / AID STATION', -19, -15.5, 20,
      [E('W', 9, 14, -19)],
      """First-aid outpost: cots, a medicine cabinet, worker biological monitoring forms ("EXPOSURE TIER /
SCREENING DUE") that foreshadow Medical Screening. A survivor who can follow (Escort Worker). Health.""", manifest='33')

district('Perimeter', 'THE RESTRICTED PERIMETER', 1, '36-44', 'Darkness', """
The architecture of secrecy: industrial gives way to clean, restricted, fortified.
Ordinary employees were never meant past here. Security Perimeter and Checkpoint
Delta are the main stealth spaces (cameras implied by signage, patrols, a
gatehouse). Checkpoint Delta is a hub: the override is in the security chief's
office in the Administrative Link (delta_override). Then the staff transit, the
labs' life support, Research Receiving (pays off the freight manifests:
RESEARCH CONTAINERS HELD AT THE NEXT TRANSFER), the first cleanroom, and the
Energy Research Entrance: a monumental threshold and the chapter 1 finale (a
cleanup assault holding the great door; ere_great_door).""")
part('PerimeterA')
chunk(57, 2, 9, 'OPERATIONS CORRIDOR', -19, -15.5, 23,
      [E('N', 10, 14, -19, 'door'), E('S', 10, 14, -19)],
      """Shift toward cleaner architecture: painted block walls, signage in corporate blue, a restricted
perimeter approach with a turnstile line and a RESTRICTED / AUTHORIZED PERSONNEL ONLY gate.""", manifest='36')
chunk(58, 2, 10, 'SECURITY PERIMETER / YARD', -19, -10, 24,
      [E('N', 10, 14, -19), E('E', 4, 20, -19), E('S', 10, 14, -19, 'door', 'perim_gate')],
      """Fortified checkpoint yard: chain-link and concrete barriers, a vehicle lane, floodlights, a patrol
squad. The gate south requires perim_gate, thrown in the gatehouse.""", manifest='37')
chunk(59, 3, 10, 'SECURITY PERIMETER / GATEHOUSE', -19, -10, 24,
      [E('W', 4, 20, -19)],
      """The gatehouse: CCTV bank, gate controls (perim_gate), lockers, a sleeping-quarters annex. Guards.""", manifest='37')
chunk(60, 2, 11, 'CHECKPOINT DELTA', -19, -12, 25,
      [E('N', 10, 14, -19, 'door'), E('W', 9, 13, -16), E('S', 10, 14, -19, 'door', 'delta_override')],
      """Heavily sealed access control with evidence of violent containment: welded doors, bullet decals
(blood material), body bags. Hub: the south door needs delta_override; a stair to the -16 mezzanine west.""", manifest='38')
chunk(61, 1, 11, 'ADMINISTRATIVE LINK', -19, -10.5, 25,
      [E('E', 9, 13, -16)],
      """Upper-tier offices overlooking the industrial zones below (glass to a void/drop): cubicles, the
security chief's office with the override console (delta_override). Personnel memos about the lockdown.""", manifest='39')
chunk(62, 2, 12, 'PERSONNEL TRANSIT', -19, -13, 26,
      [E('N', 10, 14, -19, 'door'), E('W', 14, 19, -19)],
      """Staff corridors, elevators (dead) and a transit tram platform with a stopped metro car. The tram
tunnel is blocked; the platform walkway continues west.""", manifest='40')
part('PerimeterB')
chunk(63, 1, 12, 'RESTRICTED UTILITIES', -19, -12, 27,
      [E('E', 14, 19, -19), E('S', 8, 12, -19)],
      """Dedicated life-support for the labs: HEPA plant, chilled water, redundant pumps, gas cylinder
manifolds labelled with research gases. Clean, maintained, eerily intact.""", manifest='41')
chunk(64, 1, 13, 'RESEARCH RECEIVING', -19, -11, 27,
      [E('N', 8, 12, -19), E('S', 10, 14, -16)],
      """Freight/personnel sorting hub entering the lab complex: the research containers from the freight
manifests, held, some open and empty from the inside. Stairs up to -16 for the decontamination line.""", manifest='42')
chunk(65, 1, 14, 'DECONTAMINATION', -16, -12.5, 28,
      [E('N', 10, 14, -16), E('S', 10, 14, -16, 'door', 'decon_cycle')],
      """Factory gives way to cleanroom: white tile, air showers, suit racks, a decon cycle console
(decon_cycle) that opens the south door. Lighting changes to cold white.""", manifest='43')
chunk(66, 1, 15, 'ENERGY RESEARCH / FORECOURT', -16, 8, 29,
      [E('N', 10, 14, -16, 'door'), E('S', 2, 22, -16)],
      """Massive structural threshold, first half: a 24 m tall forecourt, corporate monument signage
(DEPTHWORKS ENERGY RESEARCH / POWER FROM BELOW), security barriers, a cleanup squad dug in.""", manifest='44')
chunk(67, 1, 16, 'ENERGY RESEARCH / GREAT DOOR', -16, 8, 29,
      [E('N', 2, 22, -16), E('S', 9, 15, -12, 'door', 'ere_great_door')],
      """The great door: a monumental stair up to a blast door at -12. Chapter 1 finale: the cleanup squad's
last stand, then the door console (ere_great_door).""", manifest='44')

# =============================================================================
# CHAPTER 2: LABS AND ENERGY RESEARCH (45-76), WITH DEPTHWORKS MEDICAL
# =============================================================================
district('ResearchFront', 'RESEARCH FRONT', 2, '45-47', 'Research', """
Clinical, paranoid, corporate. Welcome signage and training media (terminals with
"YOUR WORK POWERS THE WORLD", safety films) next to biometric checkpoints.
Research Administration holds LAB 4's personnel card (EXPOSURE TIER 2 /
SCREENING DUE) and the boardroom's minutes (production quotas vs exposure).""")
chunk(68, 1, 17, 'RESEARCH INTAKE', -12, -8.5, 30,
      [E('N', 9, 15, -12, 'door'), E('S', 10, 14, -12)],
      """Decontamination chambers and a controlled security entry: glass booths, a reception desk with a
welcome film terminal, a turnstile line. Clean white panels, lab lighting.""", manifest='45')
chunk(69, 1, 18, 'PERSONNEL PROCESSING', -12, -8.8, 30,
      [E('N', 10, 14, -12), E('W', 9, 14, -12), E('E', 9, 14, -12), E('S', 10, 14, -12, 'door', 'proc_clearance')],
      """Locker bays, hazmat suit stations and biometric checkpoints. Hub: west to Administration, east to
Medical Screening, south door to the labs needs proc_clearance (issued at the Administration clearance desk).""", manifest='46')
chunk(70, 0, 18, 'RESEARCH ADMINISTRATION', -12, -6, 31,
      [E('E', 9, 14, -12)],
      """Executive offices, a boardroom and observation galleries (a mezzanine). LAB 4's personnel card.
Clearance desk terminal sets proc_clearance.""", manifest='47')

district('Medical', 'DEPTHWORKS MEDICAL', 2, '48 (subchapter)', 'Clinic', """
The scariest part of the game. The clinic was never for injuries: it graded staff
by resonance exposure and moved high readers into observation. Two parts:
- The clinic front (Screening, Triage) is open early: LAB 4's line on the triage
  board: HELD FOR OBSERVATION / WARD C. The way into Ward C is sealed
  (med_override).
- The deep wing (Imaging, Pharmacy, Ward C, Surgery, Morgue) opens with the
  medical override disk found on the escort's body in Lab 4 (sets med_override;
  Lab 4's service door leads straight into Ward C). Ward C goes dark
  (med_blackout, set by a ScriptEvent when the player is deep in the ward):
  lamps on med_blackout invert switch off, a DarkZone jams the flashlight, and
  patients wake (Mutant spawns with wakeState med_blackout). The only light is
  the muzzle flash. Restoring the ward's power (a toggle terminal in Surgery)
  clears med_blackout. Her bed is empty, her band cut: SUBJECT LEFT WARD
  UNASSISTED / DOORS OPENED FROM INSIDE. The medical director's office in
  Surgery releases the morgue's quarantine door (med_morgue_release), the way on
  into Quarantine.
Use the imported med_ models (beds, monitors, carts, wheelchairs, x-ray boxes,
cabinets, autopsy table) and clinic tile surfaces. Flickering lamps,
dripping, breathing.""")
part('MedicalFront')
chunk(71, 2, 18, 'MEDICAL SCREENING', -12, -8.8, 32,
      [E('W', 9, 14, -12), E('E', 9, 14, -12), E('S', 10, 14, -12, 'door', 'med_override')],
      """Primary Medical Hub, front: waiting room, exposure screening booths, the triage board with LAB 4's
line, posters about routine monitoring. South door into the deep wing sealed (med_override).""", manifest='48')
chunk(72, 3, 18, 'MEDICAL / TRIAGE', -12, -8.8, 32,
      [E('W', 9, 14, -12)],
      """Triage bays with curtains, gurneys, a nurses' station, exposure tier wristbands. Signs of a hurried
evacuation. A patient who sits up when you turn away (scripted actor).""", manifest='48')
chunk(73, 2, 19, 'MEDICAL / IMAGING', -12, -9, 33,
      [E('N', 10, 14, -12, 'door'), E('E', 9, 14, -12), E('S', 10, 14, -12)],
      """X-ray and scanning rooms, lightboxes with films that are wrong (skulls with crystalline growth).""", manifest='48')
chunk(74, 3, 19, 'MEDICAL / PHARMACY', -12, -9, 33,
      [E('W', 9, 14, -12)],
      """Locked pharmacy cage, cold storage, dispensing records (sedation orders for whole shifts). Health.""", manifest='48')
part('MedicalWard')
chunk(75, 2, 20, 'MEDICAL / WARD C NORTH', -12, -9, 34,
      [E('N', 10, 14, -12), E('S', 2, 22, -12)],
      """Ward C, first half of a long stitched ward: rows of beds with restraints, cardiac monitors, IV
stands. Patients lie still (dormant Mutants, wakeState med_blackout).""", manifest='48')
chunk(76, 2, 21, 'MEDICAL / WARD C SOUTH', -12, -9, 34,
      [E('N', 2, 22, -12), E('W', 15, 19, -12), E('E', 9, 14, -12), E('S', 10, 14, -12)],
      """Ward C, second half: the blackout trigger (ScriptEvent sets med_blackout), LAB 4's empty bed with her
cut band. The service door west to Lab 4. East to Surgery, south to the Morgue.""", manifest='48')
chunk(77, 3, 21, 'MEDICAL / SURGERY', -12, -8.5, 35,
      [E('W', 9, 14, -12)],
      """Operating theatre with lamps, the medical director's office (power restore toggle for med_blackout,
and the morgue release med_morgue_release), surgical logs.""", manifest='48')
chunk(78, 2, 22, 'MEDICAL / MORGUE', -15, -8.5, 35,
      [E('N', 10, 14, -12), E('S', 10, 14, -15, 'door', 'med_morgue_release')],
      """Cold rooms down a stair: autopsy tables, body drawers, sheet-covered shapes; one drawer is open from
the inside. The quarantine door south needs med_morgue_release.""", manifest='48')

district('Analysis', 'ANALYSIS LABORATORIES', 2, '49-54', 'Research', """
Conventional science around an unconventional sample: intake docks for specimens
sent up from the depths, cryogenic sample storage, wet chemistry, instrument
calibration, field measurement. The path leads to Lab 4 inside Energy
Diagnostics: her workstation (the prologue's chat came from it), her unsent
drafts, and the escort's body with the medical override disk (med_override).
The chemistry lab's south door is sealed by a chemical lockdown (lab_chem_seal),
cleared by running the fume purge in Instrumentation.""")
chunk(79, 1, 19, 'MATERIALS INTAKE', -12, -6, 36,
      [E('N', 10, 14, -12, 'door'), E('W', 9, 14, -12), E('S', 10, 14, -12)],
      """Unloading dock for anomalous specimens sent from the depths: sealed crates on a dock, a gantry
hoist, manifests stamped DEEP EXTRACTION / HANDLE AS LIVE.""", manifest='49')
chunk(80, 0, 19, 'SAMPLE STORAGE', -15, -8, 36,
      [E('E', 9, 14, -12)],
      """Hermetically sealed lockers and deep cryogenic vaults down a stair: frost, cryo tanks, one vault
door bulged outward.""", manifest='50')
chunk(81, 1, 20, 'ANALYTICAL CHEMISTRY', -12, -8.5, 37,
      [E('N', 10, 14, -12), E('W', 9, 14, -12), E('S', 10, 14, -12, 'door', 'lab_chem_seal')],
      """Wet labs and chemical processing lines: fume hoods, benches, glassware. Chemical lockdown seals the
south door (lab_chem_seal). Toxic hazard pockets.""", manifest='51')
chunk(82, 0, 20, 'INSTRUMENTATION', -12, -8.5, 37,
      [E('E', 9, 14, -12), E('S', 6, 10, -12)],
      """Spectrometry and sensor calibration labs: racks of instruments, calibration benches. The fume purge
console (sets lab_chem_seal).""", manifest='52')
chunk(83, 1, 21, 'ENERGY DIAGNOSTICS / LAB 4', -12, -8.5, 38,
      [E('N', 10, 14, -12, 'door'), E('E', 15, 19, -12), E('W', 9, 14, -12), E('S', 10, 14, -12)],
      """Lab 4: monitoring arrays for exotic energy, and HER workstation. Story beat, quiet. The escort's body
holds the medical override disk (a terminal/pickup sets med_override). The service door east into Ward C
is gated inside this chunk on med_override. The south passage to the Resonance Laboratory is sealed from
the far side (gate inside this chunk on res_lab_unsealed).""", manifest='53')
chunk(84, 0, 21, 'FIELD MEASUREMENT', -12, -7, 38,
      [E('N', 6, 10, -12), E('E', 9, 14, -12)],
      """Shielded test chambers and localized sensor clusters; a chamber where the readings never stopped.
Anomaly fields (visible shimmer) to learn around.""", manifest='54')

district('EnergyTest', 'ENERGY TEST COMPLEX', 2, '55-60', 'Research', """
Where humans first bottled resonance. The signature space is the Observation
Ring: four stitched chunks around a test pit 16 m deep, the ring walkway at -12
looking down into the pit's apparatus. West: Shielding Corridor, Experimental
Power and the Conversion Lab, where the player re-routes conversion output to
containment (cont_power), needed by the containment hub. North: the Resonance
Laboratory (early anomalous experiments) and the High-Energy Test Bay. The
Resonance Lab's north passage unseals Lab 4's back door (res_lab_unsealed): a
shortcut loop.""")
part('EnergyTestLabs')
chunk(85, 1, 22, 'RESONANCE LABORATORY', -12, -6, 39,
      [E('N', 10, 14, -12), E('W', 9, 14, -12), E('S', 10, 14, -12)],
      """Specialized lab housing early anomalous experiments: a resonance chamber, a glass test cell, logs of
the first R-numbered specimens. Unseal console for the Lab 4 passage north (res_lab_unsealed).""", manifest='55')
chunk(86, 0, 22, 'HIGH-ENERGY TEST BAY', -20, 4, 39,
      [E('E', 9, 14, -12), E('S', 10, 14, -12)],
      """Large-scale experimental apparatus in a tall bay: a capacitor bank, a test rig, a gallery at -12.""", manifest='56')
part('ObservationRing')
chunk(87, 0, 23, 'OBSERVATION RING / NORTH-WEST', -28, 6, 40,
      [E('N', 10, 14, -12), E('E', 1, 23, -12), E('S', 1, 23, -12), E('W', 9, 14, -12)],
      """Ring walkway (deck at -12) around the test pit (floor -28) with observation windows; pit apparatus
below. Entry from the test bay (north) and the Shielding Corridor (west).""", manifest='58')
chunk(88, 1, 23, 'OBSERVATION RING / NORTH-EAST', -28, 6, 40,
      [E('N', 10, 14, -12), E('W', 1, 23, -12), E('S', 1, 23, -12)],
      """Ring walkway continues; entry from the Resonance Laboratory (north).""", manifest='58')
chunk(89, 0, 24, 'OBSERVATION RING / SOUTH-WEST', -28, 6, 40,
      [E('N', 1, 23, -12), E('E', 1, 23, -12)],
      """Ring walkway; a stair down into the pit for the brave (pickups, an anomaly core).""", manifest='58')
chunk(90, 1, 24, 'OBSERVATION RING / SOUTH-EAST', -28, 6, 40,
      [E('N', 1, 23, -12), E('W', 1, 23, -12), E('E', 9, 14, -12)],
      """Ring walkway; the east passage to the Containment Intake hub.""", manifest='58')
part('EnergyTestLabs')
chunk(91, -1, 23, 'SHIELDING CORRIDOR', -12, -8, 41,
      [E('E', 9, 14, -12), E('S', 10, 14, -12)],
      """Reinforced bulkheads designed to withstand massive bursts: thick doors, lead-lined panels, scorched
blast shields.""", manifest='57')
chunk(92, -1, 24, 'EXPERIMENTAL POWER', -16, -6, 41,
      [E('N', 10, 14, -12), E('S', 10, 14, -16)],
      """Direct routing of anomalous energy into facility power: bus bars glowing with resonance, insulators,
a cage around a coupling.""", manifest='59')
chunk(93, -1, 25, 'CONVERSION LAB', -16, -4, 42,
      [E('N', 10, 14, -16)],
      """Industrial-scale energy conversion apparatus: the converter hall. Routing console sets cont_power.""", manifest='60')

district('Containment', 'CONTAINMENT', 2, '61-70', 'DeathClose', """
Where specimens were held, and where it failed. Entered from the Morgue through
Quarantine. The Containment Intake hub's deep door needs containment power
(cont_power, from the Conversion Lab via the Observation Ring west). Containment
A is intact, B is breached; the Service corridors behind the walls connect them.
The Isolation Wing bunkers hold LAB 4's handwriting on the walls and terminals:
"follow the lamps." The Breach Sector is ground zero. Emergency Containment's
barricades and welded doors (cont_barricade) are the way out to deep research.
Cleanup squads are here, losing.""")
part('ContainmentA')
chunk(94, 2, 23, 'QUARANTINE WARD', -15, -11.5, 43,
      [E('N', 10, 14, -15, 'door'), E('E', 9, 14, -15), E('S', 10, 14, -15)],
      """Improvised medical holding and biological quarantine: plastic sheeting, cots, quarantine beds, a
cleanup checkpoint overrun.""", manifest='66')
chunk(95, 2, 24, 'CONTAINMENT INTAKE', -15, -8, 44,
      [E('N', 10, 14, -15), E('W', 9, 14, -12), E('E', 9, 14, -15), E('S', 10, 14, -15, 'door', 'cont_power')],
      """Hub of containment: transfer airlocks, a specimen intake console, the deep door (cont_power) south.
West deck at -12 to the Observation Ring.""", manifest='61')
chunk(96, 3, 24, 'CONTAINMENT A', -15, -9, 44,
      [E('W', 9, 14, -15), E('S', 10, 14, -15)],
      """Intact holding cells with observation glass; environmental logs; something still in one cell.""", manifest='62')
chunk(97, 3, 25, 'CONTAINMENT SERVICE', -15, -11.6, 45,
      [E('N', 10, 14, -15), E('W', 15, 19, -15), E('S', 10, 14, -15)],
      """Behind-the-walls service corridors and monitoring ducts: cramped, pipes, viewing slots into cells.""", manifest='63')
chunk(98, 2, 25, 'CONTAINMENT B', -15, -8, 45,
      [E('N', 10, 14, -15, 'door'), E('E', 15, 19, -15), E('W', 9, 14, -15), E('S', 10, 14, -15)],
      """Breached containment units, structural destruction, resonance growth on the walls.""", manifest='64')
part('ContainmentB')
chunk(99, 2, 26, 'SPECIMEN HANDLING', -18, -6, 46,
      [E('N', 10, 14, -15), E('E', 9, 14, -15)],
      """Overhead cranes, isolation cages and heavy transfer gear; a cage hanging open.""", manifest='65')
chunk(100, 3, 23, 'ISOLATION WING', -15, -11, 43,
      [E('W', 9, 14, -15)],
      """Ultra-dense concrete containment bunkers. LAB 4's handwriting: "follow the lamps." Story beat.""", manifest='67')
chunk(101, 3, 26, 'OBSERVATION THEATER', -18, -8, 46,
      [E('N', 10, 14, -15), E('W', 9, 14, -15)],
      """Tiered seating where observers studied live phenomena; a stage with a containment cylinder.""", manifest='68')
chunk(102, 1, 25, 'BREACH SECTOR', -18, -4, 47,
      [E('E', 9, 14, -15), E('S', 10, 14, -15)],
      """Ground zero of containment failure: catastrophic damage, collapsed decks, anomaly fields.""", manifest='69')
chunk(103, 1, 26, 'EMERGENCY CONTAINMENT', -15, -10, 47,
      [E('N', 10, 14, -15), E('S', 10, 14, -15, 'door', 'cont_barricade')],
      """Barricades, welded doors, desperate containment efforts; release the barricade (cont_barricade).""", manifest='70')

district('DeepResearch', 'DEEP RESEARCH', 2, '71-76', 'Research', """
Research turns down: core samples from extreme depth, seismic maps pointing at a
deep epicentre, the primary extraction/analysis machine. Shutting the core
machine's feed (core_shutdown) opens Deep Access Control. At the Extraction
Interface the clean lab architecture is physically swallowed by rock.""")
chunk(104, 1, 27, 'DEEP MATERIALS LAB', -15, -9, 48,
      [E('N', 10, 14, -15, 'door'), E('E', 9, 14, -15), E('S', 10, 14, -15)],
      """Core samples collected from extreme sub-surface drilling, cutting saws, sample trays.""", manifest='71')
chunk(105, 2, 27, 'BORE SAMPLE ARCHIVE', -15, -7, 48,
      [E('W', 9, 14, -15)],
      """Geological library: rows of shelving holding thousands of deep core boxes.""", manifest='72')
chunk(106, 1, 28, 'GEOPHYSICS', -15, -10, 49,
      [E('N', 10, 14, -15), E('S', 10, 14, -15)],
      """Seismic mapping: plotters, a wall-sized section showing a deep epicentre.""", manifest='73')
chunk(107, 1, 29, 'ENERGY CORE RESEARCH', -25, 5, 50,
      [E('N', 10, 14, -15), E('S', 10, 14, -15, 'door', 'core_shutdown')],
      """House of the primary extraction/analysis machine, 30 m tall; galleries at -15; core_shutdown.""", manifest='74')
chunk(108, 1, 30, 'DEEP ACCESS CONTROL', -15, -9, 51,
      [E('N', 10, 14, -15, 'door'), E('S', 9, 15, -15)],
      """High-clearance gate linking research staff to the mining lines.""", manifest='75')
chunk(109, 1, 31, 'EXTRACTION INTERFACE', -15, -5, 51,
      [E('N', 9, 15, -15), E('S', 8, 16, -15)],
      """Clean lab architecture physically swallowed by heavy rock: white panels cracked by stone.""", manifest='76')

# =============================================================================
# CHAPTER 3: MINING AND HEAVY EXTRACTION (77-98)
# =============================================================================
district('Minehead', 'MINEHEAD', 3, '77-79', 'Mines', """
Brutal industry. A staging depot, the heavy equipment depot, and the Personnel
Descent: a cage lift down the main shaft (24 m drop) to the upper mine.""")
chunk(110, 1, 32, 'MINE ACCESS', -15, 2, 52,
      [E('N', 8, 16, -15), E('W', 9, 15, -15), E('S', 9, 15, -4)],
      """Industrial staging depot between the lab sector and the minehead; stairs up to the -4 cage deck.""", manifest='77')
chunk(111, 0, 32, 'EQUIPMENT DEPOT', -15, -2, 52,
      [E('E', 9, 15, -15)],
      """Excavators, loaders and tunnel machinery parked in bays; a workshop pit.""", manifest='78')
chunk(112, 1, 33, 'PERSONNEL DESCENT', -28, 2, 53,
      [E('N', 9, 15, -4), E('S', 9, 15, -28)],
      """The main shaft's cage lift (CargoLift) from the -4 deck to -28. Multi-stage feel, cable and guides.""", manifest='79')

district('UpperMine', 'UPPER MINE', 3, '80-86', 'Mines', """
Blasted rock tunnels with timber and iron shoring, haulage rail, the crusher and
screening plant, the Conveyor Canyon (a two-chunk vertical chasm crossed by
beltways) and the ventilation shaft.""")
part('UpperMineA')
chunk(113, 1, 34, 'UPPER MINE', -28, -20, 54,
      [E('N', 9, 15, -28), E('W', 9, 15, -28), E('S', 9, 15, -28)],
      """Conventional blasted rock tunnels with timber and iron shoring; a mine office.""", manifest='80')
chunk(114, 0, 34, 'ORE HAULAGE', -28, -20, 54,
      [E('E', 9, 15, -28), E('W', 9, 15, -24), E('S', 8, 14, -28)],
      """Heavy rail lines and ore cars; an incline up west to the crusher.""", manifest='81')
chunk(115, -1, 34, 'CRUSHER STATION', -28, -10, 55,
      [E('E', 9, 15, -24), E('S', 8, 14, -24)],
      """Rock-crushing machinery over massive hoppers (periodic crusher hazards).""", manifest='82')
chunk(116, -1, 35, 'SCREENING PLANT', -28, -12, 55,
      [E('N', 8, 14, -24), E('E', 9, 15, -20)],
      """Vibratory screens and sorting lines; resonant ore glows in the reject chute.""", manifest='83')
part('UpperMineB')
chunk(117, 0, 35, 'CONVEYOR CANYON / UPPER', -28, 16, 56,
      [E('N', 8, 14, -28), E('W', 9, 15, -20), E('S', 1, 23, -28)],
      """A multi-level vertical chasm crossed by intersecting beltways at -20 and -14.""", manifest='84')
chunk(118, 0, 36, 'CONVEYOR CANYON / LOWER', -28, 16, 56,
      [E('N', 1, 23, -28), E('E', 9, 15, -28), E('S', 8, 14, -20)],
      """The canyon's lower half; the beltway exit south at -20.""", manifest='84')
chunk(119, 1, 35, 'VENTILATION SHAFT', -28, 16, 57,
      [E('N', 9, 15, -28), E('S', 9, 15, -28)],
      """A colossal vertical air shaft with a fan at its foot; wind roar.""", manifest='85')
chunk(120, 1, 36, 'MAINTENANCE DRIFT', -28, -24.5, 57,
      [E('N', 9, 15, -28), E('W', 9, 15, -28)],
      """Auxiliary tunnels, equipment repair shops and tool stations.""", manifest='86')

district('LowerMine', 'LOWER MINE', 3, '87-92', 'Mines', """
Higher heat, exposed bedrock, failing supports. Shaft Junction is the hub: the
deep bore door needs drill power (mine_drill_power) from Drill Control, reached
through the Drill Assembly. The Pump Station battles flooding.""")
chunk(121, 0, 37, 'LOWER MINE', -20, -14, 58,
      [E('N', 8, 14, -20), E('W', 9, 15, -20), E('S', 9, 15, -20)],
      """Exposed bedrock, failing structural supports, heat haze.""", manifest='87')
chunk(122, -1, 37, 'PUMP STATION DEEP', -24, -12, 58,
      [E('E', 9, 15, -20)],
      """High-pressure de-watering station battling flooding.""", manifest='88')
chunk(123, 0, 38, 'SHAFT JUNCTION', -20, -10, 59,
      [E('N', 9, 15, -20), E('E', 9, 15, -20), E('W', 9, 15, -20), E('S', 9, 15, -20, 'door', 'mine_drill_power')],
      """Hub connecting aborted excavation routes; the deep bore door (mine_drill_power).""", manifest='89')
chunk(124, 1, 38, 'BORING WORKS', -20, -6, 60,
      [E('W', 9, 15, -20)],
      """Assembly sector for heavy boring shields.""", manifest='90')
chunk(125, -1, 38, 'DRILL ASSEMBLY / GALLERY', -24, 8, 61,
      [E('E', 9, 15, -20), E('S', 1, 23, -24), E('W', 9, 15, -16)],
      """The monolithic drilling engine's gallery, first half.""", manifest='91')
chunk(126, -1, 39, 'DRILL ASSEMBLY / HEAD', -24, 8, 61,
      [E('N', 1, 23, -24)],
      """The drill head itself, cutters taller than a man.""", manifest='91')
chunk(127, -2, 38, 'DRILL CONTROL', -16, -11, 61,
      [E('E', 9, 15, -16)],
      """Enclosed control tower overlooking the bore site; mine_drill_power.""", manifest='92')

district('DeepBore', 'DEEP BORE', 3, '93-98', 'Mines', """
Down the primary drill's spiral casing to the face where resonant ore was torn
out, LAB 4's badge at the Abandoned Heading, the Breach Shaft where engineered
steel collapses into a natural chasm, and the Threshold: the last human-built
structure, looking out into the Dark Below.""")
chunk(128, 0, 39, 'DEEP BORE', -28, 20, 62,
      [E('N', 9, 15, -20, 'door'), E('S', 9, 15, -28)],
      """Extended descent down the spiral casing of the primary drill.""", manifest='93')
chunk(129, 0, 40, 'EXTRACTION FACE', -28, -16, 63,
      [E('N', 9, 15, -28), E('E', 9, 15, -28), E('S', 9, 15, -28)],
      """The rock face where anomalous ore was torn out; resonance veins.""", manifest='94')
chunk(130, 1, 40, 'SURVEY DRIFT', -28, -25, 64,
      [E('W', 9, 15, -28), E('S', 9, 15, -28)],
      """Unfinished exploratory tunnels; lamps left lit by someone (LAB 4).""", manifest='95')
chunk(131, 1, 41, 'ABANDONED HEADING', -28, -24, 64,
      [E('N', 9, 15, -28), E('W', 9, 15, -28)],
      """Dig site halted by order of management; LAB 4's badge.""", manifest='96')
chunk(132, 0, 41, 'BREACH SHAFT', -28, 10, 65,
      [E('N', 9, 15, -28), E('E', 9, 15, -28), E('S', 8, 16, -10)],
      """Engineered steel tunnel collapses into a natural chasm; climb the wreck.""", manifest='97')
chunk(133, 0, 42, 'THRESHOLD', -10, 20, 66,
      [E('N', 8, 16, -10), E('S', 8, 16, -10)],
      """The final human-built structure, a platform looking out into the Dark Below.""", manifest='98')

# =============================================================================
# CHAPTER 4: THE DARK BELOW (99-136)
# =============================================================================
district('Wilds', 'THE WILD CAVERNS', 4, '99-106', 'Depths', """
Natural caverns beyond human supports: rock surfaces, rock fixtures as walls,
pillars and terraces, underground lakes, colossal stalactites, native fauna that
live here (creatures as inhabitants). Light is scarce: glowing fungus and
crystals, the player's flashlight.""")
part('WildsA')
chunk(134, 0, 43, 'THE BREACH', -16, 12, 67, [E('N', 8, 16, -10), E('S', 6, 18, -16)], """First natural cavern beyond human supports.""", manifest='99')
chunk(135, 0, 44, 'DESCENT', -28, 12, 67, [E('N', 6, 18, -16), E('S', 6, 18, -28)], """Scaffolding and lighting rigs fade away down a rock slope of ledges.""", manifest='100')
chunk(136, 0, 45, 'BLACKWATER / SHORE', -28, 4, 68, [E('N', 6, 18, -28), E('E', 4, 20, -28)], """Underground lake shore, stagnant pools.""", manifest='101')
chunk(137, 1, 45, 'BLACKWATER / LAKE', -28, 4, 68, [E('W', 4, 20, -28), E('S', 6, 18, -24)], """Wet shelf climbs over black water.""", manifest='101')
chunk(138, 1, 46, 'BROKEN SHELF / UPPER', -28, 16, 69, [E('N', 6, 18, -24), E('S', 2, 22, -28)], """Vast rock terraces spanning an abyss.""", manifest='102')
chunk(139, 1, 47, 'BROKEN SHELF / LOWER', -28, 16, 69, [E('N', 2, 22, -28), E('W', 8, 16, -20)], """Terraces continue; a ledge west.""", manifest='102')
part('WildsB')
chunk(140, 0, 47, 'HANGING CAVERNS', -28, 20, 70, [E('E', 8, 16, -20), E('W', 8, 16, -28)], """Massive vertical chamber with colossal stalactite arrays.""", manifest='103')
chunk(141, -1, 47, 'NESTING GROUNDS', -28, -14, 71, [E('E', 8, 16, -28), E('S', 8, 16, -28)], """Cavern ecosystem of aggressive native fauna.""", manifest='104')
chunk(142, -1, 48, 'BONE PASSAGE', -28, -20, 71, [E('N', 8, 16, -28), E('S', 8, 16, -28)], """Ancient predatory dumping grounds; early signs of habitation.""", manifest='105')
chunk(143, -1, 49, 'SINK', -28, 16, 72, [E('N', 8, 16, -28), E('E', 8, 16, -20)], """A deep, near-perfect circular void descending inward.""", manifest='106')

district('Scrap', 'THE SCRAP FIELDS', 4, '107-111', 'Depths', """
Where the plant's discarded scrap came down. Waste Handling and Salvage
Dispatch sent it over the chasm for years. Iron Fall, plains of crushed
machinery, wrecked train cars and elevator cages in the rock, the Tangle of
girders, and the Scrap Gate: the first conscious construction. The gate's
keepers (aware transformed workers) open it (haven_gate) if the player does not
shoot them.""")
chunk(144, 0, 49, 'IRON FALL', -24, 20, 73, [E('W', 8, 16, -20), E('S', 6, 18, -24)], """A waterfall of discarded facility scrap spilling into the caves.""", manifest='107')
chunk(145, 0, 50, 'SCRAP FIELDS', -24, 8, 73, [E('N', 6, 18, -24), E('E', 8, 16, -24), E('S', 6, 18, -24)], """Plains of crushed machinery and debris from above.""", manifest='108')
chunk(146, 1, 50, 'WRECK HOLLOW', -24, -8, 74, [E('W', 8, 16, -24), E('S', 6, 18, -24)], """Collapsed train cars and elevator cages embedded in rock (Train 27?).""", manifest='109')
chunk(147, 1, 51, 'THE TANGLE', -24, -6, 74, [E('N', 6, 18, -24), E('W', 8, 16, -20)], """Dense, disorienting labyrinth of twisted girders.""", manifest='110')
chunk(148, 0, 51, 'SCRAP GATE', -24, 4, 75, [E('N', 6, 18, -24), E('E', 8, 16, -20), E('S', 9, 15, -20, 'door', 'haven_gate')], """The first conscious construction; the gate keepers.""", manifest='111')

district('Haven', 'THE SCRAP CITY', 4, '112-122', 'Haven', """
A home. Transformed workers who kept their minds built a city from what the plant
threw away: corrugated shanties, lamp-lit lanes, a market, smelters, rope
bridges, towers up the cavern walls. They are mostly peaceful (Civilian/Scripted
Worker actors with zombie-like motion where available, Neutral faction); their
terminals remember what the player does. LAB 4 talks through their terminals.
The Cleanup Force raids the city (Guard squads) and the player can defend it.
Inner City holds the Machine Shrine (a silhouette of scavenged computers and
drives, an altar of the facility's memory). The Pit District circles the
opening where the first great entity lives (haven_pit opens the way).""")
part('HavenA')
chunk(149, 0, 52, 'OUTER SHANTIES / GATE ROW', -20, 6, 76, [E('N', 9, 15, -20, 'door'), E('W', 4, 20, -20), E('S', 6, 18, -20)], """Ramshackle shanties of corrugated paneling inside the gate.""", manifest='112')
chunk(150, -1, 52, 'OUTER SHANTIES / LAMP STREET', -20, 6, 76, [E('E', 4, 20, -20), E('S', 6, 18, -20)], """A lane of lanterns and doorways; residents.""", manifest='112')
chunk(151, 0, 53, 'LOWER MARKET', -20, -12, 77, [E('N', 6, 18, -20), E('W', 8, 16, -20), E('E', 8, 16, -20), E('S', 8, 16, -20)], """Tight, claustrophobic corridors of stalls.""", manifest='113')
chunk(152, -1, 53, 'FURNACE ROW', -20, -6, 77, [E('N', 6, 18, -20), E('E', 8, 16, -20), E('S', 8, 16, -20)], """Searing heat, jury-rigged generators, open smelters.""", manifest='114')
chunk(153, 1, 53, 'CABLE DISTRICT', -26, 10, 78, [E('W', 8, 16, -20), E('S', 6, 18, -20)], """Suspended walkways and rope bridges on high-tension wire.""", manifest='115')
chunk(154, 1, 54, 'FLOODED QUARTER', -26, -10, 78, [E('N', 6, 18, -20), E('W', 8, 16, -20)], """Lower district partly under black water.""", manifest='116')
chunk(155, -1, 54, 'SCRAP TOWERS', -20, 20, 79, [E('N', 8, 16, -20), E('E', 8, 16, -20), E('S', 8, 16, -20)], """Multistory shanties climbing the cavern walls.""", manifest='117')
part('HavenB')
chunk(156, -1, 55, 'FOUNDRY BELOW', -20, -4, 79, [E('N', 8, 16, -20), E('E', 8, 16, -20)], """Crude metalworking repurposing facility alloys.""", manifest='118')
chunk(157, 0, 54, 'CENTRAL SPAN / NORTH', -28, 16, 80, [E('N', 8, 16, -20), E('E', 8, 16, -20), E('W', 8, 16, -20), E('S', 8, 16, -20)], """A massive rusted truss bridge over a chasm, north half.""", manifest='119')
chunk(158, 0, 55, 'CENTRAL SPAN / SOUTH', -28, 16, 80, [E('N', 8, 16, -20), E('W', 8, 16, -20), E('S', 8, 16, -20)], """The span's south half.""", manifest='119')
chunk(159, 0, 56, 'INNER CITY', -20, 4, 81, [E('N', 8, 16, -20), E('E', 8, 16, -20), E('S', 9, 15, -20, 'door', 'haven_pit')], """The densest concentration of deep structures.""", manifest='120')
chunk(160, 1, 56, 'MACHINE SHRINE', -20, 12, 81, [E('W', 8, 16, -20)], """A monumental silhouette of scavenged computers and drives.""", manifest='121')
chunk(161, 0, 57, 'PIT DISTRICT', -26, 12, 82, [E('N', 9, 15, -20, 'door'), E('S', 8, 16, -20)], """Industrial shanties circling an enormous descending opening.""", manifest='122')

district('Finale', 'THE DEEP', 4, '123-136', 'Depths', """
The three great encounters and the end. Boss Arena I (a giant of the pit), the
quiet Aftermath, raw cavern ecology, crystal galleries, the Scar, the Hollow
Forest of calcified columns, human machinery impossibly deep, the Maw's funnel,
Boss Arena II, non-Euclidean rock, the pre-human Old City, the Deep Gate, the
Cleanup Force's last assault led by the Limitator, and the Dark Heart, where
LAB 4 goes on alone and sends the player up (CompleteCampaign).""")
part('FinaleA')
chunk(162, 0, 58, 'BOSS ARENA I / RIM', -28, 16, 83, [E('N', 8, 16, -20), E('S', 1, 23, -28)], """The rim of the pit arena.""", music='Boss', manifest='123')
chunk(163, 0, 59, 'BOSS ARENA I / PIT', -28, 16, 83, [E('N', 1, 23, -28), E('S', 9, 15, -28, 'door', 'boss1_dead')], """The pit floor: the first great entity.""", music='Boss', manifest='123')
chunk(164, 0, 60, 'AFTERMATH', -28, -8, 84, [E('N', 9, 15, -28, 'door'), E('S', 8, 16, -28)], """Eerie, quiet traversal after the first boss.""", manifest='124')
chunk(165, 0, 61, 'DEEP WILDS', -28, 4, 85, [E('N', 8, 16, -28), E('E', 8, 16, -28), E('S', 8, 16, -28)], """Raw cavern ecology resumes.""", manifest='125')
chunk(166, 1, 61, 'CRYSTAL GALLERIES', -28, -6, 86, [E('W', 8, 16, -28), E('S', 8, 16, -28)], """Non-terrestrial rock formations and strange mineral veins.""", manifest='126')
chunk(167, 1, 62, 'THE SCAR', -28, 20, 86, [E('N', 8, 16, -28), E('W', 8, 16, -24)], """An enormous fault line splitting the cavern floor.""", manifest='127')
part('FinaleB')
chunk(168, 0, 62, 'HOLLOW FOREST', -28, 12, 87, [E('N', 8, 16, -28), E('E', 8, 16, -24), E('S', 8, 16, -28)], """A field of towering calcified columns like petrified trees.""", manifest='128')
chunk(169, 0, 63, 'BURIED MACHINERY', -28, -4, 88, [E('N', 8, 16, -28), E('S', 8, 16, -10)], """Human relics impossibly deep; climb out through a buried structure.""", manifest='129')
chunk(170, 0, 64, 'THE MAW', -28, 24, 89, [E('N', 8, 16, -10), E('S', 6, 18, -28)], """A massive funnel-shaped descent.""", manifest='130')
chunk(171, 0, 65, 'BOSS ARENA II / FUNNEL', -28, 20, 90, [E('N', 6, 18, -28), E('S', 1, 23, -28)], """The funnel's throat.""", music='Boss', manifest='131')
chunk(172, 0, 66, 'BOSS ARENA II / FLOOR', -28, 20, 90, [E('N', 1, 23, -28), E('S', 9, 15, -28, 'door', 'boss2_dead')], """The second great entity.""", music='Boss', manifest='131')
chunk(173, 0, 67, 'LOWER DEPTHS', -28, 8, 91, [E('N', 9, 15, -28, 'door'), E('S', 8, 16, -24)], """Increasingly alien, non-Euclidean rock geometry.""", manifest='132')
part('FinaleC')
chunk(174, 0, 68, 'OLD CITY / CAUSEWAY', -28, 16, 92, [E('N', 8, 16, -24), E('E', 8, 16, -24), E('S', 9, 15, -24, 'door', 'oldcity_seal')], """Pre-human architecture predating the scrap settlements.""", manifest='133')
chunk(175, 1, 68, 'OLD CITY / TEMPLE', -28, 16, 92, [E('W', 8, 16, -24)], """The temple holding the Old City's seal (oldcity_seal).""", manifest='133')
chunk(176, 0, 69, 'THE DEEP GATE', -28, 24, 93, [E('N', 9, 15, -24, 'door'), E('S', 6, 18, -24)], """A monumental threshold sealing the final zone.""", manifest='134')
chunk(177, 0, 70, 'DEEP GATE / APPROACH', -28, 12, 94, [E('N', 6, 18, -24), E('S', 9, 15, -24, 'door', 'boss3_dead')], """The Cleanup Force's last assault, led by the Limitator.""", music='Boss', manifest='135')
chunk(178, 0, 71, 'THE DARK HEART', -28, 24, 95, [E('N', 9, 15, -24, 'door')], """Campaign climax: LAB 4 goes on alone; the player is sent up.""", music='LastChapter', manifest='136')

# =============================================================================
CHUNKS = [c for d in DISTRICTS for c in d['chunks']]
BY_IDX = {c['idx']: c for c in CHUNKS}
OPP = {'N': 'S', 'S': 'N', 'E': 'W', 'W': 'E'}
STEP = {'N': (0, -1), 'S': (0, 1), 'E': (1, 0), 'W': (-1, 0)}

def check():
    errs = []
    idxs = [c['idx'] for c in CHUNKS]
    if idxs != list(range(FIRST, FIRST + len(CHUNKS))): errs.append('chunk indices must run %d..%d in order' % (FIRST, FIRST + len(CHUNKS) - 1))
    cells = {}
    for c in CHUNKS:
        k = (c['gx'], c['gy'])
        if k in cells: errs.append('%d and %d share grid cell %s' % (cells[k], c['idx'], k))
        cells[k] = c['idx']
        for v in (c['base'], c['ceil']):
            if not -30 <= v <= 28: errs.append('%d: height %g outside [-30,28]' % (c['idx'], v))
        if c['ceil'] <= c['base'] + 2.4: errs.append('%d: ceiling too low' % c['idx'])
    # Deep Junction's blast door is the one edge into existing content.
    for c in CHUNKS:
        for e in c['edges']:
            if not (0 <= e['a'] < e['b'] <= 24): errs.append('%d: bad edge range %s' % (c['idx'], e))
            if e['z'] < c['base'] - .01 or e['z'] > c['ceil'] - 2.4: errs.append('%d: edge z %g outside chunk' % (c['idx'], e['z']))
            if e['kind'] == 'door' and e['side'] not in 'NS': errs.append('%d: doors only on N/S edges' % c['idx'])
            dx, dy = STEP[e['side']]
            nb = cells.get((c['gx'] + dx, c['gy'] + dy))
            if c['idx'] == FIRST and e['side'] == 'N':
                continue
            if nb is None: errs.append('%d: %s edge leads nowhere' % (c['idx'], e['side'])); continue
            m = [f for f in BY_IDX[nb]['edges'] if f['side'] == OPP[e['side']] and abs(f['a'] - e['a']) < .01 and abs(f['b'] - e['b']) < .01 and abs(f['z'] - e['z']) < .01 and f['kind'] == e['kind']]
            if not m: errs.append('%d %s edge %g-%g z%g %s has no match in %d' % (c['idx'], e['side'], e['a'], e['b'], e['z'], e['kind'], nb))
            elif e['kind'] == 'door' and e['side'] == 'N' and (m[0]['state'] or None) != (e['state'] or None) and e['state']:
                errs.append('%d: entry door state should be on the transfer side' % c['idx'])
    # existing content occupies y < 696
    return errs

def cue(name):
    if name in NEW_CUES and name not in WIRED_CUES: return NEW_CUES[name]
    return name

def spawn(c):
    # Start just inside the first edge, facing into the chunk.
    e = c['edges'][0]; mid = (e['a'] + e['b']) / 2
    if e['side'] == 'N': p, yaw = (mid, 2.2), 1.5708
    elif e['side'] == 'S': p, yaw = (mid, 21.8), -1.5708
    elif e['side'] == 'W': p, yaw = (2.2, mid), 0.0
    else: p, yaw = (21.8, mid), 3.1416
    return p, e['z'], yaw

def f(v):
    s = ('%.3f' % v).rstrip('0').rstrip('.')
    return s + ('.f' if '.' not in s else 'f') if s not in ('0',) else '0'

def gen_definitions():
    rows, names = [], []
    for c in CHUNKS:
        (px, py), z, yaw = spawn(c)
        ox, oy = OX + 24 * c['gx'], OY + 24 * c['gy']
        rows.append('    {{%d,%d},{%s,%s},%s,Environment::Interior,false,%s,%s,%d,FloorMaterial::Industrial,MusicCue::%s,%s},' % (
            ox, oy, f(px), f(py), f(z), f(c['ceil']), f(c['ambient']), c['group'], cue(c['music']), f(yaw)))
        names.append('"%s"' % c['name'])
    return rows, names

def write_definitions():
    p = os.path.join(ROOT, 'src', 'world', 'WorldDefinition.h')
    s = open(p, newline='').read(); nl = '\r\n' if '\r\n' in s else '\n'
    rows, names = gen_definitions()
    count = FIRST + len(CHUNKS)
    s = re.sub(r'inline constexpr int CampaignChunkCount=\d+;', 'inline constexpr int CampaignChunkCount=%d;' % count, s)
    s = re.sub(r'inline constexpr int WorldChunkCapacity=\d+;', 'inline constexpr int WorldChunkCapacity=%d;' % count, s)
    def block(tag, body):
        nonlocal s
        a, b = '    // PLAN:%s BEGIN (tools/campaign/plan.py)' % tag, '    // PLAN:%s END' % tag
        if a not in s: raise SystemExit('missing marker ' + a)
        i, j = s.index(a) + len(a), s.index(b)
        s = s[:i] + nl + nl.join(body) + nl + s[j:]
    block('CHUNKS', rows)
    block('NAMES', [' ' + ','.join(names[i:i + 6]) + ',' for i in range(0, len(names), 6)])
    open(p, 'w', newline='').write(s)

def builder_name(c):
    return 'make' + ''.join(w.capitalize() for w in re.sub(r'[^A-Za-z0-9 ]', ' ', c['name']).split())

def edge_cpp(e):
    st = 'stateId("%s")' % e['state'] if e['state'] else '0'
    return "{'%s',%s,%s,%s,%s,%s}" % (e['side'], f(e['a']), f(e['b']), f(e['z']), 'true' if e['kind'] == 'door' else 'false', st)

def files():
    order, by = [], {}
    for d in DISTRICTS:
        for c in d['chunks']:
            if c['file'] not in by: order.append(c['file']); by[c['file']] = dict(district=d, chunks=[])
            by[c['file']]['chunks'].append(c)
    return [(k, by[k]['district'], by[k]['chunks']) for k in order]

def write_chapter_stubs():
    for key, d, chunks in files():
        p = os.path.join(ROOT, 'src', 'world', 'CampaignChapter%s.cpp' % key)
        if os.path.exists(p): continue
        out = ['// %s (chapter %d, manifest %s): chunks %s. Placeholder builders generated' % (d['title'], d['chapter'], d['manifest'], ', '.join(str(c['idx']) for c in chunks)),
               '// by tools/campaign/plan.py: replace each with the authored chunk. See', '// docs/Design/descent-campaign-20261025.md for the brief.',
               '#include "CampaignChapters.h"', '#include "CampaignKit.h"', 'namespace retro {', 'namespace {', 'using namespace kit;']
        for c in chunks:
            out.append('// %s (manifest %s)' % (c['name'], c['manifest']))
            out.append('std::shared_ptr<const Map> %s(){return placeholderChunk(%d,{%s});}' % (builder_name(c), c['idx'], ','.join(edge_cpp(e) for e in c['edges'])))
        out.append('}')
        out.append('void buildChapter%s(CampaignMapTable& maps){' % key)
        for c in chunks:
            out.append(' maps[%d]=%s();' % (c['idx'], builder_name(c)))
        out.append('}')
        out.append('}')
        open(p, 'w', newline='\n').write('\n'.join(out) + '\n')

def write_cmake():
    p = os.path.join(ROOT, 'src', 'CMakeLists.txt')
    s = open(p, newline='').read(); nl = '\r\n' if '\r\n' in s else '\n'
    lines = s.split(nl)
    lines = [l for l in lines if not re.match(r'\s*world/CampaignChapter\w*\.cpp$', l) or l.strip() == 'world/CampaignChapters.cpp']
    i = lines.index('    world/CampaignChapters.cpp')
    lines[i + 1:i + 1] = ['    world/CampaignChapter%s.cpp' % k for k, d, c in files()]
    open(p, 'w', newline='').write(nl.join(lines))

def write_registry():
    h = os.path.join(ROOT, 'src', 'world', 'CampaignChapters.h')
    out = ['#pragma once', '// Chapter files for campaign chunks 34 onward (tools/campaign/plan.py).', '#include "World.h"', '#include <array>', '#include <memory>',
           'namespace retro {', 'using CampaignMapTable=std::array<std::shared_ptr<const AuthoredMapData>,CampaignChunkCount>;']
    for k, _d, _c in files(): out.append('void buildChapter%s(CampaignMapTable& maps);' % k)
    out += ['// Every chapter, in plan order.', 'void buildCampaignChapters(CampaignMapTable& maps);', '}']
    open(h, 'w', newline='\n').write('\n'.join(out) + '\n')
    c = os.path.join(ROOT, 'src', 'world', 'CampaignChapters.cpp')
    out = ['#include "CampaignChapters.h"', 'namespace retro {', 'void buildCampaignChapters(CampaignMapTable& maps){']
    for k, _d, _c in files(): out.append(' buildChapter%s(maps);' % k)
    out += ['}', '}']
    open(c, 'w', newline='\n').write('\n'.join(out) + '\n')

def write_doc():
    p = os.path.join(ROOT, 'docs', 'Design', 'descent-campaign-20261025.md')
    s = open(p, encoding='utf-8').read()
    mark = '<!-- PLAN TABLES (generated by tools/campaign/plan.py) -->'
    if mark in s: s = s[:s.index(mark)]
    out = [mark, '', '## Districts', '']
    for d in DISTRICTS:
        idx = [c['idx'] for c in d['chunks']]
        out += ['### %s (chunks %d-%d, manifest %s)' % (d['title'].title(), idx[0], idx[-1], d['manifest']), '', d['brief'].strip(), '',
                '| Chunk | File | Grid | Name | Floor | Ceiling | Edges |', '| --- | --- | --- | --- | --- | --- | --- |']
        for c in d['chunks']:
            es = '; '.join('%s %g-%g @%g%s%s' % (e['side'], e['a'], e['b'], e['z'], ' door' if e['kind'] == 'door' else '', ' [%s]' % e['state'] if e['state'] else '') for e in c['edges'])
            out.append('| %d | %s | %d,%d | %s | %g | %g | %s |' % (c['idx'], c['file'], c['gx'], c['gy'], c['name'], c['base'], c['ceil'], es))
        out.append('')
    open(p, 'w', encoding='utf-8', newline='\n').write(s.rstrip() + '\n\n' + '\n'.join(out))

if __name__ == '__main__':
    errs = check()
    for e in errs: print('PLAN:', e)
    if errs: sys.exit(1)
    print('plan ok: %d chunks (%d-%d), %d districts' % (len(CHUNKS), FIRST, FIRST + len(CHUNKS) - 1, len(DISTRICTS)))
    if '--check' in sys.argv: sys.exit(0)
    write_definitions(); write_registry(); write_chapter_stubs(); write_cmake(); write_doc()
    print('written')
