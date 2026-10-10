# Recent Apps

A work-in-progress Aroma/WUPS plugin project for a Wii U Menu recent-apps carousel concept.

## Requirements

- A Wii U running Aroma
- The Wii U plugin toolchain (WUT and WUPS development files)
- Docker, if building with the included GitHub Actions workflow or Dockerfile

## Build locally

With the Wii U plugin toolchain installed and `DEVKITPRO` configured:

```sh
make clean
make
```

The standard WUPS rules build `WiiUKioskRecentApps.wps` in the repository root.

## Build with Docker

The included Dockerfile uses the Wii U devkitPPC image and copies in the WUPS toolchain package:

```sh
docker build --pull -t recent-apps-builder .
docker run --rm -v "$PWD:/project" recent-apps-builder sh -lc 'make clean && make'
```

## Install

Copy `WiiUKioskRecentApps.wps` to the plugins directory for your Aroma environment, for example:

```text
sd:/wiiu/environments/aroma/plugins/WiiUKioskRecentApps.wps
```

## Project status

This repository is currently a scaffold, not a functional recent-apps switcher. The WUPS entry point does not install input or rendering hooks, recent-title history is not populated, and the drawing functions are placeholders. The build workflow checks compilation and uploads the resulting WPS artifact; it does not test runtime behavior.

## License

Licensed under GNU GPL v3.0. See [LICENSE](LICENSE).


Recent titles (newest first; history saved to SD).
