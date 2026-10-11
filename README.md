<img width="1800" height="1056" alt="IMG_1973" src="https://github.com/user-attachments/assets/8032e0fb-7267-4f72-abfb-6b01d87dcb22" />
<img width="1800" height="1034" alt="IMG_1980" src="https://github.com/user-attachments/assets/a10d31fc-4f64-4ebd-a455-b60ea85f4047" />
<img width="1800" height="1081" alt="IMG_1974" src="https://github.com/user-attachments/assets/834cf34a-32e3-4694-ae9b-382ea885ba3c" />
<img width="1800" height="1188" alt="IMG_1977" src="https://github.com/user-attachments/assets/d1a3c1a0-33fb-49bd-bb29-6a78406a859c" />
<img width="1800" height="1197" alt="IMG_1971" src="https://github.com/user-attachments/assets/031824df-74d6-473f-85f5-e8444925a4aa" />


# Recent Apps for Wii U

Recent Apps is an Aroma/WUPS plugin for the Wii U that displays HOME as a separate, unnumbered first entry, followed by up to nine recently used titles numbered 01–09. Select and launch titles from the on-screen menu, including during startup sequences.

## Features

- Tracks up to nine recently used titles and keeps the newest title first, with a separate HOME entry.
- Displays title names from installed-title metadata
- **Early Title Switching:** Launch another title or return to HOME during startup, without waiting for the current game to finish loading.
- **In-Game Overlay:** Open Recent Apps with **TV + L** over ANY active games or applications.
- **Dynamic UI Colors:** Distinct background color blocks make individual entries easy to distinguish.
- Opens and closes the Recent Apps notification with a controller shortcut.
- Lets you move through recorded titles and launch the selected title.
- **Persistent History:** Recent-title entries and their order are saved between sessions.
- **Persistent Color Settings:** Your color space choices are saved and restored between sessions.
- Plays Nice with the aroma plugin config menu.

## Controls

Open the menu with **TV + L**. While it is open, use **Up/Down** to move through HOME and recent titles, then press **TV + A** to open HOME or launch the selected title. Press **TV + L** again or press **B** to close the menu.

Up/Down and B are intercepted while the menu is open so they do not also move the HOME Menu cursor or trigger the native Back action.

## Requirements

- A Wii U running Aroma
- NotificationModule (`notifications.wms`) for the on-screen notification interface
- The WUT/WUPS plugin toolchain for local builds
- Docker for the provided container build workflow

## Build locally

With the Wii U plugin toolchain installed and `DEVKITPRO` configured:

```sh
make clean
make
```

The standard WUPS rules produce `RecentAppMenu.wps` in the repository root.

## Build with Docker

From the repository root:

```sh
docker build --pull -t recent-apps-builder .
docker run --rm -v "$PWD:/project" recent-apps-builder sh -lc 'make clean && make'
```

The GitHub Actions workflow builds the plugin and uploads the resulting WPS package as an artifact. A successful CI build confirms compilation and packaging; it does not replace testing on real Wii U hardware.

## Install

Copy `RecentAppMenu.wps` to the plugins directory in your Aroma environment, for example:

```text
sd:/wiiu/environments/aroma/plugins/RecentAppMenu.wps
```

Use the artifact named `RecentAppMenu-Plugin` from a successful GitHub Actions run, or build the package locally.

## Notes

- The history holds up to nine distinct recently used titles, in addition to the separate HOME entry. The list and its order persist between sessions; launching a title already in history moves it to the newest position.
- Color space choices persist between sessions.
- Health and Safety is filtered from automatic history updates. An explicit launch from Recent Apps may add it to the list.
- NotificationModule must be available for the on-screen notification interface. If it cannot initialize, history tracking can still run but the notification UI will not be available.
- The plugin's configuration menu includes status information for history storage, shortcut registration, notifications, and diagnostic GX2 hooks.

## License

Licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).
