# PIXA

PIXA is the shared format, runtime, tools, and distributable animation repository for GizClaw clients.

The canonical format contract is [docs/format.md](docs/format.md), and the C platform boundary is documented in [docs/c-osal.md](docs/c-osal.md). Committed animations are under [assets/](assets/). Existing product repositories will consume immutable PIXA release tags rather than carrying local implementation copies.

`assets/dewey.pixa` is the shared PIXA example used by the Go, Flutter, and TypeScript integration tests.

The repository root is the Go module `github.com/GizClaw/pixa`; language
implementations live in `pkgs/`. The editor is a self-contained Go command:
`make editor` runs it without requiring a preinstalled Python executable. It
currently provides preview and inspection only; editing will be added to this
same command. To install it for repeated use, run
`go install github.com/GizClaw/pixa/cmd/pixa-editor@latest`.

## Local checks

Run `make help` to view available commands, or `make check` to run all installed package checks.
