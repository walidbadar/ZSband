# ZSBand: Reverse Engineering of Xiaomi Mi Band

[![Documentation](https://github.com/walidbadar/ZSband/actions/workflows/docs.yml/badge.svg)](https://walidbadar.github.io/ZSband/)

## Overview

This project outlines a systematic approach to hack, reverse engineer, and eventually develop custom Zephyr RTOS based firmware for the Xiaomi Mi Band. It aims to understand the device's hardware components, and system overview to create an open-source alternative firmware.

![Xiaomi Mi Band](doc/img/mi_band.jpg)

## Documentation

Full documentation is published at **<https://walidbadar.github.io/ZSband/>**.

* [Xiaomi Mi Band 4](https://walidbadar.github.io/ZSband/boards/xiaomi/mi_band_4/doc/index.html):
  hardware details, debug pads, and build, flash and debug instructions

The documentation sources live in [doc/](doc/) and the board docs under
[boards/](boards/). To build them locally:

```console
pip install -r doc/requirements.txt
sphinx-build -b html doc doc/_build/html
```

## Project Phases

### Phase 1: Hardware Analysis

* Map debug ports (SWD, UART) and test points
* Document PCB layout and component connections
* Extract firmware through debug interfaces if accessible

### Phase 2: Zephyr Port Development

* Create device tree source for Xiaomi Mi Band
* Implement drivers for display and sensors

## Legal and Ethical Considerations

* Developing for personal use and research purposes only
* Avoiding distribution of copyrighted firmware components
* Focusing on interoperability and open standards
* Documenting for educational purposes
