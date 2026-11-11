"""Bring the chosen Horror SFX (YourPalRob, purchased) into the engine's sound bank.

Reads the pack's OGG archive in place (nothing is unpacked beside it), and for
each entry of BANK writes one file into src/assets/library/audio (gitignored):

  loops      hsfx_<name>.mp3  mono, trimmed, levelled to -18 dBFS RMS; the engine cross-fades the wrap
  one-shots  hsfx_<name>.sfx  RMS1 (tools/EncodeEffects.py), leading silence cut,
                              peak-levelled, short fades

then points assets.rc at them, in BANK's order, from FIRST_RESOURCE. The order
is the order of the Amb*/One*/Sting*/Pa* block in src/audio/Sound.h and of
src/audio/SoundBank.h: keep the three in step.

Usage: python tools/audio/import_horror_sfx.py ["O:/retro official/Audio/Horror SFX Complete (ogg files).zip"]
"""
import array
import re
import struct
import subprocess
import sys
import wave
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import EncodeEffects  # noqa: E402

RC = ROOT/'src'/'assets.rc'
OUT = ROOT/'src'/'assets'/'library'/'audio'
WORK = ROOT/'.build'/'hsfx'
FIRST_RESOURCE = 2370
PACK = 'Horror SFX Complete OGG/'
ROBOT = 'Ambient/Robots and Computer Interfaces/'

# (name, loop, archive path under the pack folder, start s, length s, level)
BANK = [
    ('vent_airflow',      True,  'Ambient/Vent_airflow.ogg', 2, 12, .55),
    ('air_conditioner',   True,  'Ambient/Air conditioner_Ambient.ogg', 1, 12, .55),
    ('fan_large',         True,  'Ambient/Fan_large.ogg', 3, 12, .55),
    ('light_buzz',        True,  'House & Office/Home or Office/Light_electrical_buzzing.ogg', 1, 10, .5),
    ('fluorescent',       True,  'House & Office/Kitchen/Fluorescent light bulb buzz_2.ogg', 1, 10, .5),
    ('static_electrical', True,  'Ambient/Static_electrical.ogg', 1, 10, .5),
    ('radio_static',      True,  'Ambient/Radio_static.ogg', 1, 12, .5),
    ('water_drip',        True,  'Liquids/Water_dripping.ogg', 0, 12, .6),
    ('gas_leak',          True,  'House & Office/Home or Office/Gas leaking.ogg', 0.5, 6, .5),
    ('drone',             True,  'Ambient/Creepy_ambience_6.ogg', 1, 14, .6),
    ('haunting',          True,  'Ambient/Ambience_haunting_3.ogg', 0.5, 14, .6),
    ('crying',            True,  'Ambient/Crying_moaning_ambience.ogg', 0.5, 14, .6),
    ('mechanical',        True,  'Ambient/Mechanical Randomness_Spooky_5.ogg', 0.5, 12, .6),
    ('metal_tapping',     True,  'Ambient/Metal Tapping_ambiance.ogg', 0.5, 12, .6),
    ('metal_creak',       False, 'Ambient/Mechanical Moan and Creaking.ogg', 0, 6, .7),
    ('metal_moan',        False, 'Ambient/Mechanical Moan.ogg', 0, 5, .7),
    ('metal_bang',        False, 'Ambient/Metal_banging_2.ogg', 0, 4, .75),
    ('metal_drag',        False, 'Ambient/Metal_dragging.ogg', 0, 6, .7),
    ('metal_fall',        False, 'Ambient/Metal_falling_3.ogg', 0, 4, .75),
    ('glass_bang',        False, 'Ambient/Banging_thick glass material_1.ogg', 0, 4, .75),
    ('distant_yell',      False, 'Ambient/Distant Yell_Echo and Reverb.ogg', 0, 6, .7),
    ('distant_yell_2',    False, 'Ambient/Distant Yell_Echo and Reverb_2.ogg', 0, 7, .7),
    ('house_creak',       False, 'Ambient/Old House_creeky metal and wood_ambiance_4.ogg', 0, 4, .7),
    ('debris',            False, 'Ambient/Debris_dirt_falling_2.ogg', 0, 3, .7),
    ('monster_groan',     False, 'Monsters & Ghosts/Monster Groan_3.ogg', 0, 5, .75),
    ('monster_breath',    False, 'Monsters & Ghosts/Monster_breath.ogg', 0, 5, .7),
    ('monster_growl',     False, 'Monsters & Ghosts/Monster_growl_2.ogg', 0, 5, .75),
    ('sting',             False, 'Stingers & Spooky Triggers/Stinger_3.ogg', 0, 6, .8),
    ('sting_piano',       False, 'Stingers & Spooky Triggers/Piano_stinger_dissonent.ogg', 0, 6, .8),
    ('sting_scratch',     False, 'Stingers & Spooky Triggers/Scratch_high pitch_2.ogg', 0, 5, .75),
    ('pa_access_denied',      False, ROBOT+'Access Denied.ogg', 0, 5, .75),
    ('pa_access_granted',     False, ROBOT+'Access Granted.ogg', 0, 5, .75),
    ('pa_doors_locked',       False, ROBOT+'Doors Locked.ogg', 0, 5, .75),
    ('pa_doors_unlocked',     False, ROBOT+'Doors Unlocked.ogg', 0, 5, .75),
    ('pa_containment_breach', False, ROBOT+'Containment breach.ogg', 0, 5, .75),
    ('pa_evacuate',           False, ROBOT+'Evacuate Immediately.ogg', 0, 5, .75),
    ('pa_caution_hazard',     False, ROBOT+'Caution Hazardous Area.ogg', 0, 5, .75),
    ('pa_ventilation_offline', False, ROBOT+'Ventilation System Offline.ogg', 0, 5, .75),
    ('pa_unauthorized',       False, ROBOT+'Unauthorized Personnel Detected.ogg', 0, 6, .75),
    ('pa_system_lockdown',    False, ROBOT+'System Lockdown.ogg', 0, 5, .75),
    ('pa_backup_generators',  False, ROBOT+'Backup Generators Online.ogg', 0, 6, .75),
    ('pa_hostile_detected',   False, ROBOT+'Warning Hostile Creature Detected.ogg', 0, 7, .75),
]


