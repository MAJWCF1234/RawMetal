# Multiplayer

DepthWorks plays deathmatch on custom maps and co-op through Epic Online Services (EOS), free, player-hosted.

## Layers (src/net/NetTypes.h is the contract)

1. **Session**: who plays together.
   - It handles signing in (EOS Connect with a Device ID, so players need no Epic account), lobbies (host, browse, join code, quick match) and peer-to-peer packets between lobby members.
   - **EosSession** (src/net/EosSession.cpp) is the real session.
   - **LoopbackSession** (src/net/Loopback.cpp) runs in memory for tests: several sessions in one process on one LoopbackHub.
2. **Match** (src/net/Match.cpp): the game over a Session.
   - The host's game is the truth. Guests send their input and shots; the host sends back what happened.
   - Deathmatch and co-op rules, spawns, respawns and scores.
   - A guest who lacks the map is sent the host's .rmc file.
   - The Match fills `Game::m_remotePlayers` and `Game::m_matchView`.
3. **Presentation**: the levelled-up title menu with MULTIPLAYER, the multiplayer menus, the scoreboard (Tab), the kill feed, and other players drawn as Claire or the Limitator. It reads only `Game::remotePlayers()` and `Game::matchView()`, and calls the Session.

Nothing below Session knows about the game. Nothing in the game knows about EOS.

## Finding each other

- **Browser:** public games are listed, filtered by mode and map, sorted by ping.
- **Join code:** six characters such as `K7QM-2X`, with no 0, O, 1, I or L. Private games are reached only this way.
- **Quick match:** joins the fullest open game of a mode, or hosts one.
- **NAT:** EOS P2P punches through and falls back to Epic's relays, so nobody needs port forwarding.
- **Host leaves:** the lobby passes to another member. Deathmatch carries on under the new host; co-op ends for the guests.

## Channels

| Channel | Carries | Delivery |
|---|---|---|
| 0 Control | hello, welcome, rules, goodbye | reliable |
| 1 State | snapshots | unreliable, newest wins |
| 2 Events | shots, hits, deaths, pickups | reliable |
| 3 MapData | the .rmc file in chunks | reliable |

## Maps

- The `MULTIPLAYER` record and `SPAWN` records of a custom campaign arrive as `CustomCampaign::multiplayer` (src/world/World.h).
- The Campaign Builder sets them on its Multiplayer page, with a spawn tool.
- A campaign with neither still plays: every mode is on, and spawns are found on open floor of the start map.
- **Record syntax** (map format v2, named fields only, so the file needs `FORMAT|2`):
  ```
  MULTIPLAYER|deathmatch=1|coop=1|dmmap=-1|maxplayers=8|fraglimit=20|timelimit=10|friendlyfire=0
  SPAWN|map=0|x=12.5|y=4|z=0|yaw=90|team=0
  ```
