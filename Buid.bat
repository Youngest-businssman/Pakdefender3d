
#### 📄 `build.bat` (optional, helps Windows users)
```batch
@echo off
echo Building Pakdefender3d...
gcc -O2 pakdefender3d.c -o pakdefender3d.exe -lSDL2 -lOpenGL32 -lglu32 -lm
if %errorlevel% equ 0 (
    echo Success! Run pakdefender3d.exe
) else (
    echo Build failed - check if SDL2 is installed.
)
pause
