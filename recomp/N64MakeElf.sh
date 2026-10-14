#!/bin/bash

# Set the basename from the input file without the extension
BASENAME=$(basename "$1" .z64)

# Define directories
BUILD_DIR=build
ASM_DIR=asm
BIN_DIR=assets
SRC_DIR=src
TOOLS_DIR=tools

# Define toolchain
# Fedora only ships the mips64 cross toolchain; it works for 32-bit/o32 when ld is given -m elf32btsmip
CROSS=mips64-linux-gnu-
LD_EMUL="-m elf32btsmip"
AS=${CROSS}as
LD=${CROSS}ld
OBJCOPY=${CROSS}objcopy
PYTHON=python3

# Define flags
OBJCOPYFLAGS="-O binary"
LDFLAGS="${LD_EMUL} -T ${BASENAME}.ld -Map ${BUILD_DIR}/${BASENAME}.map -T ${BUILD_DIR}/undefined_syms_auto.provide.txt -T ${BUILD_DIR}/undefined_funcs_auto.provide.txt -T undefined_syms_manual.txt --no-check-sections"

# Define compiler and assembler flags
CC=${TOOLS_DIR}/ido_recomp/linux/5.3/cc
OPT_FLAGS="-O2"
MIPSISET="-mips1 -o32"
INCLUDE_CFLAGS="-I . -I include"
ASFLAGS="-EB -mtune=vr4300 -march=vr4300 -mabi=32 -I include"

# Function to convert .bin to .o
convert_bin_to_o() {
    for bin_file in $(find ${BIN_DIR} -name "*.bin"); do
        bin_base=$(basename ${bin_file} .bin)
        mkdir -p $(dirname ${BUILD_DIR}/${bin_file})
        ${LD} ${LD_EMUL} -r -b binary -o ${BUILD_DIR}/${bin_file%.*}.o ${bin_file}
        if [ $? -ne 0 ]; then
            echo "Error: objcopy failed for ${bin_file}"
            exit 1
        fi
    done
}

# Function to assemble .s files to .o files
assemble_s_files() {
    for asm_file in $(find ${ASM_DIR} -name "*.s"); do
        asm_base=$(basename ${asm_file} .s)
        mkdir -p $(dirname ${BUILD_DIR}/${asm_file})
        ${AS} ${ASFLAGS} -o ${BUILD_DIR}/${asm_file%.*}.o ${asm_file}
        if [ $? -ne 0 ]; then
            echo "Error: assembler failed for ${asm_file}"
            exit 1
        fi
    done
}

# Function to compile .c files to .o files, if src directory exists
compile_c_files() {
    if [ -d ${SRC_DIR} ]; then
        for c_file in $(find ${SRC_DIR} -name "*.c"); do
            c_base=$(basename ${c_file} .c)
            mkdir -p $(dirname ${BUILD_DIR}/${c_file})
            ${CC} -c ${CFLAGS} ${OPT_FLAGS} ${MIPSISET} -o ${BUILD_DIR}/${c_file%.*}.o ${c_file}
            if [ $? -ne 0 ]; then
                echo "Error: compiler failed for ${c_file}"
                exit 1
            fi
        done
    fi
}

# Clean the build directory
clean_build() {
    rm -rf ${BUILD_DIR}
}

# Main function
main() {
    # Clean previous build
    clean_build

    # Create necessary directories
    mkdir -p ${BUILD_DIR}/${ASM_DIR} ${BUILD_DIR}/${BIN_DIR}

    # Convert .bin to .o
    convert_bin_to_o

    # Assemble .s files to .o files
    assemble_s_files

    # Compile .c files to .o files
    compile_c_files

    # splat lists some symbols as undefined even though another segment defines them (it splits
    # segments one at a time). A plain assignment in a linker script overrides the real definition
    # with an absolute symbol, so wrap the auto lists in PROVIDE(), which only applies when
    # nothing else defines the symbol.
    for f in undefined_syms_auto undefined_funcs_auto; do
        sed -E 's/^([^ ]+) = (.*);$/PROVIDE(\1 = \2);/' $f.txt > ${BUILD_DIR}/$f.provide.txt
    done

    # Link all .o files to create the ELF file
    ${LD} ${LDFLAGS} -o ${BUILD_DIR}/${BASENAME}.elf $(find ${BUILD_DIR} -name "*.o")

    if [ ! -f ${BUILD_DIR}/${BASENAME}.elf ]; then
        echo "Error: ELF file not created"
        exit 1
    fi

    # Convert the ELF file to binary
    ${OBJCOPY} ${OBJCOPYFLAGS} ${BUILD_DIR}/${BASENAME}.elf ${BUILD_DIR}/${BASENAME}.bin

    if [ ! -f ${BUILD_DIR}/${BASENAME}.bin ]; then
        echo "Error: Binary file not created"
        exit 1
    fi

    # Copy the binary file to .z64
    cp ${BUILD_DIR}/${BASENAME}.bin ${BUILD_DIR}/${BASENAME}.z64

    echo "Build completed: ${BUILD_DIR}/${BASENAME}.z64"
}

# Check if the input file is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <path-to-.z64-file>"
    exit 1
fi

# Run the main function
main "$1"
