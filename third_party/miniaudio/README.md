# miniaudio provider

- Source: https://github.com/mackron/miniaudio
- Version: 0.11.25
- Upstream commit: `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`
- Source artifact: `miniaudio.h` from the `0.11.25` tag
- SHA-256: `miniaudio.h` =
  `ac7af4de748b7e26b777f37e01cee313a308a7296a3eb080e2906b320cc55c89`;
  `LICENSE` =
  `457f1b500e0adf6bc059edddfa78a2f62012e7c3bb43476c20e0bd23b25ba0eb`.
- License: public domain or MIT-0; AeroGUI distributes the included `LICENSE`
  file under MIT-0.
- Owner: AeroGUI Runtime
- Purpose: private, built-in cross-platform playback provider for `Aero::Audio`.
- Integration: `src/audio/Audio.cpp` is the sole translation unit defining
  `MINIAUDIO_IMPLEMENTATION`; miniAudio types do not appear in public headers.
- Build option: `AERO_WITH_MINIAUDIO=ON` (default). `OFF` keeps the public
  `Aero::Audio` API but reports `Unsupported` from playback initialization.
- Patches: none.
- Update policy: review upstream release notes and security advisories, update
  this manifest and the copied license together, and exercise the audio smoke
  path on Windows, macOS, Linux, and Android before changing the pinned tag.
