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

## State kept network-friendly

- `AvatarPose` is sent as it is for VR players. `solveAvatar` is pure, so a remote body poses the same as the local one.
- `RemotePlayer::character` picks the body: 0 Claire, 1 the Limitator.

## Tests

- `--net-loopback-test` / `engine.net-loopback`: the in-memory hub.
- `--net-test` / `engine.net`: Games in one process play matches over the hub (deathmatch, co-op, map transfer, loss).
- `--eos-test` / `engine.eos`: signs in to the Dev sandbox, hosts, finds, leaves. Needs the internet and the keys.
- **Two machines:** the real test. See the playtest list at the end of this file once it exists.
