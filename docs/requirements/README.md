# Artie Requirements

[Back to Contributing](../../CONTRIBUTING.md)

This document describes the Artie Requirements for the various software and hardware components.

Each component's technical requirements live in its own repository, next to that component's
contribution guide. This page is the index, and holds the glossary and the requirements that
cover the whole project.

| Requirements | Component | IDs | Contribution guide |
| ----- | ----- | :---: | ----- |
| [Documentation](#documentation) | Artie Framework documentation | D | [Contributing](../../CONTRIBUTING.md) |
| [Artie Tool](https://github.com/ArtieBots/ArtieTool/blob/main/docs/requirements/artie-tool-requirements.md) | Artie Tool | T, TB, TD, TF, TG, TI, TT, TE | [Artie Tool](https://github.com/ArtieBots/ArtieTool/blob/main/docs/contributing/artie-tool-contributions.md) |
| [Artie Workbench](https://github.com/ArtieBots/ArtieWorkbench/blob/main/docs/requirements/artie-workbench-requirements.md) | Artie Workbench | W | [Artie Workbench](https://github.com/ArtieBots/ArtieWorkbench/blob/main/docs/contributing/artie-workbench-contributions.md) |
| [Artie CLI](https://github.com/ArtieBots/ArtieCLI/blob/main/docs/requirements/artie-cli-requirements.md) | Artie CLI | - | [Artie CLI](https://github.com/ArtieBots/ArtieCLI/blob/main/docs/contributing/artie-cli-contributions.md) |
| [Helm Charts](https://github.com/ArtieBots/ArtieTool/blob/main/docs/requirements/chart-requirements.md) | ArDK's base Helm Chart and its software components | SC | [Charts](https://github.com/ArtieBots/ArtieTool/blob/main/docs/contributing/chart-contributions.md) |
| [Libraries](https://github.com/ArtieBots/ArDK/blob/main/docs/requirements/library-requirements.md) | ArDK CAN library, firmware libraries, software libraries, base images | CL, FL, SL, BI | [Libraries](https://github.com/ArtieBots/ArDK/blob/main/docs/contributing/library-contributions.md) |
| [Firmware](./firmware-requirements.md) | Firmware bootloader | FW | [Firmware](../contributing/firmware-contributions.md) |
| [Yocto Images](./yocto-image-requirements.md) | Controller node and Jetson images | YC, YJ | [Yocto Images](../contributing/yocto-image-contributions.md) |
| [Drivers](./driver-requirements.md) | Drivers | - | [Drivers](../contributing/driver-contributions.md) |
| [Simulator](./simulator-requirements.md) | Artie Simulator | AS | [Simulator](../contributing/simulator-contributions.md) |
| [Artie00 Electrical](https://github.com/ArtieBots/Artie00/blob/main/docs/requirements/electronic-design-requirements.md) | Artie00 electronics | - | [Electronics Design](https://github.com/ArtieBots/Artie00/blob/main/docs/contributing/electronic-design.md) |
| [Artie00 Mechanical](https://github.com/ArtieBots/Artie00/blob/main/docs/requirements/mechanical-design-requirements.md) | Artie00 mechanics | - | [Mechanical Design](https://github.com/ArtieBots/Artie00/blob/main/docs/contributing/mechanical-design.md) |

Requirement IDs are unique across the whole project, so that an ID alone identifies a
requirement wherever it is referenced.

## Glossary

* **Artie Instance:** A single instance of Artie. This includes all the software and hardware required for an Artie to run.
* **Artie type:** A distinct set of hardware integrated with the Artie software stack. Some Artie types are official \- ones that we know about, build, test, and support \- and others may be unknown to us, custom Arties used by users of the Artie software ecosystem.
* **Artie Framework:** The tooling applications and libraries in the Artie project.
* **Artie Release:** An officially recognized combination of hardware, software, and documentation known to be working and pinned to a GitHub release and git tag.
* **Artie00:** Our first official Artie. A full-term newborn.
* **SBC:** Single Board Computer (such as a Raspberry Pi or NVIDIA Jetson)

## Artie Framework

Artie Framework is the set of software tools, software libraries, and documentation that enable Artie instances to be built and administered.

### Documentation

The Artie Framework must be documented thoroughly in order to enable its use and experimental reproduction.

| ID | Name | Description | Milestone |
| :---: | :---: | ----- | :---: |
| D1 |  |  |  |
