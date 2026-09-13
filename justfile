set shell := ["pwsh", "-NoLogo", "-NoProfile", "-Command"]

_default:
    @just --list

# Fast incremental build for local engine development.
build:
    uv tool run scons platform=windows target=editor arch=x86_64 fast_unsafe=yes

# Incremental build with conservative dependency checks.
build-safe:
    uv tool run scons platform=windows target=editor arch=x86_64 fast_unsafe=no

# Measure build configuration, SCons overhead, and command execution.
profile target *args:
    @& '{{ justfile_directory() }}/misc/scripts/profile.ps1' '{{ target }}' {{ args }}; if (-not $?) { exit 1 }; exit $LASTEXITCODE
