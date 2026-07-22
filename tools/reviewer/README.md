# PIXA reviewer

Run `make reviewer`, then open <http://127.0.0.1:4173>. The browser page accepts a local `.pixa` file or drag-and-drop input; it does not upload the file.

It renders RGB565 key frames and exposes clip, frame, anchor, duration, and payload diagnostics. Diff frames are reported as unsupported rather than rendered speculatively.
