@echo off
rem the README's GIF: the how-to film (TOUR.BAT st7796 first) inside the photo of the 3.5" board,
rem two parts of it: the planes, a tap, the rim and the zoom; then the looks and the three dots.
rem 8 frames a second keeps it near 4 MB. ffmpeg from C:\ffmpeg\bin, or set FFMPEG=path\ffmpeg.exe
rem writes ..\..\shots\st7796\tour.gif
setlocal
if "%FFMPEG%"=="" set FFMPEG=C:\ffmpeg\bin\ffmpeg.exe
if not exist "%FFMPEG%" (
  echo ffmpeg not found: %FFMPEG%
  exit /b 1
)
cd /d "%~dp0..\.."
rem the photo is 700x400 with the screen at 16%% 10%%, 71.5%% wide: here 600 wide
"%FFMPEG%" -hide_banner -loglevel error -y -f lavfi -i "color=c=0x03081a:s=600x342:r=8" ^
  -i site\img\board-35.png -i shots\st7796\tour.mp4 -filter_complex ^
  "[2:v]split[v1][v2];[v1]trim=0:24.9,setpts=PTS-STARTPTS[c1];[v2]trim=37.8:45.4,setpts=PTS-STARTPTS[c2];[c1][c2]concat=n=2:v=1[cv];[1:v]scale=600:342[ph];[0:v][ph]overlay=0:0[bg];[cv]fps=8,scale=428:286:flags=lanczos[s];[bg][s]overlay=96:34:shortest=1,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" ^
  -loop 0 shots\st7796\tour.gif
if errorlevel 1 exit /b 1
echo wrote shots\st7796\tour.gif
