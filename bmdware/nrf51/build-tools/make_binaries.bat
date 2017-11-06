@echo off
cd %1\..\..
cd build-tools
for /f %%i in ('C:\Python34\python.exe get_version.py %3') do set output=%%i
echo Binary Image Generation Script
echo Create local HEX file copy.
copy %1_build\%2.hex ..\build\%2%output%.hex /Y
echo Generate OTA update image
"C:\Python34\python.exe" genimage\genimage.py ..\build\%2%output%.hex --output ..\build\%2%output%_ota.bin
echo General raw binary image
"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe" -z --bin --output ..\build\%2%output%.bin %1_build\%2.axf
