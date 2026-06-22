idf_component_register(SRCS "main.cpp" "am-duong-lich.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES arduino PxMatrix RTClib Time Adafruit-GFX-Library)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=maybe-uninitialized -Wno-error=format-truncation)
