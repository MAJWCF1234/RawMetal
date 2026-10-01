# Freight soundtrack

`freight-source.ogg` is copied unchanged from `O:/retro official/Echoes - Audio Super Kit v2.1.zip`:
`Echoes - Audio Super Kit/Audio/Soundtracks/pressure (loop).ogg`.

The embedded `freight.mp3` (resource 271) is a 44.1 kHz stereo, 128 kbps MP3
conversion for the existing decoder and single-executable distribution:

```text
ffmpeg -i freight-source.ogg -ar 44100 -ac 2 -c:a libmp3lame -b:a 128k freight.mp3
```

This audio conversion is lossy; the original OGG is preserved. Resource packing
round-trips the embedded MP3 exactly. The mixer crossfades loop boundaries and
maintains the track cursor across stitched maps. All 22 freight chunks author
MusicCue::Freight; default maps retain their original music and reactor routing.
