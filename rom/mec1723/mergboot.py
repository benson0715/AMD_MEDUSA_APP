#*********************************************************************************
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
# *********************************************************************************
 
# Open File
f = open("zephyr.bin","rb")
ecrawdata = f.read()
ecrawdata = bytearray(ecrawdata)
f.close()
f = open("ec_sys_config.bin","rb")
ecconfigdata = f.read()
ecconfigdata = bytearray(ecconfigdata)
f.close()
f = open("boot","rb")
biosrawdata = f.read()
biosrawdata = bytearray(biosrawdata)
import sys
ecsize = sys.getsizeof(ecrawdata)-0x39
outdatsize = ecsize +0xff40
print('ecsize is 0x%x' %ecsize)
print('outdatsize  is 0x%x' %outdatsize)
biosrawdata[0xf040:0x100 ] = ecconfigdata[0x0:0x100]
biosrawdata[0xff40:outdatsize ] = ecrawdata[0x0:ecsize]


f.close()
f = open("zephyr_boot.bin","wb+")
f.write(biosrawdata)
f.close()