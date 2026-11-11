# Hard ceiling: 72 MiB (75,497,472 bytes). The project began at the Ocarina of
# Time cartridge (32 MiB); N64 cartridges shipped up to 64 MiB (Resident Evil
# 2, Conker's Bad Fur Day). The ceiling was raised to 40 MiB on 2026-10-24 for
# the Ashfall Exclusion Zone overhaul, to the full 64 MiB cartridge on
# 2026-10-25 for the research, mining and Dark Below chapters, and to 72 MiB on
# 2026-11-10 for the sound bank and Chapter 2's soundscape.
file(SIZE "${RAWMetalExecutable}" RAWMETAL_BYTES)
if(RAWMETAL_BYTES GREATER_EQUAL 75497472)
    message(FATAL_ERROR "RawMetal is ${RAWMETAL_BYTES} bytes; it must stay below 72 MiB (75,497,472 bytes).")
endif()
math(EXPR RAWMETAL_HEADROOM "75497472 - ${RAWMETAL_BYTES}")
message(STATUS "RawMetal executable: ${RAWMETAL_BYTES} / 75,497,472 bytes; headroom ${RAWMETAL_HEADROOM} bytes (72 MiB)")
# Keep normal content growth visible before it reaches the hard ceiling.
if(RAWMETAL_HEADROOM LESS 6291456)
    message(WARNING "RawMetal has less than 6 MiB of growth reserve. Review packed/compression-report.txt before adding large assets.")
endif()
