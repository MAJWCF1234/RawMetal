$ErrorActionPreference = 'Stop'
$audioSource = 'O:/retro official/Audio'
$audioTarget = Join-Path $PSScriptRoot '../assets/audio'
New-Item -ItemType Directory -Force -Path $audioTarget | Out-Null
$clips = @(
 @('shot', 'Sound Effects/Firearms/Fire/shotgun_fire_ufx_1.ogg'),
 @('pump', 'Sound Effects/Firearms/Racking/shotgun_racking_ufx_1.ogg'),
 @('metal1', 'Sound Effects/Player/Footsteps/Metal/footstep_metal_ufx_1.ogg'),
 @('metal2', 'Sound Effects/Player/Footsteps/Metal/footstep_metal_ufx_2.ogg'),
 @('metal3', 'Sound Effects/Player/Footsteps/Metal/footstep_metal_ufx_3.ogg'),
 @('metal4', 'Sound Effects/Player/Footsteps/Metal/footstep_metal_ufx_4.ogg'),
 @('concrete1', 'Sound Effects/Player/Footsteps/Concrete/footstep_concrete_ufx_1.ogg'),
 @('concrete2', 'Sound Effects/Player/Footsteps/Concrete/footstep_concrete_ufx_2.ogg'),
 @('concrete3', 'Sound Effects/Player/Footsteps/Concrete/footstep_concrete_ufx_3.ogg'),
 @('concrete4', 'Sound Effects/Player/Footsteps/Concrete/footstep_concrete_ufx_4.ogg'),
 @('jump-takeoff', 'OGG/Player/Footsteps/footstep_concrete_a_3.ogg'),
 @('landing-body', 'OGG/Player/Footsteps/footstep_concrete_a_7.ogg'),
 @('pickup', 'Sound Effects/Misc/item_pickup_ufx_1.ogg'),
 @('hurt', 'OGG/Player/Misc/hit_1.ogg'),
 @('spider_call', 'OGG/Creatures/Mindleech/mindleech_1.ogg'),
 @('spider_attack', 'OGG/Creatures/Barnacle/barnacle_2.ogg'),
 @('spider_death', 'OGG/Creatures/Barnacle/barnacle_4.ogg'),
 @('wasp_call', 'OGG/Creatures/Alien Drone/alien_drone_talking_1.ogg'),
 @('wasp_attack', 'OGG/Creatures/Alien Drone/alien_drone_roaring_1.ogg'),
 @('wasp_death', 'OGG/Creatures/Alien Drone/alien_drone_roaring_3.ogg'),
 @('brute_call', 'OGG/Creatures/Daemon/daemon_roar_1.ogg'),
 @('brute_attack', 'OGG/Creatures/Daemon/daemon_roar_3.ogg'),
 @('brute_death', 'OGG/Creatures/Daemon/daemon_roar_7.ogg'),
 @('music', 'Tracks/ost_melting_spine_mx_1_short_loop.ogg'),
 @('machine', 'Sound Effects/Machines/old_machine_mx_1_loop.wav'),
 @('wings', 'OGG/Creatures/Alien Drone/alien_drone_flying_1.ogg'),
 @('empty', 'Sound Effects/Firearms/Misc/rifle_universal_empty_barrel_ufx_1.ogg'),
 @('exit', 'Sound Effects/Machines/retro_computer_beep_mx_1.ogg'),
 @('door', 'OGG/heavy_door_open-close_1.ogg'),
 @('punch-swing', 'Sound Effects/Melee/Swing/universal_swing_miss_light_ufx_1.ogg'),
 @('punch-hit', 'Sound Effects/Melee/Hit/blunt_hit_body_ufx_1.ogg')
)
$manifest = @('# Imported RawMetal audio', '', 'Copied and converted to 44.1 kHz PCM16 for the embedded stereo mixer. Source files are untouched.', '')
for ($i = 0; $i -lt $clips.Count; $i++) {
 $clip = $clips[$i]
 $source = Join-Path $audioSource $clip[1]
 if (!(Test-Path -LiteralPath $source)) { throw "Missing audio: $source" }
 $channels = if ($clip[0] -eq 'music') { 2 } else { 1 }
 $filter = if ($clip[0] -eq 'door') { 'atrim=duration=1.5,afade=t=out:st=1.3:d=0.2,loudnorm=I=-20:TP=-3:LRA=11' } else { 'loudnorm=I=-20:TP=-3:LRA=11' }
 if ($clip[0] -eq 'jump-takeoff' -or $clip[0] -eq 'landing-body') {
  $foleySource = Join-Path $audioSource 'OGG/Player/Misc/equip_1.ogg'
  $mixFilter = if ($clip[0] -eq 'jump-takeoff') { '[0:a]atrim=duration=0.28,afade=t=out:st=0.17:d=0.11,volume=0.85[a];[1:a]atrim=duration=0.3,afade=t=out:st=0.12:d=0.18,volume=0.18,adelay=35[b];[a][b]amix=inputs=2:normalize=0,loudnorm=I=-24:TP=-5:LRA=11[out]' } else { '[0:a]atrim=duration=0.45,lowpass=f=3200,volume=1[a];[1:a]atrim=duration=0.4,volume=0.22,adelay=65[b];[a][b]amix=inputs=2:normalize=0,afade=t=out:st=0.3:d=0.18,loudnorm=I=-21:TP=-3:LRA=11[out]' }
  & ffmpeg -hide_banner -loglevel error -y -i $source -i $foleySource -filter_complex $mixFilter -map '[out]' -ar 44100 -ac 1 -c:a pcm_s16le (Join-Path $audioTarget ($clip[0] + '.wav'))
 } else {
  if ($clip[0] -eq 'punch-swing') { $filter = 'loudnorm=I=-23:TP=-4:LRA=11' }
  & ffmpeg -hide_banner -loglevel error -y -i $source -vn -af $filter -ar 44100 -ac $channels -c:a pcm_s16le (Join-Path $audioTarget ($clip[0] + '.wav'))
 }
 if ($LASTEXITCODE -ne 0) { throw "Audio conversion failed: $source" }
 $manifest += "- $($clip[0]).wav (resource $(200 + $i)): $source"
}
$manifest | Set-Content -LiteralPath (Join-Path $audioTarget 'SOURCES.md')
Copy-Item -LiteralPath 'O:/retro official/echoes-audio-super-kit/Echoes - Audio Super Kit/Game Asset License Agreement.pdf' -Destination (Join-Path $audioTarget 'Echoes-License.pdf')
Copy-Item -LiteralPath 'O:/retro official/rot-horror-audio-bundle/ROT - Horror Audio Bundle/Game Asset License Agreement.pdf' -Destination (Join-Path $audioTarget 'ROT-License.pdf')
