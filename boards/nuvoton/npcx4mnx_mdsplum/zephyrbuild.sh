#;*****************************************************************************
#; * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#; ******************************************************************************
#; */


#!/bin/bash
if [ ! -d "build" ];then
mkdir build
fi

export current_path=$PWD
cd ../../../
. ./zephyrmenubuild.sh
export board=npcx4mnx_mdsplum
if [ T"$1" == T"server" ];then
echo "This is server build"
west update
cd ../ecfwwork/zephyr_fork
git am ../../ecfw-zephyr/zephyr_patches/0001-AMD-kernel-V3_2.patch
cd ../../ecfw-zephyr
cd ./boards/nuvoton/$board/
. ./gen_ioexptable.sh
. ./gen_nativegpio.sh
cd ../../..
westbuildnpcx4mnxonserver $board $current_path
elif [ T"$1" == T"" ];then
echo "This is local build, if you want to build on server please use build server"
cd ./boards/nuvoton/$board/
. ./gen_ioexptable.sh
. ./gen_nativegpio.sh
cd ../../..
westbuildnpcx4mnx $board $PWD
else
echo "error please use right arg, if you want to build on server please use build server"
fi