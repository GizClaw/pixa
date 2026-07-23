# PIXA editor

Run `make editor`, then open <http://127.0.0.1:4173>. The browser page accepts a local `.pixa` file or drag-and-drop input; it does not upload the file. The command is self-contained, and can also be installed with `go install github.com/GizClaw/pixa/cmd/pixa-editor@latest`.

It currently renders RGB565 key frames and exposes clip, frame, anchor, duration, and payload diagnostics. It will grow editing support in this same command. Diff frames are reported as unsupported rather than rendered speculatively.
