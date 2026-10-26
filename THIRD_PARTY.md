# Third-party sources

This project builds on David Churchill's STARTcraft and UAlbertaBot. Their complete repositories are fetched separately; the exact commits and local changes are recorded in `patches/sources.lock.json` and the `.patch` files.

| Dependency | Source | License |
|---|---|---|
| STARTcraft / StarterBot | https://github.com/davechurchill/STARTcraft | MIT; text in `licenses/STARTcraft-MIT.txt` |
| UAlbertaBot (including BOSS and SparCraft in that repository) | https://github.com/davechurchill/ualbertabot | See upstream license and component notices; top-level MIT text in `licenses/UAlbertaBot-MIT.txt` |
| BWAPI 4.4.0 | https://github.com/bwapi/bwapi/tree/v4.4.0 | GNU LGPL v3; original license: https://github.com/bwapi/bwapi/blob/v4.4.0/LICENSE |

`patches/STARTcraft.patch` changes the bundled BWAPI client's connection selection and StarterBot's observation callbacks. `patches/UAlbertaBot.patch` changes observation callbacks, local configuration, and Windows build settings. These patches retain their respective upstream licensing and attribution. The referenced upstream repositories retain additional component notices.

StarCraft game assets, game executables, BWAPI release binaries, injector binaries and Microsoft build tools are not distributed by this repository. The local tournament launcher was obtained from https://github.com/davechurchill/StarcraftAITournamentManager (server/required/Required_BWAPI_440.zip).