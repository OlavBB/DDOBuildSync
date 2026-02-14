# DDOBuildSync - DDO Builder V2 Build Sync

## What is this?
Win32 C++ GUI app that launches DDO Builder V2 and syncs character build files (`.DDOBuild`) to a GitHub repo via git CLI. Designed for keeping builds in sync across multiple machines.

## Build
```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
Requires: Visual Studio 2022 with C++ workload, CMake 3.20+

## Architecture
```
src/
  main.cpp              # WinMain entry, COM init, message loop
  main_window.cpp/.h    # GUI: buttons, labels, checkboxes, log panel, DDO Builder process monitoring
  git_manager.cpp/.h    # Shell out to git CLI via CreateProcess (init/pull/push/status)
  config.cpp/.h         # JSON config via nlohmann/json (FetchContent)
  utils.cpp/.h          # String conversion, path helpers, auto-detect DDO Builder folder
config/
  default_config.json   # Default config, copied to build dir post-build
resources/
  app.manifest          # asInvoker UAC manifest
  app.rc                # Resource script (icon)
  app.ico               # Application icon
```

## Git approach
Shells out to `git.exe` via `CreateProcess` with stdout/stderr captured through pipes. Uses the user's existing git + credential setup. No libgit2 dependency.

## GUI layout
- **Launch DDO Builder**: Starts DDOBuilder.exe, monitors process, auto-pushes on exit
- **Pull Builds**: `git pull origin main --rebase`
- **Push Builds**: `git add -A && git commit && git push`
- **Setup**: Browse for DDO Builder folder, enter GitHub repo URL, init git repo
- **Checkboxes**: Auto-pull on launch, auto-push on DDO Builder exit
- **Log panel**: Timestamped output with monospace font

## Config
User config saved as `ddobuildsync_config.json` next to exe (separate from `default_config.json`).
```json
{
  "buildsFolder": "C:\\Users\\...\\DDOBuilderV2_...",
  "ddoBuilderExe": "...\\DDOBuilder.exe",
  "gitRepoUrl": "https://github.com/user/repo.git",
  "autoPushOnClose": true,
  "autoPullOnLaunch": true
}
```

## Git sync details
- `.gitignore` uses whitelist approach: ignores everything (`*`), then allows `!*.DDOBuild`, `!*.DDOBuild.backup`, `!.gitignore`
- Push uses `git add -A` to capture new files, modifications, AND deletions
- Commit messages: `"Update builds - YYYY-MM-DD HH:MM:SS"`
- First-run setup: `git init`, write `.gitignore`, `git remote add origin`, initial commit+push

## Build gotchas
- Must define UNICODE and _UNICODE (in CMakeLists.txt)
- HMENU casts on x64: `reinterpret_cast<HMENU>(static_cast<INT_PTR>(id))`
- Manifest only via linker (not RC file) to avoid duplicate resource error
- Needs `ole32` linked for COM (SHBrowseForFolder)
- Icon resource uses numeric ID (`1 ICON`) not string ID
- Git operations run on worker threads; UI updates via PostMessage