- **MULTIPLAYER** (at most one): `deathmatch`, `coop`, `friendlyfire` 1 or 0; `dmmap` -1 (the start map) or a map of the campaign; `maxplayers` 2-16; `fraglimit` 0-999 (0 none); `timelimit` 0-120 minutes (0 none). A field left out keeps its default.
- **SPAWN** (at most 64): `map`, `x` and `y` are needed. x and y are metres in that map (0-24), `z` the floor the player stands on (absolute, as the MAP spawn height), `yaw` degrees (0 east, 90 south), `team` 0 (anyone) to 4. z, yaw and team default to 0.
- A bad value is refused with the line and the field named (src/world/CustomCampaign.cpp `readMultiplayerRecord`).
- The fields ride on map 0 like every campaign-wide record's, so `exportRmc` writes them back as they were written, under CAMPAIGN.
- The Campaign Builder: the **Multiplayer** tab on the right sets the record in plain words and warns when deathmatch is on with fewer than 4 spawns on its map. The **Spawn** tool (People) places spawns: a ghost figure with an arrow follows the pointer on the plan and in 3D, R turns it, a click sets it down. tools/Level Editor/EDITOR.md has the rest.
- The Starter Map shows it off: deathmatch in the Ground paint yard, with 8 spawns round its middle.
- **Tests:** `--each-test=map-format` / `engine.map-format` (parse, defaults, refusals, round trip, the Starter Map's spawns); `RawMetalEditor.exe --multiplayer-test` (also inside `--editing-test`).

## Credentials

- `src/ThirdParty/EOS/eos_config.json` is gitignored, because the repository is public. It is baked into the exe at build time.
- Without it, or when the SDK cannot load, the Session is Offline: the MULTIPLAYER menu says why, and the game plays on alone.
- The EOS DLL is packed inside the exe and unpacked to `%LOCALAPPDATA%\RawMetal\eos\` on first use, so there is still one exe.
  - Resource 5990: the asset packer's LZMS record (`RawMetalAssetPacker --pack-file`), 19.5 MB down to 5.7 MB. The folder is named after the record's hash, so a new SDK unpacks beside the old one.
  - The import library is delay-loaded; the session loads the unpacked DLL by full path before the first EOS call.
- The EOS product version follows app.rc's ProductVersion. Lobbies carry it as `version`: the browser and quick match list only the same version, and a code for another version says so.
- Sign-in, lobby results and SDK warnings go to `RawMetal-eos.txt` beside the exe. The client secret is never written.

## Packets over EOS

- Socket `DepthWorks`. Reliable sends are ReliableOrdered; the rest are UnreliableUnordered. Relays are allowed.
- Every EOS packet starts with a one-byte header, so an unreliable message holds at most 1169 bytes (`EosUnreliableMax`); a bigger one is refused (`send` returns false).
- A reliable message up to 64 KB (`EosReliableMax`) is cut into fragments and arrives whole.
- Connections are accepted only from members of the current lobby.

## State kept network-friendly

- `AvatarPose` is sent as it is for VR players. `solveAvatar` is pure, so a remote body poses the same as the local one. Its hand, back and hip slots (WeaponId+1, the MAC-11 is 4) take three bits each.
- `RemotePlayer::character` picks the body: 0 Claire, 1 the Limitator.

## The Match (src/net/Match.cpp, MatchPlay.cpp)

- **Game's frame:** `Game::update` ticks the Session, makes a Match when the Session is in a lobby, runs the game's own frame, then the Match. The Match reads the session's state, members and host (it never takes its events, which stay for the menus). It is dropped, and the title comes back, when the session leaves, the lobby closes or the match ends; quitting to the title from the pause menu leaves the lobby.
- **In a match:** no saving or loading; the pause menu does not stop the world.
- **Joining:**
  - A guest sends hello (name, `Settings::character`, protocol version). The host answers welcome: slot, rules, the map's file name and content hash (`contentHash`, FNV-1a 64; `mapFileHash` for a file).
  - A guest without that exact file asks for it. The host streams it on MapData in chunks of 1100 bytes, and the guest saves it as `custom maps/Downloaded/<name>.rmc`, loads it and starts.
  - Deathmatch plays `multiplayer.deathmatchMap` (-1 the start map); co-op plays the start map.
- **State:**
  - Guests send their own state at 30 Hz; the host sends everyone's at 20 Hz. Both are unreliable, newest wins.
  - A player is 20 bytes in a snapshot (80 in VR, with the avatar pose): feet, angles, velocity, stance, weapon, shots, reload, life, torch and locker (protocol 2), health. A snapshot of 8 players is 170 bytes, and a guest's own state is 28. That is 3.3 KB/s down and 0.8 KB/s up per guest, and 23 KB/s out of the host to 7 guests. These are payload figures; EOS adds its own headers.
  - Others are drawn 100 ms behind, in the host's clock.
  - Movement is the guest's own. The host refuses a move too fast or through a wall, and sends a correction. Every respawn and correction bumps the player's `life`, so older reports are ignored.
- **Shots:**
  - The host traces every shot, its own included (`Game::shoot` hands it to `Match::localShot`), against the world and every player's stance hull.
  - Players are rewound to the time the shooter saw (at most 0.3 s). They are lent to `traceShot` as `Game::NetBody` and come back as `ShotKind::Player`, index slot+1.
  - Damage, hits, deaths and the shot's marks go to everyone.
- **Deathmatch:**
  - No creatures.
  - Players spawn at the map's SPAWN records, or on open floor spread from the start. A respawn comes after 3 s, at the spawn farthest from living opponents, with full health and a fresh map's shotgun.
  - The frag or time limit ends the round, and the host starts the next 10 s later.
- **Co-op:**
  - Guests' minds and scripts are off (`Game::m_netCreaturesRemote`, `m_netScriptsOff`): creatures, guards, civilians, escorts and engineers think on the host only, and scripts run there. Each guest is sent the creatures (15 Hz) and doors (5 Hz) of the map it stands on and the maps stitched beside it, and the people (10 Hz).
  - Players may be on different maps at once: see "Several maps at once" below.
  - Pickups taken and doors used go both ways.
  - The host changing map does not move anyone: every map with a player on it runs. With `COOPFOLLOW ON` (host console) the host places the guests who were on the map it left beside it.
  - A downed player returns beside the host after 10 s. Friendly fire follows the map's setting.
  - The host's minds know every player: see "One path" below.

## One path: every player, one list (Game::players())
Single player and co-op run the same code. Scripts, creatures and people read `Game::players()`, never `m_player` alone; single player is that list with one entry, the local player (id 0, its body `&m_player`, live). On a co-op host the Match adds each guest's body (src/net/MatchCoop.cpp `syncBodies`): id is their slot (the host is slot 0), feet in the active chunk's frame like `m_player`, stance, eye, angles, health as the host counts it. The only difference between single player and co-op is how many players are in the list, and on a guest that the minds run on the host. What a mind reads beyond the body comes through `playerVelocity`, `playerLit` (light on them: `Game::lightAt`, the formula the local exposure uses, measured on the host for a guest), `playerHidden` (shut in a locker) and `playerTorch`. A blow on a player goes through `Game::hurtPlayer`: the local player takes it as `receiveDamage` did; a guest's is queued (`m_netHurts`) and the Match takes it off that guest (`damage`, then Hit and Death to everyone, so the guest's health comes from the host).
- **Scripts:**
  - A SCRIPT_EVENT is tested for each player (`scriptEventReadyFor`): on its map, its gates, inside its box. It fires for the first player it is ready for, or with `players=all` (map format 2) only when it is ready for every player. `@health` and `@map` read the player tested; `@kills`, `@remaining`, states and entity properties are the world's.
  - A timed sequence starts when any player stands in its box (and looks at its actor when it asks). On a guest only the host starts one; its clock comes over.
  - ITEM and TAKE go to the player who fired the event: the host's own, or sent to that guest (ScriptFx). Sounds, shakes, captions (MESSAGE, its `{state}` filled in on the host), RELEASE and the ending go to everyone (`Game::m_scriptFx`, drained by the Match after the frame).
  - A guest's hand on a terminal (a switch, a lever, a lift call: `Game::operateTerminal`) is sent to the host (Use), which works it for them: the state it throws, the lift if that guest stands in it, the reactor console, the cursed equipment it wakes. The screen and its sounds are the guest's own. Doors were already sent (DoorUse).
  - The state store reaches guests (StateSync, reliable): every named state, objective and entity property that changed, every 0.2 s; the whole store every 10 s and to a guest who joins (it replaces theirs). So a guest's HUD (objectives, `{state}` captions, lamps switched by states, sequence captions and voiced cues, lift and props frozen by a property) shows what the host's does, and a hand-over would know it. Running clocks (sequence timers, people's idle and step counters, the lift, the storm) are sent when they start or stop and once a second between; a guest runs them itself and takes the host's value only when it has drifted more than 0.5 s. Each player's own states stay theirs (`flashlight_on`, `player_locker`), and the minds' working states (the top bit, `actorAiState`) stay on the host.
- **Creatures** (`Game::creatureTarget`, src/game/ai/enemy/EnemyAI.cpp): each think, a creature picks its prey from the living players. One player: that one, nothing weighed. Several: each is scored through the creature's own senses and the stealth rules it always used: in sight now (`aiCanSee` from its eyes, a locker hides) +40; the player who wounded it in the last 6 s (`Enemy::aggro`, set by any player's shot, a guest's included) +25; near where it last knew someone (what it saw or heard last) +12; each metre nearer +1; each other creature close by already on that player -8, so a pack spreads. It keeps its prey unless another scores 6 more, and never changes within a second of choosing. Downed and dead players are no one's prey. The rest of its think (recognition at its notice time, crouch, speed and light; the stalker's stare-down against that player's gaze; the lunge, the bite, the swing) runs against that player's body. A guest's footsteps are heard as the local player's are: the host makes them into sounds from the guest's speed and stance (`stepHeard`); their shots were already sounds on the host. Grapples: the Transformed's and the Snatcher's holds bite a guest as they bite you, but only the local player is pinned and hauled (a guest's body moves on its own machine); creatures that take the player out of a locker (LockerCapture) only do it to the local player. A recurring hunter's ear (`m_playerNoise`) is the local player's alone.
- **Guards and people** (src/game/ai/guard, src/game/ai/friendly): a guard's targets (`AiTargetRef` kind Player) carry the player's id as their index (single player: 0). He considers every player (`guardThreat`), sees each by the same cone, range, crouch and light (`guardCanSee`), makes them out by the same rate (`aiRecognitionRate`), blames the nearest player for a body he finds, and his fire is traced against every player's body (`traceShot` with the players) and lands through `hurtPlayer`. A crouching player near a civilian has him crouch; escorts keep with the nearest player; nobody walks through any player's body. The stings and ticks of being seen are the local player's own. Holding a player at gunpoint, hauling them out of a locker and detaining them are the local player's only (the hold moves the body); a guest who hides is still out of every guard's sight.
- **People on guests** (Actors, 10 Hz, unreliable): each guard, civilian and escort who thinks: feet, facing, clip and phase, and the fields their weapon, crouch, hold and cards are drawn by when not zero (MatchCoop.cpp `ActorFields`). A guest eases them to it (as creatures) and runs none of their minds, combat, drones or grenades.
- **Bandwidth per guest** (payload, `--net-test` measures it): a person is 13 bytes plus about 3 bytes for each live field (an armed guard in a fight about 30), so ten at 10 Hz is 1.5-3 KB/s; the state store at rest is under 0.1 KB/s and a burst of script changes is 8 bytes a state; a script effect is 16 bytes and its text. Creatures (unchanged) are 20 bytes each at 15 Hz. Up, a guest adds 4 bytes per terminal used.
- **Not sent** (they run on each machine as before): conversations (the dialogue a guest opens and the states its replies set stay on that guest), Follow/Wait commands to an escort, NPC voices and radio barks (they key off the minds' states), the scripted breach of map 14's receiving bay.
- **The host leaves:** in deathmatch the new host carries on from its own state, and the others hello it again. In co-op the guests' match ends with the line "The host left the game".

## Several maps at once (co-op; src/game/ForeignChunks.cpp, MatchPlay.cpp `trackLevels`)
The host runs every map a player stands on, not only its own.
- **Which maps:** each update the Match lists the maps the guests stand on. `Game::m_netKeep` holds them and the maps stitched beside them (touching on the grid): `updateStreaming` keeps those resident, as it keeps the local player's. `Game::m_netForeign` holds the occupied maps other than the host's own.
- **How another map runs:** everything that thinks was written against the active chunk (`m_world`, `m_enemies`, `m_clutter`, the players in its frame). So after the host's own frame, each map in `m_netForeign` gets a frame of its own (`Game::updateForeignChunks`): `ChunkFrame` swaps that map in as the active chunk (a three-way swap of world, creatures, pickups, props, nav graph and kills: nothing is copied) and the host's map out, its scripts, people, guards and creatures run, and the host's map comes back. Doors and props of every resident map already run in the host's own frame, so they are not run twice.
- **Who is in a frame:** `players()` holds the players in the active chunk's frame: its map and the maps stitched beside it (`Game::inPlayerFrame`). A player on another map is not there, even where a custom campaign's maps share an origin (a file's maps are kept apart by the loader; nothing else needs them to be). In another map's frame the host is in the list only if its map is beside that one. A person thinks in the frame of the map they are on (`actorInFrame`); a map's script events run in its frame, campaign-wide ones in the host's.
- **Teleports keep state:** the Match's moves to another map (`teleportLocal`: a correction, a respawn, TP) use `Game::enterChunk`, which stores the map left and swaps the next in as a seam crossing does, so a map someone is still on is not reloaded. A point inside the active map is the active map's (`crossChunkBoundary`, `worldAt`, `Match::levelAt` with a preferred map) where maps overlap.
- **What a guest is sent:** the creatures and doors of its map and of the maps beside it (`seesLevel`); a Creatures message carries the list's size, so a creature the guest's own crossing carried along is dropped. Shots on another map are traced in that map's frame and their marks reach only players who can see that map. A guest's lever on another map is worked in that map's frame. A guest's footsteps are heard on its own map.
- **A guest leaving a map:** a guest walks over a seam on its own; the host takes the new map (its own reports change map with a continuous world position) and makes it resident. Nothing pulls it back.
- **Frame cost** (`--net-test`, lab build, one host and three guests, six hunting creatures on each map): 0.85 ms with one occupied map, 0.64 ms with two, 1.26 ms with four. A frame for another map costs its creatures' and people's thinking; props and doors of every resident map ran before.
- **Not yet:** engineers' drones and guard grenades think in the host's own frame only; a guest on another map is not their target. A hunter crossing a seam after a player follows the host only.

## Props (src/net/MatchProps.cpp; deathmatch and co-op)
The host's props are the truth.
- **Names:** every prop has a match name (`Clutter::netId`): (map+1)<<16 | its CLUTTER record+1 for an authored prop, given by `seedClutter` alike on every machine; (map+1)<<16 | 0x8000+n for one the host spawned (announced by PropSpawn). Shards and gibs are `Clutter::NetDebris`: each machine's own, never sent.
- **The host** simulates every prop. Fifteen times a second each guest is sent, for its map and the maps beside it, every prop that moved since it was last sent (3 mm or about half a degree, a flag or the liquid level changed), three times more once it stops (against loss), and eight more a tick in turn whether they moved or not, so a late joiner or a lost packet catches up (PropState, unreliable). Breaks (PropBreak), spawns (PropSpawn) and panes or walls lost (PaneBreak) go reliably.
- **A guest** poses its props from that, 100 ms behind the host's clock as it draws players, and keeps them asleep: whatever its own solver woke that the host did not goes back where the host last had it. Nothing breaks on a guest but what the host breaks; a break arrives as PropBreak and the guest breaks its copy (its own shards, its own spill).
- **Hands:** what a guest's hand holds (desktop or VR, one hand or two) and the prop its Physgloves are pulling it moves itself at once and reports at 30 Hz (PropCarry); the host poses that prop from the reports (`netHolder`, out of its solver) and does not send it back to that guest. Let go (dropped, punted, thrown) the guest sends where it is and how it moves (PropThrow, exact floats): the host carries on from there; the guest's solver carries that prop on alone until the host's word on it is newer than the throw, then eases onto the host's.
- **Blows:** a guest's punch, kick or VR fist on a prop (`knockClutter`) and its damage (`damageClutter`) go to the host as PropKnock and are dealt there. Shots were already the host's. A guest walking into a prop pushes it on the host (co-op: the guests' bodies shove props as the player's does; a guest leaning on a heavy one, walking keys held while stopped, shoves it the way they face: the `pushing` bit of the player state).
- **Liquids:** a bottle's level rides in PropState; a broken bottle spills on every machine as it breaks; pouring from a tipped bottle runs on each machine from the pose it was sent.
- **Single player:** none of this runs; with no match props behave exactly as before.
- **Bandwidth** (payload): a prop in PropState is 13 bytes (14 with liquid). Fifty props awake on a guest's map: 657 bytes a tick, 9.6 KB/s at most; `--net-test` measured 9.0 KB/s while fifty knocked crates fell. At rest a map costs eight props a tick (1.6 KB/s). Up, a guest carrying something sends 15 bytes at 30 Hz (0.44 KB/s); PropThrow is 45 bytes, PropKnock 33.
- **Not yet:** in deathmatch only the host's own body pushes props by walking (guests' hands, throws, blows and shots still go through the host); VR hand shoves without a grab are the guest's own; containers' contents are props and follow as props, not as one.
- **VR:** main hands the solved `AvatarPose` to `Game::setNetAvatar` each frame in a match.

## Tests

- `--net-loopback-test` / `engine.net-loopback`: the in-memory hub.
- `--net-test` / `engine.net`: Games in one process play matches over the hub (deathmatch, co-op, map transfer, loss). Co-op's one world (a FORMAT 2 map written by the test): a guest's step fires a box trigger on the host and its state, door and caption reach the guest; a `players=all` trigger waits for both; a guest's lever runs on the host; a creature that sees only the guest hunts and wounds the guest through the host; with both in sight it takes the nearer and does not flip in a second of small moves; a pack of two splits; a downed guest is dropped; a guard walks on the host and the guest's copy follows; the guard takes the guest as his target and wounds them; the co-op traffic. Several maps (a campaign written by the test: two maps stitched with an open seam, a third on the first's own origin, a fourth with fifty crates): host and guest on different maps at once, neither drawing the other; each map's creature hunts the player on it, the guest's wounds the guest through the host while the host's map runs too; the guest is sent only its own map's creatures; a guest walks over the seam alone and is neither pulled back nor put in a wall; the host changing map leaves the guest where it is, and with COOPFOLLOW brings the guest on the map it left; a crate the host knocks over moves on the guest; a box the guest picks up follows on the host and, thrown, lands in the same place on both; a bottle broken on the host breaks and spills on the guest; a guest's blow on a crate is dealt on the host; fifty knocked crates come to rest alike (and their traffic); the host's frame with 1, 2 and 4 occupied maps; props over a hub with 20% loss and latency.
- `engine.streaming` (lab tube for `--each-test=streaming`): door streaming, seams both ways, Ashfall residency, visible halls.
- The single-player suites prove one player is the old game: `engine.scripting`, `engine.saves`, `engine.ai`, `engine.creature-behaviour`, `engine.creature-schedule`, `engine.chimera-hunt`, `engine.friendly-ai`, `engine.stealth`, `engine.dialogue`, `engine.map-format`.
- `--eos-test` / `engine.eos`: signs in to the Dev sandbox, hosts, finds, leaves. Needs the internet and the keys.
- **Two machines:** the real test. See the playtest list at the end of this file once it exists.
