# imas-sc-prism-localify
## iM@S SC Prism localify patch

### Usage:
1. Put `version.dll`, `config.json` and the `dicts` referenced by `config.json` near the `umamusume.exe`.
2. Launch the game

### Config:
- `enableConsole` Enable the console for printing debug infomations (true/false)
- `enableLogger` Output uncovered text entries into `dump.txt` (true/false)
- `dumpStaticEntries` Requires ^, Dump hardcoded text entries into `dump.txt`
- `maxFps` Max fps limit (-1 = Unmodified/0 = Unlimited/>0 = Lock to #fps)
    - Note: VSync is enabled
- `uiScale` Custom UI scale
- `replaceFont` Replace all font to default font, solves missing word issue (true/false)
- `dicts` A list of dicts that read by this (Path)

### Known issue
- None

# Build
Platform Toolset: clang required
