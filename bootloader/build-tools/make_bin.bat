@echo off
cd %1..\..
for /f %%i in ('C:\Python34\python.exe build-tools\get_version.py %3') do set output=%%i
if not exist build mkdir build
if not exist build\ver%output% mkdir build\ver%output%
echo Binary Image Generation Script
echo Create local HEX file copy.
copy %1_build\%2.hex build\ver%output%\%2%output%.hex /Y
echo Generate OTA update image
"C:\Python34\python.exe" build-tools\genimage\genimage.py build\ver%output%\%2%output%.hex --output build\ver%output%\%2%output%_ota.bin
echo General raw binary image
"C:\Keil_v5\ARM\ARMCC\bin\fromelf.exe" -z --bin --output build\ver%output%\%2%output% %1_build\%2.axf
copy build\ver%output%\%2%output%\ER_IROM1 build\ver%output%\%2%output%.bin
rem copy "build\%2%output%\ER$$.ARM.__AT_0x10001014" build\%2%output%_uicr.bin
rmdir build\ver%output%\%2%output%\ /S /Q