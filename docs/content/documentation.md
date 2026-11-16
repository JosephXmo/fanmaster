# Documentation workflow

The reference site is a separate host-built static website. Markdown sources are
in `docs/content/`, navigation in `docs/site.json`, and HTML/CSS/JavaScript in
`docs/theme/`. Generated output is ignored at `build/docs/`.

## Build and preview

From the repository root on macOS/Linux:

```sh
python3 -m venv .venv-docs
./.venv-docs/bin/python -m pip install -r docs/requirements.txt
./.venv-docs/bin/python scripts/build_docs.py
./.venv-docs/bin/python -m http.server 8000 --bind 127.0.0.1 --directory build/docs
```

On Windows PowerShell:

```powershell
py -3 -m venv .venv-docs
& ./.venv-docs/Scripts/python.exe -m pip install -r docs/requirements.txt
& ./.venv-docs/Scripts/python.exe scripts/build_docs.py
& ./.venv-docs/Scripts/python.exe -m http.server 8000 --bind 127.0.0.1 --directory build/docs
```

Open `http://127.0.0.1:8000`. Stop the server with Ctrl+C. After editing content
or theme files, rebuild and refresh. The rendered HTML also works directly from
`build/docs/index.html`; search data, styles and scripts are local assets and
require no CDN or device connection. The server example binds to localhost.

## Edit the site

- Add a Markdown file under `docs/content/`, then register it in `docs/site.json`.
- Use relative Markdown links, such as `[controls](controls.md)`. The builder
  rewrites these to HTML routes and validates their target files and anchors.
- Headings create permalink anchors and the on-page table of contents.
- Tables, fenced code and nested lists use Python-Markdown extensions.
- Search is generated from page headings and text during the build. The browser
  filters that local index; no search request or analytics service is used.

A build fails if a content source is missing/unregistered, a page title is
missing, or a local link/anchor is broken. Navigation, search, source downloads
and previous/next links are regenerated together.

## Firmware resource boundary

`scripts/assets.py` packs **only `web/`** by default. The documentation sources,
rendered website and build dependencies live outside that directory. Building
this site never flashes a board, modifies NVS, uploads to W25Q64 or adds a route
to the embedded dashboard.

The maintenance-card spacing fix is in `web/assets/panel.css`. To deploy that
panel change later, repack the seven-resource image and follow the normal
[resource update workflow](resources.md). It does not require uploading this
reference website or rebuilding either application.

## Record evidence

Keep current behavior in the feature guides. Put dated target observations,
image hashes, measurements and remaining limits in [validation](validation.md).
[Flash layout](storage.md) describes the last measured/deployed build; a new
resource pack changes its image length/hash even if firmware code is unchanged.
Historical reports should retain their original results and be labeled as such.
