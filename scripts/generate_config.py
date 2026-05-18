
import sys
import time
from time import localtime,strftime

import sys, os
if hasattr(sys, 'frozen'):
    os.environ['PATH'] = sys._MEIPASS + ";" + os.environ['PATH']

import threading
import binascii

from xml.dom.minidom import parse 
import xml.dom.minidom

try: 
    import xml.etree.cElementTree as ET 
except ImportError: 
    import xml.etree.ElementTree as ET 

def crc16_direct(bytestr):
        crc = 0
        if len(bytestr) == 0:
            return 0
        for i in range(len(bytestr)):
            R = int(bytestr[i], 16)
            for j in range(8):
                if R > 127:
                    k = 1
                else:
                    k = 0
                    
                R = (R << 1) & 0xff
                if crc > 0x7fff:
                    m = 1
                else:
                    m = 0
                
                if k + m == 1:
                    k = 1
                else:
                    k = 0
                    
                crc = (crc << 1) & 0xffff
                if k == 1:
                    crc ^= 0x1021
        return crc


regListValue = []
regListLength = []
configRegionSource = []
configSize = 0
lsb = 1

f = open(sys.argv[1],"rb")
        
tree = ET.parse(f)   
root = tree.getroot()    
print("*******ec auto config*******")
  
#print ("Root: " + root.tag, "---", root.attrib)
        
#for child in root: 
    #print ("Child: " + child.tag, "---", child.attrib)
       
#print ("*"*20)
#print (root[1][0].tag)
#print (root[1].tag)
#print ("*"*20)

for register in root[0]:  
        if register.tag == 'EcConfigSize':
           configSize = register.get('value')
           #print("ConfigRegionSize: " + configSize)

for register in root[1]:  
	#print ("Reg: " + register.tag, "---", register.attrib)
	regListValue.append(register.get('value'))
	if register.get('Type') == 'uint8_t':
		regListLength.append(1)
	elif register.get('Type') == 'uint16_t':
		regListLength.append(2)
	elif register.get('Type') == 'uint32_t':
		regListLength.append(4)
	else:
		regListLength.append(register.get('Type'))
                
#print (regListLength)
#print (regListValue) 

f.close()

 
f = open( sys.argv[1], "rb")
f2 = open( sys.argv[2], "w")
f1 = open( sys.argv[3], "wb")

index = 0
configRegionCnt = 0
for element in regListValue:
            if regListLength[index] == 1:
                element = element[2:]
                if len(element) == 2:
                    f1.write(binascii.unhexlify(element))
                    configRegionSource.append(element)
                elif len(element) == 0:
                    print ("value length error")
                elif len(element) == 1:
                    element = '0' + element
                    f1.write(binascii.unhexlify(element))
                    configRegionSource.append(element)
                elif len(element) > 2:
                    element = element[-2:]
                    f1.write(binascii.unhexlify(element))
                    configRegionSource.append(element)
                    
            elif regListLength[index] == 2:
                element = element[2:]
                if len(element) == 4:
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[:2])
                elif len(element) == 0:
                    print ("value length error")
                elif len(element) == 1:
                    element = '000' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[:2])
                elif len(element) == 2:
                    element = '00' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[:2])
                elif len(element) == 3:
                    element = '0' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[:2])
                elif len(element) > 4:
                    element = element[-4:]
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[:2])

            elif regListLength[index] == 4:
                element = element[2:]
                if len(element) == 8:
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) == 0:
                    print ("value length error")
                elif len(element) == 7:
                    element = '0' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) == 6:
                    element = '00' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) == 5:
                    element = '000' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) == 4:
                    element = '0000' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) == 3:
                    element = '00000' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) == 2:
                    element = '000000' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) == 1:
                    element = '0000000' + element
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                elif len(element) > 8:
                    element = element[-8:]
                    if lsb == 0:
                        f1.write(binascii.unhexlify(element))
                        configRegionSource.append(element[:2])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[-2:])
                    else:
                        f1.write(binascii.unhexlify(element[-2:]))
                        f1.write(binascii.unhexlify(element[4:6]))
                        f1.write(binascii.unhexlify(element[2:4]))
                        f1.write(binascii.unhexlify(element[:2]))
                        configRegionSource.append(element[-2:])
                        configRegionSource.append(element[4:6])
                        configRegionSource.append(element[2:4])
                        configRegionSource.append(element[:2])
                    
            configRegionCnt = configRegionCnt + regListLength[index]
            
            # 2 bytes for CRC
            if configRegionCnt >= (int(configSize[2:], 16) - 2):
                print ("Config region overflow %d",configRegionCnt )
                          
            index = index + 1
        
        # add '0' if the registers doesn't hit the 254
