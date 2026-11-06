# Contributing to AuroraSync BLE OTA

Thanks for taking the time to contribute! This guide explains how to report issues,
propose changes and get them merged.

This repository holds a fork of Espressif's `ble_ota`, published on the
[ESP Component Registry](https://components.espressif.com/) as `aurorasynclabs/ble_ota`.
It is maintained as-is, for AuroraSync's own firmware: issues and pull requests are read,
with no promised reply time.

| Directory                             | Content                             |
|---------------------------------------|-------------------------------------|
| `ble_ota/src/`                        | Component sources                   |
| `ble_ota/include/`                    | Public header (`ble_ota.h`)         |
| `ble_ota/examples/minimal/`           | Example project using the component |
| `ble_ota/test/`, `ble_ota/test_apps/` | Unit tests and the app running them |
| `ble_ota/Kconfig`                     | Component configuration             |
| `tools/`                              | Test client, not published          |
| `security/`                           | Security audit reports              |

CI builds and tests with ESP-IDF v6.1.

## Reporting a bug

Open an issue using the **Bug report** template (`.github/ISSUE_TEMPLATE/bug_report.md`).
The title is prefixed with `[BUG]` and the `bug` label is applied automatically.

Fill in every section of the template, and give:

- the dev board, the component version and the ESP-IDF version,
- the OTA type (plain or pre-encrypted) and whether `CONFIG_BLE_OTA_EXTERNAL_HOST` is set,
- the client sending the firmware (app, phone, OS),
- the device logs.

Search the existing issues first. A bug in the Bluedroid or protocomm backends goes to
[upstream](https://github.com/espressif/esp-iot-solution/issues): this fork removed them.

## Reporting a vulnerability

Do not open a public issue. Report it privately through
[Report a vulnerability](https://github.com/AuroraSyncLabs/BleOTA/security/advisories/new), with the
steps to trigger it and its impact. The design notes in [`security/README.md`](security/README.md)
list known limits that are not treated as vulnerabilities.

A fixed vulnerability gets a report in `security/` and a regression test tagged `[security]`.

## Requesting a feature

Open an issue using the **Feature request** template (`.github/ISSUE_TEMPLATE/feature_request.md`).
The title is prefixed with `[FEATURE]` and the `enhancement` label is applied automatically.
Describe the problem, the solution you would like and the alternatives you considered.

## Scope of contributions

| Area                                                | Status                                                      |
|-----------------------------------------------------|-------------------------------------------------------------|
| Docs, examples, tests, bug fixes, new targets       | Open                                                        |
| Public API (`ble_ota.h`), Kconfig options           | Discuss in an issue first: changes break dependent projects |
| BLE protocol ([`PROTOCOL.md`](ble_ota/PROTOCOL.md)) | Discuss in an issue first: every client has to follow       |
| Bluedroid, protocomm or other BLE hosts             | Out of scope: use upstream `espressif/ble_ota`              |

A pull request is accepted when it is linked to an issue, does one thing, keeps the example building
and the tests passing, and updates `PROTOCOL.md` or `ble_ota.h` if it changes behavior.

It is declined, always with a written reason, when it breaks the API or the protocol without agreement,
brings back a removed backend, or mixes unrelated changes.

## Working on an issue

1. Comment on the issue to say you are taking it.
2. Fork the repository and create a branch from `main`, named after the issue as GitHub generates it:
   `feature/<issue-number>-<short-description>`.
3. Open a pull request from your fork against `main` (see [Pull requests](#pull-requests)).

## Commit messages

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary in imperative mood>
```

- **Types**: `feat`, `fix`, `refactor`, `test`, `docs`, `ci`, `chore`.
- **Scopes**: only on `docs` commits, naming the file: `readme`, `protocol`, `changelog`, `security`,
  `contributing`.
- Mark breaking changes with `!`, e.g. `refactor!: remove the protocomm backend`, for any change that
  breaks `ble_ota.h`, the Kconfig options or the protocol.
- A bug fix comes with a test that fails without it, committed first.

Examples:

```
test: refuse sectors larger than 4096 bytes
fix: refuse sectors larger than 4096 bytes
docs(protocol): match the code behavior
```

A file changed from upstream keeps its Espressif SPDX header and gets a `SPDX-FileContributor` line,
as the [Apache-2.0](LICENSE) license requires.

## Building and testing

Source the ESP-IDF environment first:

```shell
# Bash, ZSH
. /path/to/.espressif/v6.1/esp-idf/export.sh

# Fish
. /path/to/.espressif/v6.1/esp-idf/export.fish

# Windows PowerShell
. C:\path\to\esp-idf\export.ps1

# Windows Command Prompt
C:\path\to\esp-idf\export.bat
```

The component is built through its example, which points to the local sources with `override_path`.
From `ble_ota/examples/minimal/`:

```shell
idf.py set-target esp32c6
idf.py build
# With the board connected
idf.py flash monitor
```

ESP32-S31 is a preview target: add `--preview` to every `idf.py` command.

### Checks

CI runs on every pull request (`.github/workflows/ci.yml`): formatting, `idf.py clang-check`,
unit tests on QEMU and the example builds. From `ble_ota/`:

```shell
# Formatting, pinned version through uv (https://docs.astral.sh/uv/)
git ls-files '*.c' '*.h' | xargs uvx clang-format==23.1.1 --style=file -i
# Static analysis (needs `idf_tools.py install esp-clang`), findings in warnings.txt
cd examples/minimal && IDF_TOOLCHAIN=clang idf.py set-target esp32c6 \
  && idf.py clang-check --include-paths "$(realpath ../..)" \
     --exclude-paths "$(realpath managed_components)" \
     --run-clang-tidy-options "-header-filter=ble_ota/"
```

CI fails above `MAX_WARNINGS` clang-tidy findings: a change must not add any.

Unit tests live in `test/test_ble_ota.c` (`TEST_CASE("<behavior>", "[ble_ota]")`) and run
through `test_apps/` with [pytest](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/contribute/esp-idf-tests-with-pytest.html)
(`install.sh --enable-ci`, plus `idf_tools.py install qemu-riscv32` for QEMU). From `ble_ota/`:

```shell
# QEMU, as in CI
idf-ci build run -p test_apps -t esp32c3 -m qemu && pytest -m qemu --target esp32c3
# Board
idf-ci build run -p test_apps -t esp32c6 -m generic && pytest -m generic --target esp32c6
```

## Pull requests

1. Open the pull request against `main`.
2. Fill in the pull request template: the description, the related issue (`Fixes #<issue-number>`),
   the type of change, how it was tested and the checklist.
3. The maintainer reviews it and you address the feedback. It is merged once CI is green.

## Releases

Releases are made by the maintainer:

1. Add the release to `ble_ota/CHANGELOG.md`.
2. Bump `version` in `ble_ota/idf_component.yml`, following [Semantic Versioning](https://semver.org/)
   (a breaking change bumps the major version).
3. Push a tag `vX.Y.Z` matching that version.

The upload component workflow then publishes the component to the ESP Component Registry.

## Repository traffic

GitHub keeps 14 days of traffic. The traffic workflow (`.github/workflows/traffic.yml`) saves it every Monday,
or on demand from the Actions tab, to the [`traffic`](https://github.com/AuroraSyncLabs/BleOTA/tree/traffic) branch:

| File                                       | Content                                          |
|--------------------------------------------|--------------------------------------------------|
| `views.csv`, `clones.csv`                  | Views and clones per day, updated by each run    |
| `repository.csv`                           | Stars, forks and watchers on each run day        |
| `referrers/<date>.csv`, `paths/<date>.csv` | Top 10 referrers and pages over the last 14 days |

It reads the traffic with the `TRAFFIC_TOKEN` secret: a fine-grained token created by an organization owner,
with resource owner `AuroraSyncLabs`, access to the `BleOTA` repository and the "Administration: read" permission.
Renew it before it expires, or the workflow fails.

## Questions

If something is unclear, open an issue or reach out to the [maintainer](README.md#maintainer).
