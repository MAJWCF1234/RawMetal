"""Generate cached ElevenLabs masters and bake the NPC voice bank.

Credentials are read only at generation time from ELEVENLABS_API_KEY or an
explicit private --key-file. They are never written into assets or logs.
Running without --generate only rebakes existing local masters. Clips already
in the shipped bank whose masters are not cached here are carried over as they
are, so adding lines never regenerates (or changes) the existing voices.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import urllib.request
import urllib.error
from concurrent.futures import ThreadPoolExecutor

import numpy as np
from scipy.signal import butter, sosfilt, resample_poly

ROOT = Path(__file__).resolve().parents[1]
# Eleven v3 reads the bracketed audio tags in each line ([shouting], [whispers],
# [grunts]...) as performance direction rather than words. Its stability is
# one of 0 (creative), .5 (natural) or 1 (robust).
MODEL = 'eleven_v3'
SETTINGS = {'stability': .5, 'similarity_boost': .75}
# What a speaker wears over the mouth is applied live by the audio engine from
# the speaker's model (bare face, face plate, respirator, sealed hood). The bake
# only adds the radio channel the squad roles and checkpoint soldiers call over.
RADIO = {1, 3, 4, 6, 8}


def radio_channel(samples, rate=22050):
    """Squad radio: narrow band, driven, light bit-crush and sample hold."""
    samples = sosfilt(butter(4, [380, 3000], btype='bandpass', fs=rate, output='sos'), samples)
    peak = max(np.max(np.abs(samples)), 1e-6)
    samples = np.tanh(samples / peak * 2.0) / np.tanh(2.0)
    coarse = resample_poly(samples, 1, 2)
    samples = np.repeat(coarse, 2)[:len(samples)]
    return np.round(samples * 2048) / 2048


def master_voice(samples, rate=22050):
    """Mastering shared by every role: rumble high-pass, a gentle compressor
    so whispers and shouts sit at one level in the mix, and a soft limiter."""
    samples = sosfilt(butter(2, 75, btype='highpass', fs=rate, output='sos'), samples)
    # Envelope follower: 5 ms attack, 120 ms release; 3:1 above -20 dBFS.
    attack, release = np.exp(-1 / (.005 * rate)), np.exp(-1 / (.12 * rate))
    level = np.abs(samples)
    envelope = np.empty_like(level)
    current = 0.0
    for i, value in enumerate(level):
        coeff = attack if value > current else release
        current = coeff * current + (1 - coeff) * value
        envelope[i] = current
    threshold = 10 ** (-20 / 20)
    over = np.maximum(envelope / threshold, 1.0)
    samples = samples * over ** (1 / 3 - 1)
    return np.tanh(samples * 1.6) / np.tanh(1.6)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generate', action='store_true')
    parser.add_argument('--key-file', type=Path)
    parser.add_argument('--masters', type=Path, default=ROOT / '.build/npc-voice-masters')
    args = parser.parse_args()
    catalog_path = ROOT / 'source-assets/cleanup-voices/catalog.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    key = os.environ.get('ELEVENLABS_API_KEY', '')
    if args.generate and not key and args.key_file:
        lines = args.key_file.read_text().splitlines()
        candidates = []
        for i, line in enumerate(lines):
            if 'eleven' in line.lower():
                candidates += re.findall(r'(?:sk_)?[A-Za-z0-9_-]{30,}', line)
                if i + 1 < len(lines):
                    candidates += re.findall(r'(?:sk_)?[A-Za-z0-9_-]{30,}', lines[i + 1])
        candidates = list(dict.fromkeys(candidates))
        if len(candidates) != 1:
            raise SystemExit('Expected exactly one explicitly labelled ElevenLabs key')
        key = candidates[0]
    if args.generate and not key:
        raise SystemExit('Set ELEVENLABS_API_KEY or provide --key-file privately')
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        raise SystemExit('ffmpeg is required for existing supported IMA ADPCM WAV output')
    args.masters.mkdir(parents=True, exist_ok=True)
    processed = args.masters / 'processed'
    processed.mkdir(exist_ok=True)

    # The shipped bank, for clips whose masters are not cached locally.
    shipped, shipped_proof = [], []
    bank_path = ROOT / 'src/assets/cleanup-voices.bank'
    provenance_path = catalog_path.parent / 'provenance.json'
    if bank_path.exists() and provenance_path.exists():
        data = bank_path.read_bytes()
        count = struct.unpack_from('<I', data, 4)[0]
        offset = 8
        for _ in range(count):
            size = struct.unpack_from('<I', data, offset)[0]
            shipped.append(data[offset + 4:offset + 4 + size])
            offset += 4 + size
        shipped_proof = json.loads(provenance_path.read_text())['clips']

    def bake(indexed):
        index, entry = indexed
        identity = {'voice_id': entry['voice_id'], 'text': entry['text'],
                    'model_id': MODEL, 'voice_settings': SETTINGS}
        digest = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
        master = args.masters / (digest + '.mp3')
        if not master.exists() and index < len(shipped) and shipped_proof[index]['id'] == entry['id']                 and shipped_proof[index].get('generation_identity') == digest:
            return shipped[index], shipped_proof[index]
        if not master.exists():
            if not args.generate:
                raise RuntimeError('Missing cached master: ' + entry['id'])
            body = {k: v for k, v in identity.items() if k != 'voice_id'}
            request = urllib.request.Request(
                'https://api.elevenlabs.io/v1/text-to-speech/' + entry['voice_id'] + '?output_format=mp3_44100_128',
                data=json.dumps(body).encode(), headers={'xi-api-key': key, 'Content-Type': 'application/json'})
            try:
                with urllib.request.urlopen(request, timeout=120) as response:
                    data = response.read()
            except urllib.error.HTTPError as error:
                # Do not print request objects, headers, or private service payloads.
                raise RuntimeError(f'ElevenLabs HTTP {error.code} for {entry["id"]}; no automatic paid retry') from None
            if len(data) < 200:
                raise RuntimeError('Truncated generation: ' + entry['id'])
            temporary = master.with_suffix('.tmp')
            temporary.write_bytes(data)
            temporary.replace(master)
        decoded = subprocess.run([ffmpeg, '-v', 'error', '-i', str(master), '-f', 'f32le', '-ac', '1',
                                  '-ar', '22050', '-'], capture_output=True, check=True).stdout
        samples = np.frombuffer(decoded, dtype='<f4').copy()
        if not 0.2 < len(samples) / 22050 < 12 or np.max(np.abs(samples)) < .005:
            raise RuntimeError('Silent or implausible duration: ' + entry['id'])
        # Preserve delivery and timing; only trim extended encoder lead/tail silence.
        active = np.flatnonzero(np.abs(samples) > .008)
        samples = samples[max(0, active[0]-1102):min(len(samples), active[-1]+2205)]
        if entry['role'] in RADIO:
            samples = radio_channel(samples)
        samples = master_voice(samples)
        rms = np.sqrt(np.mean(samples*samples))
        samples *= min(.15 / max(rms, 1e-6), .88 / max(np.max(np.abs(samples)), 1e-6))
        fade = min(110, len(samples)//2)
        samples[:fade] *= np.linspace(0, 1, fade)
        samples[-fade:] *= np.linspace(1, 0, fade)
        wav = processed / (entry['id'] + '.wav')
        subprocess.run([ffmpeg, '-v', 'error', '-y', '-f', 'f32le', '-ar', '22050', '-ac', '1', '-i', '-',
                        '-c:a', 'adpcm_ima_wav', str(wav)], input=np.asarray(samples, dtype='<f4').tobytes(), check=True)
        result = {'id': entry['id'], 'master_sha256': hashlib.sha256(master.read_bytes()).hexdigest(),
                  'generation_identity': digest, 'seconds': round(len(samples)/22050, 4),
                  'wav_bytes': wav.stat().st_size, 'radio': entry['role'] in RADIO}
        print('Ready:', entry['id'], result['seconds'], 'seconds', flush=True)
        return wav.read_bytes(), result

    # Two concurrent calls stay within the account's normal request capacity.
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(bake, enumerate(catalog['clips'])))
    bank = b'RVB1' + struct.pack('<I', len(results))
    for data, _ in results:
        bank += struct.pack('<I', len(data)) + data
    target = ROOT / 'src/assets/cleanup-voices.bank'
    target.write_bytes(bank)
    cues = catalog['cues']
    header = ['#pragma once', '#include <array>', 'namespace retro {',
              'enum class NpcVoiceCue {' + ','.join(cues) + '};',
              'struct NpcVoiceClip {int role; NpcVoiceCue cue; float seconds; int stance=0; int mood=0; const char* line="";};',
              f'inline constexpr std::array<NpcVoiceClip,{len(results)}> NpcVoiceClips={{{{']
    for entry, (_, proof) in zip(catalog['clips'], results):
        # stance (1 hostile, 2 truce, 3 ally), mood (1 steady, 2 shaken, 3 broken) and a dialogue line key are optional.
        header.append(f' {{{entry["role"]},NpcVoiceCue::{entry["cue"]},{proof["seconds"]:.4f}f,{entry.get("stance",0)},{entry.get("mood",0)},"{entry.get("line","")}"}},')
    header += ['}};', '}']
    (ROOT / 'src/audio/NpcVoiceCatalog.h').write_text('\n'.join(header)+'\n')
    provenance = {'provider': 'ElevenLabs', 'model': MODEL, 'settings': SETTINGS,
                  'bank_bytes': len(bank), 'bank_sha256': hashlib.sha256(bank).hexdigest(),
                  'clips': [proof for _, proof in results]}
    (catalog_path.parent / 'provenance.json').write_text(json.dumps(provenance, indent=2)+'\n')
    print('Baked', len(results), 'clips;', len(bank), 'bytes')


if __name__ == '__main__':
    main()
