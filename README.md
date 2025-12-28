<h1>cpiped</h1>

[![Release][release-shield]][release-url]
![GitHub commit activity][commit-shield]
[![Issues][issues-shield]][issues-url]
[![Contributors][contributors-shield]][contributors-url]
[![project_license][license-shield]][license-url]
[![Stargazers][stars-shield]][stars-url]
[![Forks][forks-shield]][forks-url]
![Build flow][build-shield]

    cpiped captures an audio stream and outputs it to a pipe

`cpiped` was born  to capture line-in audio from a sound card and send it to [forked-daapd](https://github.com/ejurgensen/forked-daapd). On sound detection, it runs a simple script to start playing the associated pipe in forked-daapd.

## Contents
- [Contents](#contents)
- [Features](#features)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
- [Usage](#usage)
  - [Options](#options)
  - [Env variables](#env-variables)
- [ToDo(s)](#todos)
- [Acknowledgments](#acknowledgments)
  - [Top contributors:](#top-contributors)

## Features
- Buffering to handle clock mismatch
- Command execution on sound and silence detection
- User defined noise threshold  
- Different sample rate handling
- Multiple instances running in parallel thanks to parametrized pid

## Getting Started

### Prerequisites
`cpiped` relies on [ALSA lib]( http://www.alsa-project.org) as only prerequisite

- Debian
  ```sh
  sudo apt install -y libasound2-dev
  ```

### Installation
1. Clone the repository
   ```sh
   git clone https://github.com/ale275/cpiped.git
   ```
2. Make cpiped
   ```sh
   cd cpiped
   make
   ```
4. Install cpiped
   ```sh
   sudo install cpiped /usr/local/sbin/
   ```

## Usage

`cpiped` is an application meant to be run from command line or demonized

```sh
cpiped [-s arg] [-e arg] [OPTIONS] FIFO
```
FIFO: path to the named pipe where sound will be written

### Options

| Option | Description                                                   | Type      | Default                 | Range            | Required? |
| ------ | ------------------------------------------------------------- | --------- | ----------------------- | ---------------- | --------- |
| `-d`   | ALSA capture device in *hw:\<card>,\<device>* format.         | `string`  | `'default'`             |                  | Yes       |
| `-f`   | ALSA capture device sample rate in Hz.                        | `integer` | `44100`                 | `16000 - 192000` | No        |
| `-b`   | Buffer size in seconds.                                       | `float`   | `.25`                   | `.1 - 5.0`       | No        |
| `-s`   | Command to run, in background, when sound is detected.        | `string`  |                         |                  | Yes       |
| `-e`   | Command to run, in background, when silence is detected.      | `string`  |                         |                  | Yes       |
| `-t`   | Silence threshold expressed as signal power.                  | `integer` | `100`                   | `1 - 32767`      | No        |
| `-D`   | Daemonize.                                                    | `bool`    |                         |                  | No        |
| `-p`   | Path to pidfile.                                              | `string`  | `'/var/run/cpiped.pid'` |                  | No        |
| `-v`   | Log verbosity. (`-vv` and `-vvv` for higher verbosity levels) | `bool`    |                         |                  | No        |

### Env variables
On start `cpiped` will set the following env variables
- *CPIPED_SR* audio capture sample rate
- *CPIPED_SS* audio capture sample size
- *CPIPED_CC* audio capture channel(s) count

Those variables are visible by the sound and silence detect commands

## ToDo(s)
- [ ] Improve Makefile to create dedicated build folder
- [ ] Improve Makefile to add install directive

See the [open issues](https://github.com/ale275/cpiped/issues) for a full list of proposed features (and known issues).

## Acknowledgments

* [cpiped](https://github.com/b-fitzpatrick/cpiped) original code of cpiped by [b-fitzpatrick](https://github.com/b-fitzpatrick)
* [NDpiped](https://github.com/natedreger/NDpiped) modified version writing the buffer only when sound was detected
* [maweki cpiped fork](https://github.com/maweki/cpiped) for parametrized pid file

### Top contributors:

<a href="https://github.com/ale275/cpiped/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=ale275/cpiped" alt="contrib.rocks image" />
</a>

<!-- variables -->
[build-shield]: https://github.com/ale275/cpiped/actions/workflows/cpiped-build.yml/badge.svg
[commit-shield]: https://img.shields.io/github/commit-activity/t/ale275/cpiped?style=flat
[release-shield]: https://img.shields.io/github/release/ale275/cpiped.svg?colorB=58839b
[release-url]: https://github.com/ale275/cpiped/releases/latest
[contributors-shield]: https://img.shields.io/github/contributors/ale275/cpiped.svg
[contributors-url]: https://github.com/ale275/cpiped/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/ale275/cpiped.svg?style=flat
[forks-url]: https://github.com/github_usale275ername/cpiped/network/members
[stars-shield]: https://img.shields.io/github/stars/ale275/cpiped.svg?style=flat
[stars-url]: https://github.com/ale275/cpiped/stargazers
[issues-shield]: https://img.shields.io/github/issues/ale275/cpiped.svg
[issues-url]: https://github.com/ale275/cpiped/issues
[license-shield]: https://img.shields.io/github/license/ale275/cpiped.svg
[license-url]: https://github.com/ale275/cpiped/blob/master/LICENSE