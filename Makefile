.DEFAULT_GOAL := help

.PHONY: help check c-check go-check flutter-check ts-check ts-package-check tools-check editor

help:
	@printf '%s\n' \
	  'check          Run all repository checks' \
	  'c-check        Compile and test the portable C package through cgo' \
	  'go-check       Format, vet, modernize, and test Go modules' \
	  'flutter-check  Format, analyze, and test the Flutter package' \
	  'ts-check       Format, type-check, and test the TypeScript package' \
	  'ts-package-check  Pack and install the root TypeScript Git package' \
	  'tools-check    Validate the Node.js PIXA cooking tools' \
	  'editor         Start the local PIXA animation editor'

check: c-check go-check flutter-check ts-check tools-check

c-check:
	clang-format --dry-run --Werror pkgs/c/include/*.h pkgs/c/src/*.c pkgs/c/tests/*.c pkgs/c/tests/cgo/*.c
	! rg -n 'h2_pal|esp_|freertos|lvgl|#include <lvgl' pkgs/c/include pkgs/c/src
	cd pkgs/c/tests/cgo && go test ./...

go-check:
	test -z "$$(gofmt -l cmd pkgs/go)"
	go vet ./...
	modernize ./...
	go test ./...

flutter-check:
	cd pkgs/flutter && dart format --output=none --set-exit-if-changed lib test
	cd pkgs/flutter && flutter analyze
	cd pkgs/flutter && flutter test

ts-check:
	cd pkgs/typescript && npm ci
	cd pkgs/typescript && npm run format:check
	cd pkgs/typescript && npm run typecheck
	cd pkgs/typescript && npm test
	$(MAKE) ts-package-check

ts-package-check:
	cd pkgs/typescript && npm run test:package

tools-check:
	node --check tools/lib/pixa-v1.mjs
	node --check tools/webp-to-pixa.mjs
	node --check tools/gifs-to-pixa.mjs

editor:
	go run ./cmd/pixa-editor
