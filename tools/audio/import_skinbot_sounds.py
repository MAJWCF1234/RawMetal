"""Edit the Skinbot Foley and synthesize its low motor and drive engagement.

Run from any directory. Writes 44.1 kHz mono masters and RMS1 game samples.
"""
import array
import math
import wave

from import_mac11_sounds import EncodeEffects, OUT, decode

RATE = 44100
CLIPS = (
    ("skinbot-blade", "Character/Metal Clink_4.ogg", .23, .76, .045, 130, 5400),
    ("skinbot-joint", "House & Office/Home or Office/Switch_clicking_2.ogg", .13, .57, .032, 360, 7200),
    ("skinbot-hit", "Ambient/Metal_struck_3.ogg", .57, .82, .15, 67, 5200),
    ("skinbot-scrape", "House & Office/Home or Office/Metal scrape_scratch_2.ogg", .38, .62, .075, 175, 6200),
)


def shape(pcm, low, high):
    low_coeff = 1 - math.exp(-math.tau * low / RATE)
    high_coeff = 1 - math.exp(-math.tau * high / RATE)
    bass = rolloff = 0.0
    out = []
    for sample in pcm:
        bass += (sample - bass) * low_coeff
        rolloff += (sample - bass - rolloff) * high_coeff
        out.append(rolloff)
    return out


def prepare(name, source, seconds, peak, fade, low, high):
    source = decode("zip:Horror SFX Complete OGG/" + source)
    threshold = max(abs(value) for value in source) * .075
    start = max(0, next(i for i, value in enumerate(source) if abs(value) >= threshold) - 65)
    clip = shape(source[start:start + int(seconds * RATE)], low, high)
    write_sample(name, clip, peak, fade)


def write_sample(name, clip, peak, fade=0):
    gain = peak * 32767 / max(abs(value) for value in clip)
    fade_frames = max(1, int(fade * RATE))
    samples = array.array("h")
    for i, sample in enumerate(clip):
        envelope = min(1, i / 44, (len(clip) - 1 - i) / fade_frames) if fade else 1
        samples.append(round(sample * gain * envelope))
    wav = OUT / (name + ".wav")
    with wave.open(str(wav), "wb") as master:
        master.setnchannels(1)
        master.setsampwidth(2)
        master.setframerate(RATE)
        master.writeframes(samples.tobytes())
    frames, packed = EncodeEffects.encode(wav, OUT / (name + ".sfx"))
    print(f"{name}: {frames / RATE:.3f} s, {packed + 12} encoded bytes")


def write_drive():
    # Low coupled tones with a slow torque pulse. Integer cycles keep the loop continuous.
    motor = []
    for frame in range(2 * RATE):
        t = frame / RATE
        pulse = .57 + .43 * (.5 + .5 * math.cos(math.tau * 2.5 * t)) ** 2
        phase = math.tau * 72 * t + .21 * math.sin(math.tau * 2.5 * t)
        motor.append(pulse * (.62 * math.sin(phase) + .21 * math.sin(2 * phase + .4)
                              + .1 * math.sin(math.tau * 108 * t) + .07 * math.sin(3 * phase)))
    write_sample("skinbot-motor", motor, .55)

    # A weighted engagement and short release, with the pitch dropping as the gear takes load.
    surge = []
    for frame in range(int(.38 * RATE)):
        t = frame / RATE
        phase = math.tau * (76 * t + 60 * .1 * (1 - math.exp(-t / .1)))
        housing = math.exp(-t * 31) * math.sin(math.tau * 338 * t)
        load = (1 - math.exp(-t * 52)) * math.exp(-t * 8)
        surge.append(.36 * housing + load * (.63 * math.sin(phase) + .29 * math.sin(2 * phase + .5)))
    write_sample("skinbot-surge", surge, .54, .09)


if __name__ == "__main__":
    for clip in CLIPS:
        prepare(*clip)
    write_drive()
