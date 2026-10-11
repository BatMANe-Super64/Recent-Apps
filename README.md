# Recent Apps for Wii U

Recent Apps is an Aroma/WUPS plugin for the Wii U that keeps a persistent list of up to ten recently used titles and lets you select and launch a title from an on-screen notification.

## Features

- Tracks recently used titles and keeps the newest title first.
- Stores history in WUPS plugin storage so it persists across sessions.
- Displays title names from installed-title metadata, falling back to the title ID when no name is available.
- Opens and closes the Recent Apps notification with a controller shortcut.
- Lets you move through recorded titles and launch the selected title.
- Avoids recording the HOME Menu and filters Health and Safety from automatic startup history.

## Controls

Use the TV button together with:

| Shortcut | Action |
| --- | --- |
| TV + L | Toggle the Recent Apps notification |
| TV + Up | Select the previous recorded title |
| TV + Down | Select the next recorded title |
| TV + A | Launch the selected title |

The Up/Down shortcuts are handled while the notification is open so they do not also move the HOME Menu cursor.

## Requirements

- A Wii U running Aroma
- The WUT/WUPS plugin toolchain for local builds
- Docker for the provided container build workflow

## Build locally

With the Wii U plugin toolchain installed and `DEVKITPRO` configured:

```sh
make clean
make
```

The standard WUPS rules produce `WiiUKioskRecentApps.wps` in the repository root.

## Build with Docker

From the repository root:

```sh
docker build --pull -t recent-apps-builder .
docker run --rm -v "$PWD:/project" recent-apps-builder sh -lc 'make clean && make'
```

The GitHub Actions workflow builds the plugin and uploads the resulting WPS package as an artifact. A successful CI build confirms compilation and packaging; it does not replace testing on real Wii U hardware.

## Install

Copy `WiiUKioskRecentApps.wps` to the plugins directory in your Aroma environment, for example:

```text
sd:/wiiu/environments/aroma/plugins/WiiUKioskRecentApps.wps
```

Use the artifact named `WiiUKioskRecentApps-Plugin` from a successful GitHub Actions run, or build the package locally.

## Notes

- The list holds up to ten distinct titles; launching a title already in history moves it to the newest position.
- Health and Safety is filtered from automatic history updates. An explicit launch from Recent Apps may add it to the list.
- NotificationModule must be available for the on-screen notification interface. If it cannot initialize, history tracking can still run but the notification UI will not be available.
- The plugin's configuration menu includes status information for history storage, shortcut registration, notifications, and diagnostic GX2 hooks.

## License

Licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).