def decode(source, start, length):
    """Mono 44.1 kHz 16-bit samples of the chosen stretch."""
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-ss', str(start), '-t', str(length), '-i', str(source),
                          '-ac', '1', '-ar', '44100', '-f', 's16le', '-'], capture_output=True, check=True).stdout
    samples = array.array('h')
    samples.frombytes(raw[:len(raw)//2*2])
    return samples


def level(samples, peak):
    top = max(1, max(abs(min(samples)), max(samples)))
    gain = peak*32767/top
    return array.array('h', (max(-32767, min(32767, int(s*gain))) for s in samples))


def level_bed(samples, rms_db=-18.0, ceiling=0.89):
    """Beds are levelled by loudness, not by their loudest click: -18 dBFS RMS, peaks kept under -1 dBFS."""
    top = max(1, max(abs(min(samples)), max(samples)))
    rms = max(1.0, (sum(s*s for s in samples)/len(samples))**0.5)
    gain = min(32767*10**(rms_db/20)/rms, ceiling*32767/top)
    return array.array('h', (max(-32767, min(32767, int(s*gain))) for s in samples))


def trim_silence(samples, floor=400):
    first = next((i for i, s in enumerate(samples) if abs(s) > floor), 0)
    last = next((i for i in range(len(samples)-1, -1, -1) if abs(samples[i]) > floor), len(samples)-1)
    first = max(0, first-441)
    last = min(len(samples), last+8820)
    return samples[first:last]


def fade(samples, head, tail):
    out = array.array('h', samples)
    for i in range(min(head, len(out))):
        out[i] = int(out[i]*i/head)
    for i in range(min(tail, len(out))):
        out[-1-i] = int(out[-1-i]*i/tail)
    return out


def write_wav(path, samples):
    stereo = array.array('h')
    for s in samples:
        stereo.append(s)
        stereo.append(s)
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(44100)
        w.writeframes(stereo.tobytes())


def main():
    pack = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('O:/retro official/Audio/Horror SFX Complete (ogg files).zip')
    WORK.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    lines = []
    with zipfile.ZipFile(pack) as archive:
        members = {name.lower(): name for name in archive.namelist()}
        for index, (name, loop, member, start, length, peak) in enumerate(BANK):
            found = members.get((PACK+member).lower())
            if not found:
                raise SystemExit(f'not in the pack: {member}')
            source = WORK/f'{name}.ogg'
            source.write_bytes(archive.read(found))
            samples = decode(source, start, length)
            if loop:
                samples = level_bed(samples)
                master = WORK/f'{name}.wav'
                write_wav(master, samples)
                out = OUT/f'hsfx_{name}.mp3'
                subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(master), '-ac', '1', '-c:a', 'libmp3lame', '-q:a', '5', str(out)], check=True)
            else:
                samples = fade(level(trim_silence(samples), peak), 64, 2205)
                master = OUT/f'hsfx_{name}.wav'
                write_wav(master, samples)
                out = OUT/f'hsfx_{name}.sfx'
                EncodeEffects.encode(master, out)
                master.unlink()
            seconds = len(samples)/44100
            print(f'{FIRST_RESOURCE+index} {out.name}: {seconds:.2f} s, {out.stat().st_size} bytes')
            lines.append(f'{FIRST_RESOURCE+index} RCDATA "assets/library/audio/{out.name}"')
            source.unlink()
    text = RC.read_text()
    text = re.sub(r'^\d+ RCDATA "assets/library/audio/hsfx_[^"]+"\n', '', text, flags=re.M)
    if not text.endswith('\n'):
        text += '\n'
    with open(RC, 'w', newline='') as file:
        file.write(text+'\n'.join(lines)+'\n')
    print(f'{len(lines)} bank sounds, resources {FIRST_RESOURCE}-{FIRST_RESOURCE+len(lines)-1}')


if __name__ == '__main__':
    main()
