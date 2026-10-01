# f8implayer

This repository owns its source, tests, `extension.json`, service manifests and model metadata.
The Studio superbuild can check it out unchanged at `extensions/f8implayer`.

The SDK source revision used by CI is `d0e42420d31bc8b5bf760be132a35df953d34e49`. Checkout `feel8-fun/f8sdk` at that
revision into `.sdk` before `pixi install`; only the public SDK is a runtime dependency.
Use the commands in `.github/workflows/quality.yml` for local build/test parity.

Python releases contain this package's wheel contents and reuse the official environment
named by `extension.json`. Native releases contain deployed executables and their runtime
libraries. `python -m f8pysdk.extension_packaging` verifies the real `--describe` entrypoints,
then produces an importable ZIP and its SHA-256. No editable source path is included.

The initial source export is a snapshot. Original commit history remains in the superbuild;
use `git subtree split --prefix=extensions/f8implayer` if full package history is needed.

Service manifests use named package, bundle, and writable roots. See the [SDK service path contract](https://github.com/feel8-fun/f8sdk/blob/d0e42420d31bc8b5bf760be132a35df953d34e49/docs/service-paths.md). Package metadata stays inside the installation; user models and settings use separate writable roots.
