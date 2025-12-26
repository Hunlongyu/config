# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial release of the Config Library.
- **Core**: Thread-safe `ConfigStore` with JSON backend.
- **Strategies**:
  - `Path`: Absolute, Relative, AppData.
  - `Save`: Auto, Manual.
  - `Get`: DefaultValue, ThrowException.
- **Obfuscation**: Base64, Hex, ROT13, Reverse, Combined.
- **Listeners**: Callback system for configuration changes.
- **Global API**: Convenient `config::get`, `config::set` wrappers.
- **CI/CD**: GitHub Actions for Linux/Windows/macOS testing and release.
- **Docs**: Comprehensive API reference and build guide (English & Chinese).
