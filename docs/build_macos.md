# macOS (Apple Silicon) build + packaging

This guide targets macOS on Apple Silicon (M1/M2/M3) and uses Homebrew.

## Prerequisites

Install build and runtime dependencies:

```bash
./scripts/setup_macos.sh
```

This installs:

- `cmake`
- `wxwidgets`
- `sqlite`
- `curl`
- `mosquitto` (for MQTT CLI helpers used by the app)
- `create-dmg` (for packaging)

## Build

```bash
make build
```

Artifacts are produced under `build/`.

## Run

```bash
make run
```

Application data and the config file live in:

```
~/Library/Application Support/BambuQueue
```

## Create a DMG

```bash
make dmg
```

The output is:

```
dist/BambuQueue.dmg
```

## Quick build check

```bash
make test
```
