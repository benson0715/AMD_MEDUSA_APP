#EC target build options

# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.


#!/bin/bash
. ./zephyrmenubuild.sh

PS3="Select which CRB EC you want to build or delete build:"
select option in \
"NPCX4MNX MDS Plum" \
"NPCX4MNX MDS Plum EC Module" \
"NPCX4MNX MDS Dorne" \
"NPCX4MNX MDS Aeris" \
"NPCX4MNX BOOT" \
"AI CHECK" \
"Delete Build" \
"Exit"
do
    case $option in
    "Exit")
        break;;
    "NPCX4MNX MDS Plum")
        export board=npcx4mnx_mdsplum
	if [ T"$board" != T"" ];then
    	cd ./boards/nuvoton/$board/
   	 . ./gen_ioexptable.sh
   	 . ./gen_nativegpio.sh
   	 cd ../../..
	westbuildnpcx4mnx $board $PWD
	fi
        break;;
    "NPCX4MNX MDS Plum EC Module")
        export board=npcx4mnx_mdsplum_ecmodule
	if [ T"$board" != T"" ];then
    	cd ./boards/nuvoton/$board/
   	 . ./gen_nativegpio.sh
   	 cd ../../..
	westbuildnpcx4mnx $board $PWD
	fi
        break;;
    "NPCX4MNX MDS Dorne")
        export board=npcx4mnx_mdsdorne
	if [ T"$board" != T"" ];then
    	cd ./boards/nuvoton/$board/
   	 . ./gen_ioexptable.sh
   	 . ./gen_nativegpio.sh
   	 cd ../../..
	westbuildnpcx4mnx $board $PWD
	fi
        break;;
    "NPCX4MNX MDS Aeris")
        export board=npcx4mnx_mdsaeris
	if [ T"$board" != T"" ];then
    	cd ./boards/nuvoton/$board/
   	 . ./gen_ioexptable.sh
   	 . ./gen_nativegpio.sh
   	 cd ../../..
	westbuildnpcx4mnx $board $PWD
	fi
        break;;
    "NPCX4MNX BOOT")
        export board=npcx4mnx_boot
	if [ T"$board" != T"" ];then
	westbuildnpcx4mnxboot $board $PWD
	fi
        break;;
    "AI CHECK")
	python3 ./EC_AI_CODE_CHECK.py
        break;;
    "Delete Build")
        westclean
        break;;
    *)
        clear
        echo "Wrong Selection";;
esac
done



