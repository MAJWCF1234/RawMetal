# Hard ceiling: Legend of Zelda Ocarina of Time N64 cartridge (32 MiB).
# Above this and the size constraint that defines the project is void.
file(SIZE "${RAWMetalExecutable}" RAWMETAL_BYTES)
if(RAWMETAL_BYTES GREATER_EQUAL 33554432)
    message(FATAL_ERROR "RawMetal is ${RAWMETAL_BYTES} bytes; it must stay below Ocarina of Time (32 MiB / 33,554,432 bytes).")
endif()
message(STATUS "RawMetal executable: ${RAWMETAL_BYTES} / 33,554,432 bytes (OoT 32 MiB)")
