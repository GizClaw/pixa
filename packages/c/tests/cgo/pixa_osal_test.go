package cgo

import "testing"

func TestHostOSALABI(t *testing.T) {
	if got := hostOSALSmoke(); got != 0 {
		t.Fatalf("pixa_osal host callback returned %d", got)
	}
}
