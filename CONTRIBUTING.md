# Contributing

## Scope

This repository contains two deliverables:

- A Win32 Discord Rich Presence plugin for Richard Burns Rally / RallySimFans.
- A static website that also publishes the public car and stage images used by Discord Rich Presence.

Keep both parts simple and practical. Avoid adding infrastructure that the project does not actively need.

## Native Plugin

- Keep runtime logic under `src/core`, `src/sources`, `src/discord`, `src/rbr`, and `src/win32`.
- Keep tracked plugin config under `src/config`.
- Keep build and packaging scripts under `src/build`.
- Keep vendored dependencies under `src/third_party`.
- Prefer small, explicit integrations over speculative abstractions.
- Preserve the current Win32 / Visual Studio build flow unless there is a concrete reason to change it.
- Do not commit local build outputs, packaged zips, IDE files, or deployment copies.

Build targets:

- Project: `RSFDiscordRPC.vcxproj`
- Configurations: `Debug|Win32`, `Release|Win32`

For a release build and package:

```powershell
./src/build/build-release.ps1
```

That script builds `Release|Win32`, creates `dist/RSFDiscordRPC-<version>/`, and creates `dist/RSFDiscordRPC-<version>.zip`.

Optional environment overrides:

- `RSF_MSBUILD_PATH`: override the `MSBuild.exe` path used by the build script
- `RSF_VERSION`: override the package version used in `dist/RSFDiscordRPC-<version>`

To copy the plugin into a local RallySimFans install as part of the release build:

```powershell
./src/build/build-release.ps1 -DeployToPlugins
```

`-DeployToPlugins` requires `RSFDiscordRPC.local.props` with a configured `<RSFPluginDir>`.

If you want the Visual Studio post-build step or `-DeployToPlugins` to copy files into your game install:

1. Copy [RSFDiscordRPC.local.props.example](RSFDiscordRPC.local.props.example) to `RSFDiscordRPC.local.props`
2. Set `<RSFPluginDir>` to the absolute path of your local RallySimFans `Plugins` folder

Without `RSFDiscordRPC.local.props`, builds and packaging still work, but nothing is copied into the game `Plugins` folder automatically.

## Web

- Keep the site static-first and English-only for code, UI text, comments, and documentation.
- Presentation assets live under `web/assets/`.
- RPC images are published from `web/assets/images/`.
- Source images for conversion belong in `web/scripts/data/cars` and `web/scripts/data/stages`.
- Converted images are written to `web/assets/images/cars` and `web/assets/images/stages`.
- Cars and stages are center-cropped to `512x512` and exported as `.jpg`.

Install dependencies:

```bash
cd web
npm install
```

Run the image converter:

```bash
cd web
npm run process-images
```

## Repository Hygiene

- Keep documentation centralized at the repository root.
- Keep source files under `src/` or `web/`, not at the root unless the file is intentionally repository-level.
- Ignore or remove generated directories such as Visual Studio output, local build trees, and packaged release artifacts.
