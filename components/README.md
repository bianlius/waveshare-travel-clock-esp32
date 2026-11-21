<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Shared engine components

`GVIEW/` contains the common renderer, fonts, image/animation operations and
viewport calculations. Both device targets and the desktop compile these same
sources through `main/shared_sources.txt`.

Device LCD and SPI drivers live in `ports/waveshare/components/` and
`ports/ai-passport/components/`; they are not duplicate application logic.
See [the source layout](../ports/README.md) for ownership and build entry points.
