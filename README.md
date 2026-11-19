# PS4 Downloader (OpenOrbis / GoldHEN)

Homebrew app that reads a **JSON game list over HTTPS**, shows it as a proper on-screen menu and
downloads the selected `.pkg` to `/data/pkg/` on the PS4 (with a progress bar, speed/ETA, cancel and
**resume**). Install the finished file afterwards from GoldHEN's Package Installer.
Only download and host files you have the right to distribute.

## Controls
| Button | Action |
|---|---|
| D-pad Up / Down | select (hold to scroll) |
| L1 / R1 (or D-pad Left / Right) | page up / down |
| **X** | download the selected entry (resumes a partial download) |
| **O** | cancel a running download (partial file is kept) / exit from the list |
| **Triangle** | reload the list (also retries after an error) |

## Where the list comes from
`src/config.h` -> `FEED_URL` (default: `https://raw.githubusercontent.com/Plutana-eng-hash/ps4-store/refs/heads/main/Gms.json`).

You can change it **without rebuilding**: put a URL on the first line of `/data/ps4store/feed_url.txt` on the console.

### Accepted JSON layouts
The reader is deliberately tolerant; it finds the entries wherever they are. Any of these work:

```jsonc
[ { "name": "Game", "pkg": "https://host/Game.pkg", "title_id": "CUSA00000", "version": "01.00", "region": "USA", "size": "4.2 GB" } ]

{ "games": [ { "title": "Game", "download_url": "https://host/Game.pkg" } ] }

{ "DATA": { "https://host/Game.pkg": { "name": "Game", "version": "1.00", "size": 4509715456 } } }   // fPKGi style

{ "Game A": "https://host/a.pkg", "Game B": "https://host/b.pkg" }
```
* Link keys recognised: `pkg, pkg_url, url, link, download, download_url, file, path, href ...` (or any value that is an `http(s)` link ending in `.pkg`). Relative links are resolved against the list's own URL. Links that redirect (e.g. GitHub release assets) are followed.
* Name keys: `name, title, game, game_name, app_name ...`; extra info shown if present: `title_id/id/cusa`, `version`, `region`, `type/category`, `size` (bytes, or text like `"12.5 GB"`).
* Image links (`.png/.jpg/...`) are ignored; cover art is not displayed.
* Limits (edit in `config.h`): 6 MB list file, 2048 entries.

See `examples/Gms.example.json`.

## Build
**Online (GitHub Actions):** push this folder to a GitHub repo -> Actions tab -> download `PS4Downloader-pkg` from the run's Artifacts. The workflow also runs the host-side feed tests and checks that the toolchain has every library the app links (VideoOut, Http, Ssl, ...), so a missing piece is reported clearly.

**Local (Linux/WSL):**
    export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis
    ./build.sh      # -> out/*.pkg

**Host tests (no PS4 needed):** `./tests/run_host_tests.sh`

## Notes / limitations
* **HTTPS certificate verification is switched off** (homebrew cannot use Sony's CA store). That is what makes `raw.githubusercontent.com` work, but it means the connection is not authenticated.
* No cover images (no PNG/JPG decoder is bundled); the details panel shows a coloured tile with the initial.
* Links that need a web page / login / confirmation step (Google Drive, MediaFire, ...) cannot be downloaded - the list needs direct file links.
* Files are written as `<name>.pkg.part` and renamed to `<name>.pkg` when complete, which is also how resume works.
* The UI font is generated from DejaVu Sans (`tools/gen_font.py`, free license).
