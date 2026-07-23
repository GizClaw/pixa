# PIXA

PIXA is the shared format, runtime, tools, and distributable animation repository for GizClaw clients.

The canonical format contract is [docs/format.md](docs/format.md), and the C platform boundary is documented in [docs/c-osal.md](docs/c-osal.md). Committed animations are under [assets/](assets/). Existing product repositories will consume immutable PIXA release tags rather than carrying local implementation copies.

`assets/codex-pets/dewey.pixa` is the shared PIXA example used by the Go,
Flutter, and TypeScript integration tests. Additional distributable animation
bundles are cataloged in [assets/README.md](assets/README.md).

The repository root is the Go module `github.com/GizClaw/pixa`; language
implementations live in `pkgs/`.

TypeScript consumers install the repository root using a full commit SHA:

```json
{
  "dependencies": {
    "@gizclaw/pixa": "git+https://github.com/GizClaw/pixa.git#<full-commit-sha>"
  }
}
```

Flutter consumers use the package under `pkgs/flutter`, also pinned to a full
commit SHA:

```yaml
dependencies:
  pixa:
    git:
      url: https://github.com/GizClaw/pixa.git
      ref: <full-commit-sha>
      path: pkgs/flutter
```

Go consumers use the root module and import `github.com/GizClaw/pixa/pkgs/go`:

```sh
go get github.com/GizClaw/pixa/pkgs/go@<full-commit-sha>
```

The editor is a self-contained Go command:
`make editor` starts it and opens the browser without requiring a preinstalled
Python executable. It currently provides preview and inspection only; editing
will be added to this same command. To install it for repeated use, run
`go install github.com/GizClaw/pixa/cmd/pixa-editor@latest`.

Reusable WebP-atlas and GIF cooking commands are documented in
[tools/README.md](tools/README.md).

## Local checks

Run `make help` to view available commands, or `make check` to run all installed package checks.
