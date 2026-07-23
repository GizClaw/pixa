# PIXA editor

Run `make editor`; it starts the local server and opens <http://127.0.0.1:4173> automatically. Pass `--no-browser` to `go run ./cmd/pixa-editor` when running headlessly. The browser page accepts a local `.pixa` file or drag-and-drop input; it does not upload the file. The command is self-contained, and can also be installed with `go install github.com/GizClaw/pixa/cmd/pixa-editor@latest`.

It currently renders RGB565 key frames and exposes clip, frame, anchor, duration, and payload diagnostics. It will grow editing support in this same command. Diff frames are reported as unsupported rather than rendered speculatively.
