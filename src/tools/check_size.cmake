# Hard ceiling: 40 MiB (41,943,040 bytes). The project began at the Ocarina of
# Time cartridge (32 MiB); N64 cartridges shipped up to 64 MiB (Resident Evil
# 2, Conker's Bad Fur Day), and the ceiling was raised to 40 MiB on 2026-10-24
# for the Ashfall Exclusion Zone overhaul.
file(SIZE "${RAWMetalExecutable}" RAWMETAL_BYTES)
if(RAWMETAL_BYTES GREATER_EQUAL 41943040)
    message(FATAL_ERROR "RawMetal is ${RAWMETAL_BYTES} bytes; it must stay below 40 MiB (41,943,040 bytes).")
endif()
math(EXPR RAWMETAL_HEADROOM "41943040 - ${RAWMETAL_BYTES}")
message(STATUS "RawMetal executable: ${RAWMETAL_BYTES} / 41,943,040 bytes; headroom ${RAWMETAL_HEADROOM} bytes (40 MiB)")
# Keep normal content growth visible before it reaches the hard ceiling.
if(RAWMETAL_HEADROOM LESS 6291456)
    message(WARNING "RawMetal has less than 6 MiB of growth reserve. Review packed/compression-report.txt before adding large assets.")
endif()
