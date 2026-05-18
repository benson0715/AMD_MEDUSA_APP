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
cp external_lib/script/build/CMakeLists.txt.nuvoton drivers/nuvoton/CMakeLists.txt  
cp external_lib/script/build/CMakeLists.txt.app app/CMakeLists.txt  
cp external_lib/script/build/CMakeLists.txt.drivers drivers/CMakeLists.txt

rm external_lib/script/build/CMakeLists.txt.nuvoton 
rm external_lib/script/build/CMakeLists.txt.app 
rm external_lib/script/build/CMakeLists.txt.drivers 
####################################remove from build folder####################################################


####################################remove c code to build####################################################

cp external_lib/script/build/app_4cc.c app/usbc/app_4cc.c  
cp external_lib/script/build/app_ccgx.c  app/usbc/app_ccgx.c 

cp external_lib/script/build/acpi_npcx4mnx.c  drivers/nuvoton/acpi_npcx4mnx.c    
cp external_lib/script/build/gpio_npcx4mnx.c  drivers/nuvoton/gpio_npcx4mnx.c   
cp external_lib/script/build/nuvoton_espi.c drivers/nuvoton/nuvoton_espi.c   
cp external_lib/script/build/nuvoton_gpio_ctrl.c drivers/nuvoton/nuvoton_gpio_ctrl.c 


cp external_lib/script/build/amd_crb_drivers_tps6599x.c  drivers/amd_crb_drivers_tps6599x.c  
cp external_lib/script/build/amd_crb_drivers_tps6699x.c drivers/amd_crb_drivers_tps6699x.c  
cp external_lib/script/build/amd_crb_drivers_ucsi.c  drivers/amd_crb_drivers_ucsi.c  
cp external_lib/script/build/amd_crb_drivers_hpi.c drivers/amd_crb_drivers_hpi.c  


cp external_lib/script/build/dev_mp2845_SVI3.c  drivers/dev_mp2845_SVI3.c  
cp external_lib/script/build/dev_isl9241.c drivers/dev_isl9241.c  

cp external_lib/script/build/amd_crb_drivers_raa489300.c drivers/amd_crb_drivers_raa489300.c  


####################################delete temp c code####################################################

rm external_lib/script/build/app_4cc.c 
rm external_lib/script/build/app_ccgx.c  

rm external_lib/script/build/acpi_npcx4mnx.c  
rm external_lib/script/build/gpio_npcx4mnx.c  
rm external_lib/script/build/nuvoton_espi.c 
rm external_lib/script/build/nuvoton_gpio_ctrl.c


rm external_lib/script/build/amd_crb_drivers_tps6599x.c  
rm external_lib/script/build/amd_crb_drivers_tps6699x.c 
rm external_lib/script/build/amd_crb_drivers_ucsi.c  
rm external_lib/script/build/amd_crb_drivers_hpi.c 


rm external_lib/script/build/dev_mp2845_SVI3.c  
rm external_lib/script/build/dev_isl9241.c 

rm external_lib/script/build/amd_crb_drivers_raa489300.c 
echo "----------Success!!!-----------"
