# Third-party notices

Darkfield Lightwar / 暗域光战 includes or links against the following
third-party components. They are **not** covered by this project's MIT /
CC BY 4.0 licenses; their own licenses apply.

## SFML 2.6.2

- Copyright (C) 2007-2023 Laurent Gomila
- License: zlib
- Project: https://www.sfml-dev.org/
- Full text: [licenses/SFML.txt](licenses/SFML.txt)

The distributed Windows build includes `sfml-graphics-2.dll`,
`sfml-window-2.dll` and `sfml-system-2.dll`. SFML's own license lists its
bundled third-party libraries (OpenAL-Soft, stb, FreeType, libogg,
libvorbis, libflac, minimp3); their notices are part of the SFML license
file.

## LXGW WenKai / 霞鹜文楷

- Font included: `Darkfield Lightwar/fonts/LXGWWenKaiTC-Light.ttf`
- Copyright: LXGW and contributors
- License: SIL Open Font License 1.1
- Project: https://github.com/lxgw/LxgwWenKai
- Full text: [licenses/OFL-1.1.txt](licenses/OFL-1.1.txt)

This repository/release includes **only** LXGW WenKai as a bundled CJK
font. The game also has optional local fallbacks and a Windows system-font
fallback; those are not redistributed.

## Fonts that are NOT redistributed

The original development machine also had copies of:

- `msyh.ttc` — Microsoft YaHei (proprietary)
- `arial.ttf` — Arial (proprietary)
- `SourceHanSans-Light.ttf` — optional, OFL 1.1, not bundled
- `SmileySans-Oblique.otf` — optional, OFL 1.1, not bundled

The proprietary fonts are **not** part of the public repository or the
distributed release. Do not add them to a release archive.

## Microsoft Visual C++ runtime

The Windows release may include the Microsoft Visual C++ 2015-2022 x64
redistributable DLLs (`msvcp140.dll`, `msvcp140_1.dll`, `vcruntime140.dll`,
`vcruntime140_1.dll`). These are redistributed under the Microsoft Visual
Studio license terms and are not covered by this project's licenses.

## Names and other content

The faction/ship name lists under `names/` were compiled/edited for this
project and are covered by CC BY 4.0 as original non-code assets. If you
add names from another work, check that work's license and add the
appropriate attribution here.
