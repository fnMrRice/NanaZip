@echo off
setlocal
cd /d "%~dp0.."
if not exist Output\PasswordHistoryTests mkdir Output\PasswordHistoryTests
cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE /Fe:Output\PasswordHistoryTests\PasswordHistoryTests.exe /Fo:Output\PasswordHistoryTests\PasswordHistoryTests.obj Tests\PasswordHistoryTests.cpp
if errorlevel 1 exit /b %errorlevel%
Output\PasswordHistoryTests\PasswordHistoryTests.exe "%CD%\Output\PasswordHistoryTests\History.json"
exit /b %errorlevel%
