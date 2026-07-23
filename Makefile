.DEFAULT_GOAL := help

.PHONY: help check c-check go-check flutter-check ts-check reviewer

help:
	@printf '%s\n' \
	  'check          Run all repository checks' \
	  'c-check        Compile and test the portable C package and Go-backed cgo OSAL' \
	  'go-check       Format, vet, modernize, and test the Go package' \
	  'flutter-check  Format, analyze, and test the Flutter package' \
	  'ts-check       Format, type-check, and test the TypeScript package' \
	  'reviewer       Start the local PIXA animation reviewer'

check: c-check go-check flutter-check ts-check

c-check:
	clang-format --dry-run --Werror packages/c/include/*.h packages/c/src/*.c packages/c/tests/*.c packages/c/tests/cgo/*.c
	! rg -n 'h2_pal|esp_|freertos|lvgl|#include <lvgl' packages/c/include packages/c/src
	$(MAKE) -C packages/c check
	cd packages/c/tests/cgo && go test ./...

go-check:
	cd packages/go && test -z "$$(gofmt -l .)"
	cd packages/go && go vet ./...
	cd packages/go && modernize ./...
	cd packages/go && go test ./...

flutter-check:
	cd packages/flutter && dart format --output=none --set-exit-if-changed lib test
	cd packages/flutter && flutter analyze
	cd packages/flutter && flutter test

ts-check:
	cd packages/typescript && npm install
	cd packages/typescript && npm run format:check
	cd packages/typescript && npm run typecheck
	cd packages/typescript && npm test

reviewer:
	python3 -m http.server 4173 --directory tools/reviewer
