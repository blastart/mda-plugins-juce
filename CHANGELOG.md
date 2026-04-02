# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/), and this project adheres to [Semantic Versioning](https://semver.org/).

## [2.0.0-alpha.1] - 2026-04-02

### Added
- CMake build system — no Projucer or pre-installed JUCE required
- JUCE 8 fetched automatically via FetchContent
- 15 newly ported plug-ins from the original VST2 source: Combo, De-ess, DubDelay, Leslie, Looplex, MultiBand, RePsycho!, RoundPan, SpecMeter, TalkBox, ThruZero, Tracker, Transient, VocInput, Vocoder
- VST3, AU, CLAP, and LV2 output formats (original only had VST3)
- CLAP support via clap-juce-extensions
- VST2 replacement compatibility: original VST2 unique IDs and plugin names preserved for all 39 plug-ins
- GitHub Actions CI for macOS (ARM64 + x86_64), Windows, and Linux
- Automated release builds with pre-built binaries

### Changed
- Upgraded from JUCE 7 to JUCE 8.0.4 (required for macOS 15 Sequoia compatibility)
- Build system changed from Projucer (.jucer files) to CMake (original .jucer files preserved)

### Fixed
- Build failure on macOS 15 due to deprecated CGWindowListCreateImage API (resolved by JUCE 8 upgrade)
