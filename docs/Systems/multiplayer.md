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
| 2 Events | shots, hits, deaths, pickups, chat | reliable |
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
- The EOS DLL (and XAudio 2.9 for voice, resource 5991) is packed inside the exe and unpacked to `%LOCALAPPDATA%\RawMetal\eos\` on first use, so there is still one exe.
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
  - Guests' minds and scripts are off (`Game::m_netCreaturesRemote`, `m_netScriptsOff`): creatures, guards, civilians, escorts and engineers think on the host only, and scripts run there. The host sends its map's creatures at 15 Hz, its people at 10 Hz and its doors at 5 Hz.
  - Pickups taken and doors used go both ways.
  - A guest far from a host who changed map is brought beside it.
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
- **VR:** main hands the solved `AvatarPose` to `Game::setNetAvatar` each frame in a match.

## Chat and voice (src/net/MatchChat.cpp, src/game/SettingsMenu.cpp, EosSession's voice section)

- **Text chat:**
  - T opens a chat line in a match (not in VR); Enter sends, Esc drops it. The world runs on while typing; the player stands still. `InputState::chat` is T, `InputState::textInput` carries the letters.
  - A line goes to the host as `ChatSay` on Events (reliable). The host cleans it (printable ASCII, spaces folded, at most 120 characters) and holds each sender to an allowance: 4 lines at once, then one every 1.5 s (`Match::ChatBurst`, `ChatRefill`). The rest are dropped. The sender keeps the same count and is told "Too fast" without sending.
  - The host relays every line as `ChatRelay` to everyone, the sender too, so every machine shows the same lines in the same order. An older game ignores both messages, so the protocol stays 1.
  - Notices are made on each machine, with no traffic: "X joined the game" and "X left the game" from the roster, "X killed Y" and "The dark took Y" from the kill feed. A newcomer is not told about those already there.
  - What the box shows follows this machine's settings (`MatchView::chat`, newest last): text chat off hides the players' lines and keeps the notices; the chat filter masks a small built-in word list with asterisks (whole words, and stems such as "fuck"; "Scunthorpe" is left alone); a muted player's lines are dropped.
  - The box (FrontEnd.cpp `drawChat`) sits lower left above the HUD: four lines, each full for 8 s and gone by 10 s; while typing, the last six and the line under them with a count of what is left.
  - VR: there is no typing. The newest line or notice (6 s) rides the VR caption strip when no subtitle, objective or alarm needs it (`Game::vrChatCaption`, read in SoftwareRenderer's VR card stamping).
- **Voice (EOS RTC):**
  - The platform starts with RTC options naming XAudio 2.9.
    - Windows' own `System32\xaudio2_9.dll` does not work: the SDK delay-loads `XAudio2_9Redist.dll` by that name, and the process stops (exception 0xC06D007E) when no module of that name is loaded.
    - So the SDK's `xaudio2_9redist.dll` is packed like the SDK DLL: resource 5991, an LZMS record of 383,838 bytes (847 KB unpacked). It is unpacked to `%LOCALAPPDATA%\RawMetal\eos\<hash>\xaudio2_9redist.dll` and handed to the platform by path.
    - Without it, or if the platform will not start with RTC, the game starts without voice and says why.
  - Every hosted lobby has an RTC room (`bEnableRTCRoom`); members join it on joining the lobby and leave it on leaving. If a deployment refuses lobbies with voice, the host hosts again without it and voice says why.
  - The gate is in the audio thread's `AudioBeforeSend`. It measures the mic for the meter (peak on a 60 dB scale, `micLevelOf`) and sends silence unless push-to-talk is held or, in open mic, the level is over the threshold (with 350 ms of hangover). Voice off disables sending and receiving.
  - Devices come from `EOS_RTCAudio_Query{Input,Output}DevicesInformation` and refresh when Windows' devices change. Settings keep them by name ("" the system default).
  - Mic volume and voice volume go to `UpdateSendingVolume` and `UpdateReceivingVolume` (EOS 50 = as it comes, so the settings' 0-2 map to 0-100).
  - Muted players go to `UpdateReceiving(participant, off)`. Proximity goes to `UpdateParticipantVolume`.
  - Speaking comes from `AddNotifyParticipantUpdated`.
  - Everything fails quietly to "voice off" with a reason: Offline, signing in, no XAudio, no microphone ("no microphone was found"), a microphone unplugged or failed. None of it blocks a frame.
- **Session interface** (NetTypes.h, additive): `setVoice(VoiceSettings)`, `setTalking`, `voiceStatus()` (available, inRoom, transmitting, reason), `inputDevices`/`outputDevices`, `micLevel`, `peerSpeaking`, `setPeerMuted`, `setPeerVolume`.
  - OfflineSession answers "multiplayer is offline (why)".
  - LoopbackSession fakes it for tests: a member transmits while push-to-talk is held or `fakeMicLevel` is over the open-mic threshold; `fakeVoiceRoom`, `fakeInputs`/`fakeOutputs`; mutes and volumes are kept in `mutedPeers`/`peerVolumes`.
- **The game's side:**
  - `Game::updateVoice` hands the COMMUNICATION settings to the session whenever they change, and the push-to-talk key every frame (Win32Window reads `Settings::pushToTalkKey`; VR's push-to-talk sets `InputState::talk` too).
  - `Match::publishComms`, every frame, fills `ScoreRow::speaking`/`muted` and `RemotePlayer::speaking`. Every 0.2 s it sets the session's mutes (by display name, from `Settings::mutedPlayers`) and, in co-op with proximity voice on, each voice's volume: full within 3 m, gone by 30 m (a 1.5 power), a wall between (Game::lineOfSight) keeps 35 %, a player not near here is silent. Deathmatch is always heard in full.
  - Speaking shows as three small amber bars beside the name on the scoreboard and over the head (RemotePlayers.cpp). A muted player says MUTED in red on the scoreboard.
  - With Tab held, the arrows pick a scoreboard row and Enter mutes or unmutes that player.
- **TEST MY MIC** (the COMMUNICATION page) listens with Windows waveIn (`audio/MicMonitor`), only while it is on and the page is open, and shows the same meter. Nothing is recorded or sent.

## The settings menu (src/game/SettingsMenu.cpp, drawn by renderer/FrontEnd.cpp)

- **PAUSE** (Esc) is short, in the title's language (grouped rows, the lit red row, a line saying what it does):
  - Out of a match: RESUME, SETTINGS | SAVE GAME, LOAD GAME | RESTART, QUIT TO TITLE. RESTART and QUIT ask for a second press.
  - In a match: RESUME, SETTINGS | JOIN CODE (Enter copies it) | LEAVE MATCH.
  - `PauseMenuLayout` and `PauseRow` name the rows for the pointer, the renderer and the tests.
- **SETTINGS** (from PAUSE or the title) has a column of pages: AUDIO, VIDEO, CONTROLS, GAMEPLAY, COMMUNICATION, VR, then BACK.
  - The column has the keys first: up and down turn the pages, right or Enter goes into the rows, Esc leaves.
  - In the rows: left and right change a value, Enter toggles or acts, Esc goes back to the column.
  - The mouse and the VR laser click pages, hover and click rows and drag sliders.
  - The chosen row's one plain line sits at the foot. Every page ends with RESET TO DEFAULTS (a second press; muted players stay muted).
  - Changes apply at once. Leaving SETTINGS asks main to save (`Game::takeSettingsSave`).
- **What each page holds:**
  - AUDIO: master, music, effects, voice chat volume.
  - VIDEO: render scale (50/75/100 %, the console's `scale`), depth of field, lens effects, the frame-time readout.
  - CONTROLS: mouse sensitivity, invert Y, and the keys (read-only).
  - GAMEPLAY: pull highlight, character (Claire or the Limitator).
  - COMMUNICATION: text chat, chat filter, voice chat (off / push to talk / open mic), the push-to-talk key (V, B, X, H, Caps Lock, Left Alt, Mouse 4, Mouse 5), open mic sensitivity with the live meter, microphone, speakers, mic volume, voice volume, proximity voice in co-op, muted players (a list to mute and unmute; the players in the match and everyone muted before), test my mic.
  - VR: the comfort settings (turn speed, vignette, seated, recalibrate height, left-handed, render scale, frame-rate readout). Turning is smooth only.
- **The settings file** stays append-only. After the character come text chat, filter, voice mode, push-to-talk key, open-mic threshold, mic volume, voice volume, proximity, render scale and frame-time readout. Then come named lines (`mic <name>`, `speakers <name>`, `muted <name>`) for what can hold spaces. An older file loads with the new defaults.

## Tests

- `--net-loopback-test` / `engine.net-loopback`: the in-memory hub.
- `--net-test` / `engine.net`: Games in one process play matches over the hub (deathmatch, co-op, map transfer, loss). Co-op's one world (a FORMAT 2 map written by the test): a guest's step fires a box trigger on the host and its state, door and caption reach the guest; a `players=all` trigger waits for both; a guest's lever runs on the host; a creature that sees only the guest hunts and wounds the guest through the host; with both in sight it takes the nearer and does not flip in a second of small moves; a pack of two splits; a downed guest is dropped; a guard walks on the host and the guest's copy follows; the guard takes the guest as his target and wounds them; the co-op traffic.
- The single-player suites prove one player is the old game: `engine.scripting`, `engine.saves`, `engine.ai`, `engine.creature-behaviour`, `engine.creature-schedule`, `engine.chimera-hunt`, `engine.friendly-ai`, `engine.stealth`, `engine.dialogue`, `engine.map-format`.
- `--eos-test` / `engine.eos`: signs in to the Dev sandbox, hosts, finds, leaves. Needs the internet and the keys.
- **Two machines:** the real test. See the playtest list at the end of this file once it exists.
- `--net-test` / `engine.net`: Games in one process play matches over the hub (deathmatch, co-op, map transfer, loss).
  - Chat: a guest's line reaches the host and the other guest; it is cut to 120 characters; the filter masks for whoever has it on; a muted sender's lines are hidden; text chat off hides lines; a burst of 10 is cut to 4 by the sender and again by the host; join, leave and kill notices.
  - Voice: push-to-talk marks the speaker on the scoreboard and the name tag; voice off reaches the session; deathmatch voices are full; co-op proximity follows distance and the wall.
- `--eos-test` / `engine.eos`: signs in to the Dev sandbox, hosts, finds, leaves. Voice too: the lobby's RTC room is joined, the microphones and speakers are listed (or it says the machine has none), open mic over the loudest level sends nothing, and leaving leaves the room. Needs the internet and the keys.
- `engine.settings`: PAUSE and SETTINGS by pointer and keys, sliders, resets, restart and quit confirmations, the settings file round trip; it runs `Game::testSettingsMenu` (settings-menu-test.txt). VR's `--vr-test` runs `Game::testComfortMenu` (the VR page).
- Lab tube `communication`: the chat box talking and typing, the speaking marks, PAUSE alone and in a match, and every SETTINGS page with a fake mic and device list (`.build/lab-out/communication-*.png`).
- **Two machines:** the real test. For chat and voice:
  - Each hears the other on push-to-talk and in open mic.
  - Speaking marks show on the scoreboard and the name tag.
  - Muting from the scoreboard silences voice and chat.
  - Co-op proximity fades the voice with distance and walls.
  - Changing the microphone and speakers mid-match works.
  - Unplugging the mic says so on the COMMUNICATION page.
