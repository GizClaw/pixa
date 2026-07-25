# PIXA assets

This directory contains PIXA animation bundles managed by Git LFS. Licensing
varies by asset directory.

- `codex-pets/` contains the Codex desktop pet bundles; these files are
  expressly excluded from the repository's BSD 3-Clause License. See the
  directory's [licensing notice](codex-pets/README.md). `dewey.pixa` is the
  shared integration-test fixture.

Source artwork and asset-cooking scripts remain in the product or deployment
repository that owns their production pipeline. Every committed `.pixa` file
in this directory is parsed by the Go test suite.
