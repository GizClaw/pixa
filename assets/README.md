# PIXA assets

This directory contains distributable PIXA animation bundles managed by
Git LFS.

- `codex-pets/` contains the Codex desktop pet bundles; `dewey.pixa` is the
  shared integration-test fixture.

Source artwork and asset-cooking scripts remain in the product or deployment
repository that owns their production pipeline. Every committed `.pixa` file
in this directory is parsed by the Go test suite.
