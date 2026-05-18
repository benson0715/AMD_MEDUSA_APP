@echo off
set SPI_PATH="C:\Program Files (x86)\DediProg\SF Programmer"
set SPI_TOOL=dpcmd.exe

echo SPI TOOL: %SPI_PATH%\%SPI_TOOL%

%SPI_PATH%\%SPI_TOOL% -u %1 --type W25Q256JW --addr 0 -v

pause