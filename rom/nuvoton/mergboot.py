#*********************************************************************************
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
# *********************************************************************************
import sys, os
import binascii
# Open File
f = open("boot_nuvton","rb")
boot_nuvton = f.read()
boot_nuvton = bytearray(boot_nuvton)
f.close()
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
boot_nuvtonsize = sys.getsizeof(boot_nuvton)-0x39
print('boot_nuvtonsize is 0x%x' %boot_nuvtonsize)
ecsize = sys.getsizeof(ecrawdata)-0x39
outdatsize = ecsize +0xff80
print('ecsize is 0x%x' %ecsize)
biosrawdata[0x0:boot_nuvtonsize ] = boot_nuvton [0x0:boot_nuvtonsize]
biosrawdata[0xf100:0x100 ] = ecconfigdata[0x0:0x100]
biosrawdata[0x10000:outdatsize ] = ecrawdata[0x0:ecsize]
filldatsize = 0x58000-outdatsize-0xC0
f.close()
f = open("zephyr_boot.bin","wb+")
f.write(biosrawdata)
for i in range(filldatsize):
	f.write(binascii.unhexlify("FF"))
f.close()
print('outdatsize  is 352k')