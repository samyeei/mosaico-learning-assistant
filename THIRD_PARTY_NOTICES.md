# Third-party notices

The noncommercial license applies to this project's original code and contributions, not to upstream rights.

- `projects/learning_assistant/main/board_display.c` and its header derive from the ESP-Mosaico Hello World display adapter; Apache-2.0, retained separately in LICENSES/Apache-2.0.txt. Upstream ESP-Mosaico / Espressif notices are preserved.
- ESP-Mosaico utils, BSP and Raylib Lite are external Git submodules. Each keeps its own upstream license; this project does not relicense them. SDK/registry dependencies likewise retain their licenses.
- Noto Sans SC / Source Han-derived font: SIL OFL 1.1. Full notice: projects/learning_assistant/ui/fonts/OFL.txt.
- DejaVu / Bitstream Vera glyphs: original font notices at projects/learning_assistant/ui/fonts/LICENSE. LearningSans combines the supplied Chinese font with missing DejaVu math glyphs; these font licenses remain applicable. They are not placed under the noncommercial software license.
- Glass icons and project cover were generated with ChatGPT; the cover is labelled AI visual illustration. Some app shapes/waves are authored as SVG. No third-party trademark or brand affiliation is granted.
- Device photographs are cropped demonstration materials supplied by the project owner. The illustrative selection screenshot is explicitly test data. Raw footage, room conversations and personal worksheet headers are excluded.

Prebuilt firmware links upstream SDK/components and includes licensed fonts. Its upstream portions retain their licenses; the application author's noncommercial terms apply to the original learning-assistant portions.

For v1.1.0, utils is pinned to `be151ae047014e79a9c7ae0092dd203844499003`, BSP to `5f88b91e1be055d1d3674364123b2779a1758377`, and ESP-GSP to 1.5.1. Current registry and upstream license copies are under `LICENSES/v1.1.0/`; the application lock file lists exact dependency versions. Earlier copies under `LICENSES/upstream/` remain historical notices for v1.0.0 and its derived code. Raylib Lite is not used by the learning-assistant build.