buf_configRegionCnt = configRegionCnt
while (configRegionCnt < (int(configSize[2:], 16) - 4)):
	configRegionCnt = configRegionCnt + 1
	f1.write(binascii.unhexlify("00"))
	configRegionSource.append('00')

# Index fixed data 55 AA
import math

a = buf_configRegionCnt
b = configRegionCnt
c = a / b
c = c * 100
use_rate = "{:.2f}".format(c)

f1.write(binascii.unhexlify("AA"))
configRegionSource.append('AA') 
f1.write(binascii.unhexlify("55"))
configRegionSource.append('55')  
         
crc16 = crc16_direct(configRegionSource)
crc16 = hex(crc16)
print("Used rate =",use_rate ,"%")
print("Config CRC = ",crc16)

crc16 = binascii.unhexlify(crc16[2:])
f1.write(crc16)
        
f2.write('/*****************************************************************************\n')
f2.write(' * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.\n')
f2.write(' ******************************************************************************\n')
f2.write(' */\n')
f2.write('   \n')
f2.write('#ifndef __EC_CONFIG_REGION_H__\n')
f2.write('#define __EC_CONFIG_REGION_H__\n')
f2.write('   \n')
f2.write('#include <stdint.h>\n')      
f2.write('#include <stdbool.h>\n')       
f2.write('   \n')
f2.write('#define EC_CONFIG_REGION_LENGTH     ')
f2.write(configSize)
f2.write('   \n')
f2.write('   \n')
f2.write('#define BSYSCOF_BASE_ADDR      0xCF040   \n')
f2.write('#define BSYSCOF_BUF_SIZE       256   \n')
f2.write('#pragma pack(push, 1)\n')
f2.write('   \n')
f2.write('typedef union {\n')
f2.write('uint8_t arr[EC_CONFIG_REGION_LENGTH];\n')
f2.write('  struct {\n')
f2.write('   \n')
        
for register in root[1]:  
	f2.write('   ')
	f2.write(register.get('Type'))
	f2.write(' ')
	f2.write(register.tag)
	f2.write(';  /* ')
	f2.write(register.get('label'))
	f2.write(' */  \n')
        
f2.write('   ')
f2.write('uint8_t')
f2.write(' ')
f2.write('Resv[')
f2.write(str(int(configSize[2:], 16) - 2 - buf_configRegionCnt))
#print (int(configSize[2:], 16) - 2 - buf_configRegionCnt)
f2.write('];')
f2.write('   /* Reserve region */\n')
        
f2.write('   ')
f2.write('uint16_t')
f2.write(' ')
f2.write('crc16')
f2.write(';')
f2.write('   /* 2 bytes for CRC16 */\n')
        
f2.write('   \n')
f2.write('  } f;\n')
f2.write('} T_EC_CONFIG_REGION_FIELD;\n')
f2.write('   \n')
f2.write('#pragma pack(pop)\n')
f2.write('   \n')
f2.write('extern T_EC_CONFIG_REGION_FIELD *_s_BoardSysConfig; \n')
f2.write('extern int brdSysConfig_Init(void);   \n')
f2.write('#endif // end of __EC_CONFIG_REGION_H__')       


f.close()
f1.close()
f2.close()


print("**************************** ")
