# Työpöytäsimulaattorin kolmannen osapuolen aineistot

Riippuvuudet kuuluvat vain CMake-työpöytärakennukseen. ESP32:n
PlatformIO-riippuvuuksia ei muuteta.

| Aineisto | Versio ja alkuperä | Lisenssi |
| --- | --- | --- |
| SDL2 | [SDL 2.32.10](https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10) | zlib, lähdepaketin `LICENSE.txt` |
| Dear ImGui sekä SDL2/SDL_Renderer-sovittimet | [Dear ImGui 1.91.9b](https://github.com/ocornut/imgui/releases/tag/v1.91.9b), Omar Cornut ja muut tekijät | MIT, lähdepaketin `LICENSE.txt` |
| Roboto Medium, Christian Robertson | Edellä lukitun ImGui-paketin `misc/fonts/Roboto-Medium.ttf` | Apache License 2.0, `Roboto-LICENSE.txt` (lähdepuussa `assets/`) |

Testirakennus voi hakea myös [Unity 2.6.1:n](https://github.com/ThrowTheSwitch/Unity/tree/v2.6.1)
(MIT, lähdepaketin LICENSE.txt). Sen SHA-256 on
`b41a66d45a6b99758fb3202ace6178177014d52fc524bf1f72687d93e9867292`.
Unity ja testiohjelmat eivät kuulu ajonaikaiseen julkaisuun.

CMake-tiedosto `cmake/DesktopDependencies.cmake` lukitsee arkistot
versiotunnuksilla ja seuraavilla SHA-256-tiivisteillä:

```text
SDL:   03f9d7c191a837525c9cda6406af2f2e48be02b5e7eb03d949cc9f1e9ca41c8b
ImGui: 8e1bbc76c71d74fef2fb85db7e7ca8eba13d6a86623c54992b60162db554ffdb
```

Roboto-fontin muuttumattomat tavut sisällytetään ohjelmaan CMake-rakennuksessa.
Dear ImGui dokumentoi fontin lähteen ja lisenssin omassa
[`docs/FONTS.md`](https://github.com/ocornut/imgui/blob/v1.91.9b/docs/FONTS.md)
-tiedostossaan. Apache 2.0 -lisenssiteksti on peräisin
[Apache Software Foundationilta](https://www.apache.org/licenses/LICENSE-2.0.txt).

`cmake --install` ja CPack asentavat ASkompun oman MIT-lisenssin sekä
ImGui-, Roboto- ja haetun SDL:n lisenssit. Windowsin siirrettävässä paketissa
ne ovat EXE:n viereisessä `LISENSSIT`-hakemistossa. Linux-asennuksessa ne ovat
`share/askompu-simulaattori/LISENSSIT`-hakemistossa. Fonttitiedostoa ei
muuteta; sen sisältö ja metatiedot säilyvät EXE:hen sisällytettynä.

Järjestelmä-SDL:n kehityspolkua käytettäessä kyseisen paketin ajonaikainen
kirjasto ja lisenssit ovat kehittäjän/jakelun vastuulla; siirrettävä
Windows-julkaisupolku ei salli järjestelmä-SDL:ää. Myös suoraan
rakennushakemistosta jaettavan ohjelman mukana pitää toimittaa soveltuvat
lisenssit.
