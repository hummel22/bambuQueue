# bambuQueue

## macOS (Apple Silicon) build + run

These instructions target macOS on Apple Silicon (M1/M2/M3). You can build from the repo
root with `make` or the helper scripts.

### Prerequisites

Install required tooling with Homebrew:

```bash
./scripts/setup_macos.sh
```

This installs: `cmake`, `wxwidgets`, `sqlite`, `curl`, `mosquitto`, and `create-dmg`.

### Build

```bash
make build
```

Or directly:

```bash
./scripts/build_macos.sh
```

### Run

```bash
make run
```

The app stores its data/config in:

```
~/Library/Application Support/BambuQueue
```

### Create a DMG

```bash
make dmg
```

This produces `dist/BambuQueue.dmg`.

### Quick test build

```bash
make test
```

## Connecting to a Bambu printer (LAN mode)

For a longer walkthrough, see [docs/build_macos.md](docs/build_macos.md).  

See [docs/connecting_to_bambu.md](docs/connecting_to_bambu.md) for setup details,
including LAN mode and where to find the access code.
