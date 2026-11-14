"""Extract pinned, credited source-sheet poses into editable follower strips.

Recipes in assets/pokemon_followers/manifest.json retain the source rectangles,
body pivots and loop order. Source sheets, attribution and pixels stay intact.
"""

import argparse
from collections import deque
import hashlib
import json
from pathlib import Path

from PIL import Image

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
        left, top, right, bottom = pose["box"]
        pivot_x, pivot_y = pose["pivot"]
        frame, count = Image.new("RGBA", (width, height)), 0
        for (x0, y0, x1, y1), pixels in groups:
            # Adjacent poses can overlap a rectangular crop by one pixel.
            # Include whole contained components, including detached halos.
            if left <= x0 and top <= y0 and x1 <= right and y1 <= bottom:
                for x, y in pixels:
                    dx, dy = anchor_x+x-pivot_x, anchor_y+y-pivot_y
                    assert 0 <= dx < width and 0 <= dy < height, (source, pose)
                    frame.putpixel((dx, dy), image.getpixel((x, y)))
                    count += 1
        assert count >= 100, (source, pose, "empty or partial pose")
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
