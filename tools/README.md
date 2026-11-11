# Scripts

This repo keeps e-paper asset generators ported from the sibling `followup`
firmware repo.

## E-paper Assets

Source artwork lives in:

- `assets/icons/`
- `assets/logos/`
- `fonts/`

The generators emit monochrome C++ assets for the SSD1677 e-paper UI:

- `generate_epaper_icons.py`: fixed `36x36` icon assets
- `generate_epaper_footer_icons.py`: fixed `44x44` footer icon assets
- `generate_epaper_logos.py`: aspect-ratio-preserving logo assets
- `generate_epaper_fonts.py`: packed ASCII bitmap fonts from TTF files
- `generate_epaper_project_assets.py`: manifest-driven wrapper for all project
  image assets

The generators run on any OS: PNGs are loaded with Pillow and fonts are rasterized with
FreeType (`pip install -r tools/requirements.txt`). See "Fidelity vs. the original macOS
pipeline" in `docs/asset-generation.md` for how the output compares with the old
`sips`/CoreText version.

For normal UI work, update `assets/epaper_assets.json` and run:

```bash
python3 tools/generate_epaper_project_assets.py
```

The lower-level scripts remain available for quick one-off experiments.

Examples:

```bash
python3 tools/generate_epaper_icons.py \
  --output-header firmware/components/core/project_assets/generated_epaper_icons.h \
  --output-source firmware/components/core/project_assets/generated_epaper_icons.cpp \
  assets/icons/home.png:kHome \
  assets/icons/settings.png:kSettings

python3 tools/generate_epaper_logos.py \
  --output-header firmware/components/core/project_assets/generated_epaper_logos.h \
  --output-source firmware/components/core/project_assets/generated_epaper_logos.cpp \
  assets/logos/folloup-logo.png:kFollowupLogo

python3 tools/generate_epaper_fonts.py \
  --output firmware/components/core/epaper_ui/generated_epaper_fonts.cpp \
  fonts/Inter_18pt-SemiBold.ttf:kInter22SemiBold:22
```

Do not hand-edit generated asset files once they are added. Update the source
PNG/TTF files and regenerate instead.
