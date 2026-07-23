# pixa

Flutter and Dart support for the
[PIXA animation format](https://github.com/GizClaw/pixa). The package includes
the cross-platform parser and validator plus Flutter image conversion and
painting helpers.

## Add from Git

Pin a full commit SHA in the consuming application's `pubspec.yaml`:

```yaml
dependencies:
  pixa:
    git:
      url: https://github.com/GizClaw/pixa.git
      ref: <full-commit-sha>
      path: pkgs/flutter
```

## Use

```dart
import 'dart:typed_data';

import 'package:pixa/pixa.dart';
import 'package:pixa/pixa_flutter.dart';

Future<PixaFramePainter> loadPixa(Uint8List bytes) async {
  final asset = parsePixa(bytes);
  final frame = renderPixaFrameRgba(asset, asset.clips.first.firstFrame);
  final image = await pixaFrameRgbaToImage(frame);
  return PixaFramePainter(image);
}
```

See the
[format documentation](https://github.com/GizClaw/pixa/blob/main/docs/format.md)
for the binary contract and renderer limitations.
