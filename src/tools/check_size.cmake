# Hard ceiling: Legend of Zelda Ocarina of Time N64 cartridge (32 MiB).
# Above this and the size constraint that defines the project is void.
file(SIZE "${RAWMetalExecutable}" RAWMETAL_BYTES)
if(RAWMETAL_BYTES GREATER_EQUAL 33554432)
    message(FATAL_ERROR "RawMetal is ${RAWMETAL_BYTES} bytes; it must stay below Ocarina of Time (32 MiB / 33,554,432 bytes).")
endif()
math(EXPR RAWMETAL_HEADROOM "33554432 - ${RAWMETAL_BYTES}")
message(STATUS "RawMetal executable: ${RAWMETAL_BYTES} / 33,554,432 bytes; headroom ${RAWMETAL_HEADROOM} bytes (OoT 32 MiB)")
# Keep normal content growth visible before it reaches the hard ceiling.
if(RAWMETAL_HEADROOM LESS 6291456)
    message(WARNING "RawMetal has less than 6 MiB of growth reserve. Review packed/compression-report.txt before adding large assets.")
endif()
