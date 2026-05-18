#;*****************************************************************************
#;
#; Copyright (C) 2008-2022 Advanced Micro Devices, Inc. All rights reserved.
#;
#;******************************************************************************

# Open File
f = open("spi_image.bin","rb")
ecrawdata = f.read()
ecrawdata = bytearray(ecrawdata)
f.close()

f = open("ec.bin","wb+")
f.write(ecrawdata[0x81000:0xC1000])
f.close()