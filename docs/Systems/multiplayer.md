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
- **Record syntax** (map format v2, named fields):
  ```
  MULTIPLAYER deathmatch=1 coop=1 dmmap=0 maxplayers=8 fraglimit=20 timelimit=10 friendlyfire=0
  SPAWN map=0 x=12.5 y=4 z=0 yaw=90 team=0
  ```

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

- `AvatarPose` is sent as it is for VR players. `solveAvatar` is pure, so a remote body poses the same as the local one.
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
  - A player is 19 bytes in a snapshot (79 in VR, with the avatar pose). A snapshot of 8 players is 162 bytes, and a guest's own state is 27. That is 3.2 KB/s down and 0.8 KB/s up per guest, and 22 KB/s out of the host to 7 guests. These are payload figures; EOS adds its own headers.
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
  - Guests' creature minds and scripts are off (`Game::m_netCreaturesRemote`, `m_netScriptsOff`). The host sends its map's creatures at 15 Hz and its doors at 5 Hz.
  - Pickups taken and doors used go both ways.
  - A guest far from a host who changed map is brought beside it.
  - A downed player returns beside the host after 10 s. Friendly fire follows the map's setting.
- **The host leaves:** in deathmatch the new host carries on from its own state, and the others hello it again. In co-op the guests' match ends with the line "The host left the game".
- **VR:** main hands the solved `AvatarPose` to `Game::setNetAvatar` each frame in a match.

## Tests

- `--net-loopback-test` / `engine.net-loopback`: the in-memory hub.
- `--net-test` / `engine.net`: Games in one process play matches over the hub (deathmatch, co-op, map transfer, loss).
- `--eos-test` / `engine.eos`: signs in to the Dev sandbox, hosts, finds, leaves. Needs the internet and the keys.
- **Two machines:** the real test. See the playtest list at the end of this file once it exists.
