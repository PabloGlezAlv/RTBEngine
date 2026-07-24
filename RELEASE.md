# Release — RTBEngine

Publishes the **engine SDK** (`RTBEngine_SDK`) as a zip on GitHub Releases.

## When it runs

When you push a semver tag (for example `0.10.0`) to the remote. The workflow is triggered by the tag push, not by every push to `main`.

## Create a release

```bash
git checkout main
git pull
git tag 0.10.0
git push origin 0.10.0
```

The workflow [`.github/workflows/release.yml`](.github/workflows/release.yml) produces:

- `RTBEngine_SDK-0.10.0-win-x64.zip` — headers, `RTBEngine.dll`, Release libs, runtime DLLs, and `Default/`

## Repository prerequisites

**ThirdParty** binaries required to build (~135 MB) are committed directly in the repository (no Git LFS).

```bash
# After cloning, binaries are already present under RTBEngine/ThirdParty/.
# If Assimp for the runner toolset is missing, CI installs it via vcpkg (cached).
```

## MSVC toolset

- Local (VS 2026): `vc145` / `v145`
- GitHub Actions (`windows-latest`, VS 2022): `vc143` / `v143` (auto-detected in `scripts/ci-common.ps1`)

## Alignment with RTBEngineEditor

Use the **same tag** in both repos when publishing an editor version (the editor workflow checks out this repo at that tag).
