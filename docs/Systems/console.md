---
tags: [systems, console, testing]
aliases: [Console, Developer console]
---

# Developer console

Open it with the backtick key (`` ` ``). Esc closes it. Commands are not case-sensitive. `help` lists the general commands and `help summon` lists the summon commands. The code is in `src/game/Console.cpp`, and the summon commands are in `src/game/Summon.cpp`.

## General

| Command | What it does |
|---|---|
| `maps`, `help` | List the maps and commands. |
| `map <id or name>`, `reload` | Load a campaign map, or reload the current one. `map wasteland`, `map exclusion` and `map coast` load the Ashfall worlds. |
| `where` | Print the map, position and height. |
| `god` | No damage. |
| `dev` | Developer mode: no damage, no creatures, every door open. |
| `noclip [on/off]` | Fly through walls. This is not saved. |
| `unlock` | Unlock the door or terminal you are looking at. |
| `give flashlight`, `give handgun`, `give 9mm`, `give 12 ga` | Add items or ammo. |
| `fps`, `r_scale`, `tex_up`, `r_post`, `r_lens`, `r_taa`, `r_pixel`, `r_texels`, `thirdperson` | Display and render switches. |
| `labsnap NAME` | Save this scene as a lab fixture (`lab/fixtures/NAME.lab` plus its save). |
| `clear` | Clear the console. |

## Summons

```
summon <kind> [count] [ally|enemy|neutral|faction=NAME] [ai=ORDER] [hp=N] [scale=N] [weapon=rifle|handgun|none] [name=X]
```

`spawn` works the same as `summon`. You can give `count` from 1 to 16.

### Kinds

| Kind | Notes |
|---|---|
| `huntsman`, `wasp`, `brute` (also `fiend`), `warden` (also `stalker`), `mutant`, `snatcher`, `chimera`, `transformed`, `machine` (also `equipment`) | Every creature kind. The cursed equipment comes up awake. |
| `hunter` (also `r117`) | R-117's kind: a Chimera that is a recurring hunter. It cannot be finished. When badly hurt it runs, and once out of sight it is gone. Its default order is `hunt`. |
| `guard`, `soldier`, `engineer`, `sharpshooter`, `heavy`, `medic`, `rusher` | Cleanup men on the guard AI, with the matching squad role. Summoned men on the same side form one squad, so they cover and flank together. Engineers and medics carry handguns. Their default order is `guard`. |
| `civilian` (also `worker`), `scientist`, `doctor`, `labtech` | Civilians on the civilian AI. They are neutral by default. |

### Sides

The default side is `enemy`, except for civilians, who are `neutral`. Allegiance goes through the existing faction rule, `factionsHostile` in `src/world/ScriptDefinition.h`, which creatures, guards, civilians, drones and grenades all use.

| Side | Faction | Fights |
|---|---|---|
| `ally` | Facility | Creatures and raiders. It never targets the player and doesn't count toward "clear the room" gates. |
| `enemy` (creature) | Creature | The player, allies, guards of every side, and allied creatures. |
| `enemy` (man) | Raider | The player, allies, Facility guards, and creatures. |
| `neutral` | Neutral | Nobody. Summoned neutral creatures and civilians run from anything that fights all comers, and from whatever hurts them. Authored neutral civilians keep their old calm. |
| `faction=player`, `facility`, `creature`, `raider`, `wary` | That faction | Whatever that faction fights. |

This covers three kinds of fight:
- Creatures and guards attack each other.
- Allied creatures fight hostile creatures.
- Hostile creatures fight allied creatures.

### Orders (`ai=`)

| Order | Creature | Man |
|---|---|---|
| `idle` | Stands and reacts to what it senses. | Stands. |
| `patrol` | Walks to a clear point up to 7 m ahead and back. | A two-post beat between the same two points. |
| `wander` | Roams within 5 m of where it was summoned. | Goes to a new post within 5 m every 7 s. |
| `guard` | Holds its spot. It ignores anything more than 8 m from it. | Stands at his post. |
| `hunt` | Goes after the nearest thing its side is at war with. | Same. |
| `follow` | Keeps about 1.6 m behind the player. | Same. A civilian uses his own follow. |

When a summon meets something hostile, its own AI takes over the fight. The order resumes when the fight is over.

### Options

| Option | Effect |
|---|---|
| `hp=N` | Health. For a man, it is his track health. |
| `scale=N` | Size, from 0.25 to 4. It scales the body, hull and reach. |
| `weapon=rifle`, `handgun` or `none` | A man's weapon. Creatures carry none. |
| `name=X` | The name shown in `list`. |

### Placement

Summons appear in front of you, on the nearest clear floor you can see. The floor must be within 1.5 m of your height, and the hull must fit using the game's own `hullFits` (walls, props and other creatures). Each summon in a group gets its own spot. If no spot fits, the console says so.

## Commands on what you aim at

The crosshair's shot trace picks the target. If the trace misses, the nearest living body within about 10° of your view counts.

| Command | Effect |
|---|---|
| `ally`, `enemy`, `neutral`, `faction <name>` | Turn it to that side. On an authored man, this overrides his squad's stance for this session only. |
| `kill` | Kill it. An authored recurring hunter (R-117) leaves instead, as its story expects. |
| `freeze` | Toggle holding it still. `freeze all` holds every summon, and `freeze off` (or `unfreeze off`) releases everything. |

## Other commands

| Command | Effect |
|---|---|
| `killall` | Kills every creature and every summoned man. |
| `killall <kind>` | Kills only that kind, authored ones included. |
| `killall summons` | Kills only the summons. |
| `ai off`, `ai on` | Stop or restart thinking and moving for every creature and man. |
| `list` | Every summon with its number, name, side, order, health and position, plus a count of everything else here and how many are hostile to you. |

## Summons are transient

Summons last only for the session, on the map where you summoned them.
- **Saves:** A save leaves them out completely (`Game::stripSummons`, called from `encodeSave`). That covers the bodies, every state their AI wrote, and any reference an authored guard holds to one, such as his target or attacker.
- **Load and map change:** After a load or a map change, none of them exist.
- **Session-only settings:** `ally`/`enemy` on an authored man, `freeze` and `ai off` are session settings too and are never saved.
- **Why:** This keeps the save format unchanged.

## Testing

- `python tools/dev/lab.py run --feature=engine.summon` runs the console end to end. This covers parsing, placement, every side, a fight each way, the commands on the aimed target, freeze, `ai off`, `killall`, and a save that carries no summons. It writes `summon-test.txt`.
- `python tools/dev/lab.py run --feature=summon` runs four staged match-ups and prints the numbers: allies, guards, raiders and neutral (`lab/fixtures/summon-*.lab`).
