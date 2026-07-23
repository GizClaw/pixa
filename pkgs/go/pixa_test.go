package pixa

import (
	"os"
	"path/filepath"
	"testing"
)

func TestParseSharedAsset(t *testing.T) {
	data, err := os.ReadFile(filepath.Join("..", "..", "assets", "codex-pets", "dewey.pixa"))
	if err != nil {
		t.Fatal(err)
	}
	asset, err := Parse(data)
	if err != nil {
		t.Fatal(err)
	}
	if asset.Width == 0 || asset.Height == 0 || asset.ClipCount == 0 || asset.FrameCount == 0 || len(asset.Clips) != int(asset.ClipCount) || len(asset.Frames) != int(asset.FrameCount) {
		t.Fatalf("invalid asset metadata: %+v", asset)
	}
	if asset.Clips[0].Name != "idle" || asset.Clips[0].FrameCount == 0 || asset.Frames[0].PayloadLength == 0 {
		t.Fatalf("invalid parsed tables: clip=%+v frame=%+v", asset.Clips[0], asset.Frames[0])
	}
}

func TestParseCommittedAssets(t *testing.T) {
	assetsRoot := filepath.Join("..", "..", "assets")
	err := filepath.WalkDir(assetsRoot, func(path string, entry os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if entry.IsDir() || filepath.Ext(path) != ".pixa" {
			return nil
		}
		t.Run(filepath.ToSlash(path), func(t *testing.T) {
			data, err := os.ReadFile(path)
			if err != nil {
				t.Fatal(err)
			}
			asset, err := Parse(data)
			if err != nil {
				t.Fatal(err)
			}
			if asset.ClipCount == 0 || asset.FrameCount == 0 {
				t.Fatalf("asset has no animation data: %+v", asset)
			}
		})
		return nil
	})
	if err != nil {
		t.Fatal(err)
	}
}
