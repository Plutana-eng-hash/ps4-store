# PS4 Downloader (OpenOrbis / GoldHEN)

Homebrew app that fetches a file list from your website and downloads files
(including .pkg) to `/data/pkg/` on the PS4. Install them afterwards from
GoldHEN's Package Installer. Only host files you have the right to distribute.

## 1. Configure
Edit `src/config.h`: set `SERVER_HOST`, `SERVER_PORT`, `LIST_PATH`.
Your site must serve a plain-text list (see `server/list.txt`), one line per file:
`Display Name|/path/to/file.pkg`

NOTE: this version uses plain HTTP (no HTTPS). Use a LAN server or an HTTP URL.
HTTPS needs libSceHttp/libSceSsl and is a good next step.

## 2. Build
**Online:** push this folder to a GitHub repo -> Actions tab -> download the PKG from Artifacts.
**Local (Linux/WSL):**
    export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis
    ./build.sh      # -> out/*.pkg

## 3. Test server
    cd server && python3 serve.py     # put files in server/files/

## 4. Use
Install the PKG with GoldHEN, launch it. Notifications show status:
D-pad Up/Down = choose, X = download, Circle = exit.

## Status
Untested scaffold. Header/function names follow OpenOrbis conventions but may
need small fixes against your toolchain version. Check the OpenOrbis samples
(`samples/`) and Discord if the compiler complains.
