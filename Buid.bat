@echo off
echo Building Pakdefender3d Multiplayer...
gcc -O2 pakdefender3d_server.c -o pakdefender3d_server.exe -lws2_32 -lm
gcc -O2 "Pak defender 3d.c" -o Pakdefender3d.exe -lSDL2 -lfreeglut -lopengl32 -lglu32 -lws2_32 -lm
echo Done!
pause
