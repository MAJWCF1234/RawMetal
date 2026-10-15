"""Generate cached ElevenLabs masters and bake the NPC voice bank.

Credentials are read only at generation time from ELEVENLABS_API_KEY or an
explicit private --key-file. They are never written into assets or logs.
Running without --generate only rebakes existing local masters.
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
MODEL = 'eleven_multilingual_v2'
SETTINGS = {'stability': .5, 'similarity_boost': .75, 'style': .25, 'use_speaker_boost': True}
PROFILES = {1: (280, 2900, 1.35, 12, 2), 2: None,
            3: (260, 2600, 1.2, 13, 2), 4: (150, 2100, 1.6, 11, 3),
            5: (260, 3000, 1.15, 13, 1), 6: (140, 4400, 1.1, 14, 1),
            7: (180, 3500, 1.15, 13, 1)}


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

    def bake(entry):
        identity = {'voice_id': entry['voice_id'], 'text': entry['text'],
                    'model_id': MODEL, 'voice_settings': SETTINGS}
        digest = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()
        master = args.masters / (digest + '.mp3')
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
        profile = PROFILES[entry['role']]
        if profile:
            low, high, drive, bits, hold = profile
            samples = sosfilt(butter(3, [low, high], btype='bandpass', fs=22050, output='sos'), samples)
            samples = np.tanh(samples * drive) / np.tanh(drive)
            if hold > 1:
                # Filter before decimation; sample hold provides restrained radio grit.
                coarse = resample_poly(samples, 1, hold)
                samples = np.repeat(coarse, hold)[:len(samples)]
            samples = np.round(samples * (2**(bits-1))) / (2**(bits-1))
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
                  'wav_bytes': wav.stat().st_size, 'masked': profile is not None}
        print('Ready:', entry['id'], result['seconds'], 'seconds', flush=True)
        return wav.read_bytes(), result

    # Two concurrent calls stay within the account's normal request capacity.
    with ThreadPoolExecutor(max_workers=2) as pool:
        results = list(pool.map(bake, catalog['clips']))
    bank = b'RVB1' + struct.pack('<I', len(results))
    for data, _ in results:
        bank += struct.pack('<I', len(data)) + data
    target = ROOT / 'src/assets/cleanup-voices.bank'
    target.write_bytes(bank)
    cues = catalog['cues']
    header = ['#pragma once', '#include <array>', 'namespace retro {',
              'enum class NpcVoiceCue {' + ','.join(cues) + '};',
              'struct NpcVoiceClip {int role; NpcVoiceCue cue; float seconds;};',
              f'inline constexpr std::array<NpcVoiceClip,{len(results)}> NpcVoiceClips={{{{']
    for entry, (_, proof) in zip(catalog['clips'], results):
        header.append(f' {{{entry["role"]},NpcVoiceCue::{entry["cue"]},{proof["seconds"]:.4f}f}},')
    header += ['}};', '}']
    (ROOT / 'src/audio/NpcVoiceCatalog.h').write_text('\n'.join(header)+'\n')
    provenance = {'provider': 'ElevenLabs', 'model': MODEL, 'settings': SETTINGS,
                  'bank_bytes': len(bank), 'bank_sha256': hashlib.sha256(bank).hexdigest(),
                  'clips': [proof for _, proof in results]}
    (catalog_path.parent / 'provenance.json').write_text(json.dumps(provenance, indent=2)+'\n')
    print('Baked', len(results), 'clips;', len(bank), 'bytes')


if __name__ == '__main__':
    main()
