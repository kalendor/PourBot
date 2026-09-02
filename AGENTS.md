# PourBot project instructions

For every firmware behavior or UI change:

1. Increment the patch segment in `src/config/version.h` unless the user explicitly requests a different semantic-version increment.
2. Build the release firmware successfully.
3. Run `tools/publish-ota.ps1 -SkipBuild` after the successful build.
4. Verify `docs/ota.json` and `docs/manifest.json` advertise the same version as `src/config/version.h`.
5. Verify `docs/firmware/pourbot-<version>.bin` exists and was produced by the current build.
6. Include the versioned binary and both metadata files whenever committing and pushing the change to GitHub so GitHub Pages OTA updates are current.

Do not report an OTA-ready release as complete if these artifacts are missing or out of sync.
