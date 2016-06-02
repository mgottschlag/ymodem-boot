
cmake_minimum_required(VERSION 2.6)
project(ymodem-boot C ASM)

set(TARGET "at91sam9" CACHE STRING "Target board (at91sam9, ar9331)")

if(${TARGET} MATCHES "at91sam9")
	set(PREFIX arm-none-eabi-)
elseif(${TARGET} MATCHES "ar9331")
	set(PREFIX mips-unknown-elf-)
else()
	message(FATAL "Unknown target.")
endif()

set(CMAKE_AR "${PREFIX}gcc-ar")
set(CMAKE_RANLIB "${PREFIX}gcc-ranlib")
set(CMAKE_C_COMPILER ${PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${PREFIX}gcc)
set(CMAKE_STRIP ${PREFIX}strip)
set(CMAKE_OBJCOPY ${PREFIX}objcopy)
set(SIZE_CMD ${PREFIX}size)

# disable rdynamic
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

# build a freestanding binary
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib -nostdinc -ffreestanding")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -nostdlib -nostdinc -ffreestanding")

# enable warnings and asserts
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -Wall -Wextra")

# optimization
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -flto")

# common source code
set(SRC
	main.c
	stdint.h
	ymodem.c
	ymodem_boot.h
)

# target-specific sources and options
if(${TARGET} MATCHES "at91sam9")
	set(SRC
		${SRC}
		at91sam9/start.s
		at91sam9/at91sam9.c
	)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv5te -msoft-float -marm -mfpu=vfp")
	add_definitions(-DLITTLE_ENDIAN)
elseif(${TARGET} MATCHES "ar9331")
	set(SRC
		${SRC}
		ar9331/start.s
		ar9331/ar9331.c
	)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=mips32 -msoft-float -G 0 -EB")
	set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -march=mips32 -msoft-float -EB")
endif()
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}/linker.ld")

add_executable(ymodem-boot ${SRC} ${LINKER_SCRIPT})
set_target_properties(ymodem-boot PROPERTIES LINK_FLAGS "-T ${LINKER_SCRIPT}")
target_link_libraries(ymodem-boot gcc)
set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/ymodem-boot")

# show the size of the executable
add_custom_command(
	TARGET ymodem-boot
	POST_BUILD
	COMMAND ${SIZE_CMD} ${OUTPUT_FILE}
)

# generate uImage
if(${TARGET} MATCHES "at91sam9")
	set(UIMAGE_ARCH "arm")
	set(ADDRESS "21100000")
elseif(${TARGET} MATCHES "ar9331")
	set(UIMAGE_ARCH "mips")
	set(ADDRESS "80060000")
endif()
add_definitions(-DADDRESS=0x${ADDRESS})

# uboot cannot load uimages reliably, so better use "loadelf" to load the ELF
# binary on AR9331 systems
if(${TARGET} MATCHES "at91sam9")
	add_custom_command(
		TARGET ymodem-boot
		POST_BUILD
		COMMAND ${CMAKE_OBJCOPY} -O binary ${OUTPUT_FILE} ${OUTPUT_FILE}.bin &&
			mkimage -A ${UIMAGE_ARCH} -O linux -T standalone -C none
				-a ${ADDRESS} -e ${ADDRESS} -n ymodem-boot
				-d ${OUTPUT_FILE}.bin
				${OUTPUT_FILE}.uImage
	)
endif()
