@echo off
rem the pictures of where to tap (shots\guide\*.png), from the preview's shots and notes.txt
rem run tools\preview\BUILD.BAT for st7796, gc9a01 and ili9341 first. needs the .NET 8 SDK
setlocal
cd /d "%~dp0"
dotnet run -c Release -- notes.txt ..\..\shots
