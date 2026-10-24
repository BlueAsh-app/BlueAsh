# BlueAsh

A free, open-source Windows tray app for gamers: one hotkey switches display colour (NVIDIA gamma, brightness, contrast,
vibrance, hue) and browser volume between a normal and a game profile, plus helpers for Escape from Tarkov (map with
extracts, quests, bosses and keys; item prices; raid start alert) and PUBG (plane boarding alert, map overlay).

게임할 때 화면 색감과 브라우저 볼륨을 단축키 하나로 바꾸고, 타르코프·배그 보조 도구를 모은 무료 오픈소스 Windows 트레이 앱입니다.

- Download: <https://blueash.pages.dev> or [GitHub Releases](https://github.com/BlueAsh-app/BlueAsh/releases/latest)
  — run `BlueAsh-Setup-<version>.exe` (the same file on both). 설치 파일 `BlueAsh-Setup-<버전>.exe`를 받아 실행하세요.
- Privacy policy: <https://blueash.pages.dev/privacy.html>
- Code signing policy: <https://blueash.pages.dev/code-signing.html>

BlueAsh never reads or changes game memory and sends no input to games. It reads the logs and settings files games leave
on the PC and the screen through Windows screen capture. Not made or endorsed by NVIDIA, Battlestate Games or KRAFTON.

## Building

See [source/BUILDING.txt](source/BUILDING.txt). [GitHub Actions](.github/workflows/build.yml) builds and tests every push and
tag; the setup program attached to each release is the one the update server ships.

Some data BlueAsh uses is not open source (Fandom map pictures and texts, PUBG reference maps). It is not in this
repository: BlueAsh downloads it at run time as a data pack whose SHA-256 is pinned in
[source/datapack_id.h](source/datapack_id.h). See [NOTICE.txt](NOTICE.txt) for every third-party part.

## Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by
[SignPath Foundation](https://signpath.org/) (application pending; releases are not signed yet).
Committers, reviewers and approvers: [BlueAsh-app](https://github.com/BlueAsh-app).

## License

GPL-3.0-or-later ([LICENSE.txt](LICENSE.txt)). Includes code adapted from
[nvBrightness](https://github.com/pbatard/nvBrightness) by Pete Batard.
