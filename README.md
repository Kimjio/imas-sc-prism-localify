# imas-sc-prism-localify
## iM@S SC Prism localify patch

### Usage:
1. Put `version.dll`, `config.json` and the `dicts` referenced by `config.json` near the `imasscprism.exe`.
2. Launch the game

### Config:
- `enableConsole` Enable the console for printing debug infomations (`true` / `false`)
- `enableLogger` Output uncovered text entries into `dump.txt` (`true` / `false`)
- `dumpIl2Cpp` Dump IL2CPP data into `dump.cs` (`true` / `false`)
- `dumpStaticEntries` Requires `enableLogger`, Dump hardcoded text entries into `dump.txt` (`true` / `false`)
- `dumpLocalization` Dump localization dict into `localization.json`, `localization_local.json` (`true` / `false`)
- `dumpTexture` Dump loaded textures into `TextureDump` (performance may be degraded) (`true` / `false`)
- `maxFps` Max FPS limit (`-1` = Unmodified / `0` = Unlimited / `n > 0` = = Limit to `n`)
    - Note: VSync is enabled
- `unlockSize` Allow game to resizable (`true` / `false`)
- `uiScale` Custom UI scale (`0` < ~)
- `uiAnimationScale` Change UI animation scale (higher the faster) (`0` < ~)
    - Caution: Soft lock occurs when set to `0`.
- `resolution3dScale` Custom 3D resolution scale (higher the better quality) (`0` < ~)
- `replaceToCustomFont` Replace font to custom font (`true` / `false`)
- `fontAssetBundlePath` Font asset bundle path
- `fontAssetName` Font asset name for UGUI Text
- `tmproFontAssetName` Font asset name for TMPro Text
- `antiAliasing` Change MSAA settings (`-1`, `0`, `2`, `4`, `8`)
- `customTitleName` Custom window title name
- `replaceAssetsPath` Directory path containing custom hash assets
- `replaceAssetBundleFilePaths` A list of custom assetbundles
- `localizationJsonPath` Custom localization dict json path
- `dicts` A list of dicts that read by this (Path)

### Always activated feature
- Prevent nProtect from running during game initialization

### Known issue
- None

# Build
Platform Toolset: clang required
