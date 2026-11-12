#!/bin/sh
# Chapter 2 promo v2: re-shoot every take with EXE and rebuild the trailer, stills and board.
#   sh rerender_all.sh [EXE]      (default rm-trailer2.exe; copy a new build in beside it first)
# Order: environments + returns (shots.py, two passes for the tracked ones), the kill in 96
# (film96.py; f96.txt is regenerated from pos96c.log, the sim being deterministic at the anchor),
# the Machine (film-machine.txt), then frames, score, mp4s, contact sheet, stills/key art/board.
set -e
cd "$(dirname "$0")"
EXE=${1:-rm-trailer2.exe}
OUT=N:/rawmetal/docs/Content/Promotional/chapter-2/v2
python shots.py "$EXE"
python film96.py f96.txt pos96c.log
(cd r96 && rm -rf .build/film && "../$EXE" --film=../f96.txt)
(cd mach2 && rm -rf .build/film && "../$EXE" --film=../film-machine.txt)
rm -rf frames && python compose_ch2v2.py frames --jobs=10
python score_ch2v2.py audio/score.wav
python cues_ch2v2.py "$OUT/cue-sheet.txt"
ffmpeg -y -v error -framerate 30 -i frames/f%04d.png -i audio/score.wav -map 0:v -map 1:a -c:v libx264 -preset slow -crf 18 -tune grain -pix_fmt yuv420p -profile:v high -level 4.2 -g 60 -c:a aac -b:a 320k -ar 48000 -shortest -movflags +faststart "$OUT/trailer-1080p.mp4"
ffmpeg -y -v error -i "$OUT/trailer-1080p.mp4" -vf scale=1280:720:flags=lanczos -c:v libx264 -preset slow -crf 23 -pix_fmt yuv420p -c:a aac -b:a 160k -ar 48000 -movflags +faststart "$OUT/trailer-720p-web.mp4"
python contact_ch2v2.py frames "$OUT/trailer-contact-sheet.png"
python make_ch2v2.py "$OUT"
