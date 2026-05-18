#EC build functions

# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#!/bin/bash
COLUMNS=12

function westbuild1723() {
    source ../ecfwwork/zephyr_fork/zephyr-env.sh
    source buildVersion.sh "microchip/$1"
    python3 scripts/generate_config.py  $2/boards/microchip/$1/source/EC_CONFIG.xml $2/boards/microchip/$1/source/board_sys_config.h rom/mec1723/ec_sys_config.bin
    west build -c -p auto -b "$1"
    cp build/zephyr/zephyr.bin rom/mec1723
    cd rom/mec1723
    python3 ./mergboot.py
    ./mec172x_spi_gen_lin_x86_64
    python3 filecopy.py
    cp ec.bin $1.bin
    unset board
}


function westbuild1723onboard() {
    source ../ecfwwork/zephyr_fork/zephyr-env.sh
    source buildVersion.sh "microchip/$1"
    python3 scripts/generate_config.py  $2/source/EC_CONFIG.xml $2/source/board_sys_config.h rom/mec1723/ec_sys_config.bin
    west build -c -p auto -b "$1"
    cp build/zephyr/zephyr.bin rom/mec1723
    cd rom/mec1723
    python3 ./mergboot.py
    ./mec172x_spi_gen_lin_x86_64
    python3 filecopy.py
    cp ec.bin $2/build/$1.bin
    unset board
}

function westbuild1723onserver() {
    source ../ecfwwork/zephyr_fork/zephyr-env.sh
    python3 scripts/generate_config.py  $2/source/EC_CONFIG.xml $2/source/board_sys_config.h rom/mec1723/ec_sys_config.bin
    west build -c -p auto -b "$1"
    cp build/zephyr/zephyr.bin rom/mec1723
    cd rom/mec1723
    python3 ./mergboot.py
    ./mec172x_spi_gen_lin_x86_64
    python3 filecopy.py
    cp ec.bin $2/build/$1.bin
    unset board
}

function westbuildnpcx4mnxonserver() {
    source ../ecfwwork/zephyr_fork/zephyr-env.sh
    source buildVersion.sh "nuvoton/$1"
    python3 scripts/generate_config.py  $2/source/EC_CONFIG.xml $2/source/board_sys_config.h rom/nuvoton/ec_sys_config.bin
    west build -c -p auto -b "$1"
    cp build/zephyr/zephyr.bin rom/nuvoton
    cd rom/nuvoton
    python3 ./mergboot.py
    python3 ecst.py -i zephyr_boot.bin -o ec_external.bin -chip npcx4m8 -flashsize 16 -spimaxclk 50 -spireadmode "quad"
    cp ec_external.bin $1.bin
    unset board
}

function westbuildnpcx4mnx() {
    source ../ecfwwork/zephyr_fork/zephyr-env.sh
    source buildVersion.sh "nuvoton/$1"
    python3 scripts/generate_config.py  $2/boards/nuvoton/$1/source/EC_CONFIG.xml $2/boards/nuvoton/$1/source/board_sys_config.h rom/nuvoton/ec_sys_config.bin
    west build -c -p auto -b "$1"
    cp build/zephyr/zephyr.bin rom/nuvoton
    cd rom/nuvoton
    python3 ./mergboot.py
    python3 ecst.py -i zephyr_boot.bin -o ec_external.bin -chip npcx4m8 -flashsize 16 -spimaxclk 50 -spireadmode "fast"
    python3 ecst.py -i zephyr_boot.bin -o ec_internal.bin -chip npcx4m8 -flashsize 16 -spimaxclk 50 -spireadmode "dual"
    cp ec_external.bin $1.bin
    unset board
}

function westbuildnpcx4mnxboot() {
    source ../ecfwwork/zephyr_fork/zephyr-env.sh
    # source buildVersion.sh "nuvoton/$1"
    # python3 scripts/generate_config.py  $2/boards/nuvoton/$1/source/EC_CONFIG.xml $2/boards/nuvoton/$1/source/board_sys_config.h rom/nuvoton/ec_sys_config.bin
    west build -c -p auto -b "$1" boot
    cp build/zephyr/zephyr.bin rom/nuvoton/boot_nuvton
    unset board
}
function westclean {
  clear
    west build -t clean
}

