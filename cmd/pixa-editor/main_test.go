package main

import (
	"runtime"
	"testing"
)

func TestBrowserCommand(t *testing.T) {
	command, err := browserCommand("http://127.0.0.1:4173")
	if err != nil {
		t.Fatal(err)
	}

	want := map[string]string{
		"darwin":  "open",
		"linux":   "xdg-open",
		"windows": "rundll32",
	}[runtime.GOOS]
	if command.Args[0] != want {
		t.Fatalf("command = %q, want %q", command.Args[0], want)
	}
}
