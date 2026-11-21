"""Extract pinned, credited source-sheet poses into editable follower strips.

Recipes in assets/pokemon_followers/manifest.json retain the source rectangles,
body pivots, layered parts and loop order. Source sheets and attribution stay intact.
"""

import argparse
from collections import deque
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/pokemon_followers"


def components(image, background):
    remaining = {(x, y) for y in range(image.height) for x in range(image.width)
                 if image.getpixel((x, y)) != tuple(background)}
    groups = []
    while remaining:
        start = remaining.pop()
        queue, pixels = deque([start]), [start]
        while queue:
            x, y = queue.popleft()
            for point in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
                if point in remaining:
                    remaining.remove(point)
                    queue.append(point)
                    pixels.append(point)
        bounds = (min(x for x, y in pixels), min(y for x, y in pixels),
                  max(x for x, y in pixels)+1, max(y for x, y in pixels)+1)
        groups.append((bounds, pixels))
    return groups


def render_layer(image, groups, layer, canvas_size, origin):
    """Place an intact source part, optionally mirrored/rotated around its joint."""
    left, top, right, bottom = layer["box"]
    tile = Image.new("RGBA", (right-left, bottom-top))
    count = 0
    for (x0, y0, x1, y1), pixels in groups:
        if left <= x0 and top <= y0 and x1 <= right and y1 <= bottom:
            for x, y in pixels:
                tile.putpixel((x-left, y-top), image.getpixel((x, y)))
                count += 1
    assert count > 0, (layer, "empty source part")
    width, height = tile.size
    joint_x, joint_y = layer.get("joint", [0, 0])
    if layer.get("flip_x"):
        tile = ImageOps.mirror(tile)
        joint_x = width-1-joint_x
    if layer.get("flip_y"):
        tile = ImageOps.flip(tile)
        joint_y = height-1-joint_y
    if "joint_at" in layer:
        assert 0 <= joint_x < width and 0 <= joint_y < height, layer
        radius = math.ceil(max(math.hypot(x-joint_x, y-joint_y)
                               for x, y in ((0, 0), (width-1, 0), (0, height-1), (width-1, height-1)))) + 2
        # An odd square keeps the source joint on the same pixel during rotation.
        padded = Image.new("RGBA", (radius*2+1, radius*2+1))
        padded.paste(tile, (radius-joint_x, radius-joint_y))
        tile = padded.rotate(layer.get("angle", 0), resample=Image.Resampling.NEAREST)
        x, y = layer["joint_at"]
        x -= radius
        y -= radius
    elif "at" in layer:
        x, y = layer["at"]
    else:
        pivot_x, pivot_y = layer["pivot"]
        x, y = origin[0]+left-pivot_x, origin[1]+top-pivot_y
    bounds = tile.getbbox()
    assert bounds and 0 <= x+bounds[0] < x+bounds[2] <= canvas_size[0] and 0 <= y+bounds[1] < y+bounds[3] <= canvas_size[1], (layer, "part clipped")
    result = Image.new("RGBA", canvas_size)
    result.paste(tile, (x, y))
    return result


def extract(entry):
    recipe = entry["sheet_extract"]
    source = ASSETS / recipe["file"]
    assert hashlib.sha256(source.read_bytes()).hexdigest() == recipe["sha256"], source
    image = Image.open(source).convert("RGBA")
    groups = components(image, recipe["background"])
    width, height = entry["frame_width"], entry["frame_height"]
    anchor_x, anchor_y = recipe["origin"]
    poses = []
    for pose in recipe["poses"]:
        layers = pose.get("layers", [pose])
        frame = Image.new("RGBA", (width, height))
        for layer in layers:
            piece = render_layer(image, groups, layer, (width, height), (anchor_x, anchor_y))
            if len(layers) == 1:
                frame = piece
            else:
                frame.alpha_composite(piece)
        assert sum(bool(p[3]) for p in frame.getdata()) >= 100, (source, pose, "empty or partial pose")
        poses.append(frame)
    sequence = recipe["sequence"]
    assert len(sequence) == entry["frames"]
    strip = Image.new("RGBA", (width * len(sequence), height))
    for index, pose in enumerate(sequence):
        strip.paste(poses[pose], (index * width, 0))
    return strip


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    path = ASSETS / "manifest.json"
    manifest = json.loads(path.read_text())
    for entry in manifest["assets"]:
        if "sheet_extract" not in entry:
            continue
        strip, target = extract(entry), ASSETS / entry["file"]
        if args.check:
            current = Image.open(target).convert("RGBA")
            assert current.size == strip.size and current.tobytes() == strip.tobytes(), target
            assert hashlib.sha256(target.read_bytes()).hexdigest() == entry["sha256"], target
        else:
            strip.save(target, compress_level=9)
            entry["sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
        print(f"{'Verified' if args.check else 'Extracted'} {entry['file']}: {entry['frames']} frames")
    if not args.check:
        path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
