# ME2CoalFix

## About

An open source alternative to the one random binary utility I found recommended for
fixing Mass Effect 2's `Coalesced.ini` after editing it.

The latest version will attempt to fix files broken after editing them in Windows'
Notepad application. It will also automatically convert CRLF line endings to the
proper LR ones.

### Usage

If compiled for Windows, the utility should get the path to Coalesced.ini automagically.
Should that fail, it will default to `./Coalesced.ini`. You may also specify the path
manually by running the utility with the full path as its first argument.

```bash
./me2-coalfix "/path/to/Coalesced.ini"
```

## Download

Head over to [Releases](https://gitgud.io/orochi/mods/mass-effect/me2-coalfix/-/releases) and download the latest release matching your platform.

## Reporting Bugs

Open a new Issue, or [email me](https://phobos.gitgud.site/about/).

## License

[GNU Affero General Public License v3.0 only](/LICENSE)
