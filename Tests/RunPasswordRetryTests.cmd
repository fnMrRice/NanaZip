@echo off
setlocal
cd /d "%~dp0.."
set "Binaries=%CD%\Output\Binaries\Debug\x64"
set "Fixtures=%CD%\Output\PasswordHistoryTests"
if not exist "%Fixtures%" mkdir "%Fixtures%"
cl /nologo /std:c++17 /EHsc /DUNICODE /D_UNICODE /Fe:Output\PasswordHistoryTests\PasswordRetryTests.exe /Fo:Output\PasswordHistoryTests\ Tests\PasswordRetryTests.cpp Tests\PasswordRetryGuids.cpp NanaZip.Universal\SevenZip\CPP\Common\MyString.cpp NanaZip.Universal\SevenZip\CPP\Common\StringConvert.cpp NanaZip.Universal\SevenZip\CPP\Common\IntToString.cpp NanaZip.Universal\SevenZip\CPP\Windows\ErrorMsg.cpp oleaut32.lib ole32.lib user32.lib
if errorlevel 1 exit /b %errorlevel%
echo Password retry fixture>"%Fixtures%\payload.txt"
"%Binaries%\Nanazip-PM.Console.exe" a "%Fixtures%\header.7z" "%Fixtures%\payload.txt" -pright -mhe=on -y
if errorlevel 1 exit /b %errorlevel%
"%Binaries%\Nanazip-PM.Console.exe" a "%Fixtures%\content.7z" "%Fixtures%\payload.txt" -pright -y
if errorlevel 1 exit /b %errorlevel%
"%Binaries%\Nanazip-PM.Console.exe" a "%Fixtures%\aes.zip" "%Fixtures%\payload.txt" -pright -mem=AES256 -y
if errorlevel 1 exit /b %errorlevel%
"%Binaries%\Nanazip-PM.Console.exe" a "%Fixtures%\legacy.zip" "%Fixtures%\payload.txt" -pright -y
if errorlevel 1 exit /b %errorlevel%
"%Fixtures%\PasswordRetryTests.exe" "%Binaries%\NanaZip.Core.dll" "%Fixtures%"
exit /b %errorlevel%
