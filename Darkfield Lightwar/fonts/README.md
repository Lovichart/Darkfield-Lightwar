# Fonts

The public repository/release ships **only** `LXGWWenKaiTC-Light.ttf`
(霞鹜文楷, SIL Open Font License 1.1, see `../licenses/OFL-1.1.txt`).

`Renderer::init` tries these files in order and skips any that are missing:

1. `fonts/LXGWWenKaiTC-Light.ttf`
2. `fonts/msyh.ttc` (Microsoft YaHei — proprietary, not redistributed)
3. `fonts/SourceHanSans-Light.ttf` (OFL, optional local fallback)
4. `fonts/SmileySans-Oblique.otf` (OFL, optional local fallback)
5. `fonts/arial.ttf` (Arial — proprietary, not redistributed)
6. `C:/Windows/Fonts/msyh.ttc` (Windows system fallback)

The proprietary fonts are ignored by `.gitignore` and are **not** included
in the public repository or release zip. If you add an optional OFL font
locally, keep its license file and update `THIRD_PARTY_NOTICES.md` before
publishing a build that contains it.
