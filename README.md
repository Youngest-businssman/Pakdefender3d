## ⚠️ LEGAL NOTICE
This software may not be copied, redistributed, or converted into an app.  
See the [LICENSE](LICENSE) file for full terms.
# Pakdefender3d

A 3D multiplayer shooter inspired by Free Fire, set in Pakistan.  
Real weapons, traditional characters, gloo walls, emotes, diamonds & coins.

## Compile and run on any device

### Prerequisites – install SDL2
| OS | Command |
|----|---------|
| **Windows (MSYS2)** | `pacman -S mingw-w64-x86_64-SDL2` |
| **Ubuntu/Debian**   | `sudo apt install libsdl2-dev`   |
| **Fedora**          | `sudo dnf install SDL2-devel`    |
| **macOS**           | `brew install sdl2`              |

### Build
```bash
gcc -O2 pakdefender3d.c -o pakdefender3d -lSDL2 -lGL -lGLU -lm
