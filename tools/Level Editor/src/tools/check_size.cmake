# Hard ceiling: 95 MiB (99,614,720 bytes). The project began at the Ocarina of
# Time cartridge (32 MiB); N64 cartridges shipped up to 64 MiB (Resident Evil
# 2, Conker's Bad Fur Day). The ceiling was raised to 40 MiB on 2026-10-24 for
# the Ashfall Exclusion Zone overhaul, to the full 64 MiB cartridge on
# 2026-10-25 for the research, mining and Dark Below chapters, to 72 MiB on
# 2026-11-10 for the sound bank and Chapter 2's soundscape, to 88 MiB on
# 2026-11-17 for the paid Brutal sky catalog (47 neural panoramas at the
# legacy 2048x512 wasteland resolution), and to 95 MiB on 2026-11-19 for
# multiplayer (the packed EOS SDK and XAudio), the Limitator and the MAC-11.
file(SIZE "${RAWMetalExecutable}" RAWMETAL_BYTES)
if(RAWMETAL_BYTES GREATER_EQUAL 99614720)
    message(FATAL_ERROR "RawMetal is ${RAWMETAL_BYTES} bytes; it must stay below 95 MiB (99,614,720 bytes).")
endif()
math(EXPR RAWMETAL_HEADROOM "99614720 - ${RAWMETAL_BYTES}")
message(STATUS "RawMetal executable: ${RAWMETAL_BYTES} / 99,614,720 bytes; headroom ${RAWMETAL_HEADROOM} bytes (95 MiB)")
# Keep normal content growth visible before it reaches the hard ceiling.
if(RAWMETAL_HEADROOM LESS 6291456)
    message(WARNING "RawMetal has less than 6 MiB of growth reserve. Review packed/compression-report.txt before adding large assets.")
endif()
