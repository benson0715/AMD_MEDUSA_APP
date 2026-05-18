#;*****************************************************************************
#;
#; Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#;
#;******************************************************************************

#!/bin/bash
if [ ! -d "build" ];then
mkdir build
fi

export current_path=$PWD


cd ../../
####################################copy CMakelist####################################################
cp drivers/nuvoton/CMakeLists.txt  external_lib/script/build/CMakeLists.txt.nuvoton
cp external_lib/driver/nuvoton/CMakeLists.txt drivers/nuvoton/CMakeLists.txt

cp app/CMakeLists.txt  external_lib/script/build/CMakeLists.txt.app
cp external_lib/app/CMakeLists.txt app/CMakeLists.txt

cp drivers/CMakeLists.txt  external_lib/script/build/CMakeLists.txt.drivers
cp external_lib/driver/CMakeLists.txt drivers/CMakeLists.txt
####################################copy lib from build folder####################################################

cp ./build/app/libapp.a  libamd/lib/libamd.a


####################################remove c code to build####################################################

cp app/usbc/app_4cc.c  external_lib/script/build/app_4cc.c
cp app/usbc/app_ccgx.c external_lib/script/build/app_ccgx.c 

cp drivers/nuvoton/acpi_npcx4mnx.c  external_lib/script/build/acpi_npcx4mnx.c 
cp drivers/nuvoton/gpio_npcx4mnx.c   external_lib/script/build/gpio_npcx4mnx.c 
cp drivers/nuvoton/nuvoton_espi.c   external_lib/script/build/nuvoton_espi.c
cp drivers/nuvoton/nuvoton_gpio_ctrl.c  external_lib/script/build/nuvoton_gpio_ctrl.c

cp drivers/amd_crb_drivers_tps6599x.c  external_lib/script/build/amd_crb_drivers_tps6599x.c 
cp drivers/amd_crb_drivers_tps6699x.c  external_lib/script/build/amd_crb_drivers_tps6699x.c
cp drivers/amd_crb_drivers_ucsi.c  external_lib/script/build/amd_crb_drivers_ucsi.c 
cp drivers/amd_crb_drivers_hpi.c  external_lib/script/build/amd_crb_drivers_hpi.c


cp drivers/dev_mp2845_SVI3.c  external_lib/script/build/dev_mp2845_SVI3.c 
cp drivers/dev_isl9241.c  external_lib/script/build/dev_isl9241.c
cp drivers/dev_mp2945.c  external_lib/script/build/dev_mp2945.c 
cp drivers/amd_crb_drivers_raa489300.c  external_lib/script/build/amd_crb_drivers_raa489300.c


####################################final delete c code ####################################################

rm app/usbc/app_4cc.c  
rm app/usbc/app_ccgx.c 

rm drivers/nuvoton/acpi_npcx4mnx.c 
rm drivers/nuvoton/gpio_npcx4mnx.c  
rm drivers/nuvoton/nuvoton_espi.c    
rm drivers/nuvoton/nuvoton_gpio_ctrl.c  


rm drivers/amd_crb_drivers_tps6599x.c  
rm drivers/amd_crb_drivers_tps6699x.c  
rm drivers/amd_crb_drivers_ucsi.c  
rm drivers/amd_crb_drivers_hpi.c  


rm drivers/dev_mp2845_SVI3.c  
rm drivers/dev_isl9241.c  

rm drivers/amd_crb_drivers_raa489300.c  

echo "----------Success!!!-----------"
