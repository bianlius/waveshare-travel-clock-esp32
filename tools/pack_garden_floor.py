"""Pack the ImageGen lawn as a native-size GVIEW sprite in the original palette."""
from collections import deque
from pathlib import Path
import re
import argparse

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "assets/garden"
WIDTH, HEIGHT = 224, 98


def main_component(mask: bytes, width: int, height: int) -> bytearray:
    seen = bytearray(width * height)
    largest = []
    for start, opaque in enumerate(mask):
        if not opaque or seen[start]:
            continue
        queue = deque([start])
        seen[start] = 1
        component = []
        while queue:
            pos = queue.popleft()
            component.append(pos)
            x, y = pos % width, pos // width
            for nxt in ((pos-1 if x else -1), (pos+1 if x+1<width else -1),
                        (pos-width if y else -1), (pos+width if y+1<height else -1)):
                if nxt >= 0 and mask[nxt] and not seen[nxt]:
                    seen[nxt] = 1
                    queue.append(nxt)
        if len(component) > len(largest):
            largest = component
    result = bytearray(width * height)
    for pos in largest:
        result[pos] = 255
    return result


def closed_surface(mask: bytearray, width: int, height: int) -> bytearray:
    """Only the exterior is transparent: the lawn is one continuous planting surface."""
    exterior = bytearray(width * height)
    queue = deque()
    for pos in range(width * height):
        x, y = pos % width, pos // width
        if (x in (0, width-1) or y in (0, height-1)) and not mask[pos]:
            exterior[pos] = 1
            queue.append(pos)
    while queue:
        pos = queue.popleft()
        x, y = pos % width, pos // width
        for nxt in ((pos-1 if x else -1), (pos+1 if x+1<width else -1),
                    (pos-width if y else -1), (pos+width if y+1<height else -1)):
            if nxt >= 0 and not mask[nxt] and not exterior[nxt]:
                exterior[nxt] = 1
                queue.append(nxt)
    return bytearray(0 if value else 255 for value in exterior)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input',type=Path,default=ART/'floorPicWS-sparse-generated.png')
    parser.add_argument('--output',type=Path,default=ART/'floorPicWS.png')
    parser.add_argument('--header',type=Path,default=ROOT/'main/pic_data/home/floorPicWS.h')
    args=parser.parse_args()
    original = bytes(int(n, 16) for n in re.findall(r"0x[0-9a-fA-F]+",
        (ROOT / "main/pic_data/home/floorPic.h").read_text()))
    assert original[:2] == b"\x02\x01"
    palette = []
    for i in range(3):
        color = int.from_bytes(original[9+i*2:11+i*2], "big")
        palette.append((((color>>11)&31)*255//31,
                        ((color>>5)&63)*255//63, (color&31)*255//31))
    generated = Image.open(args.input).convert("RGBA")
    mask = main_component(bytes(a >= 128 for a in generated.getchannel("A").tobytes()),
                          *generated.size)
    alpha = Image.frombytes("L", generated.size, bytes(mask))
    generated.putalpha(alpha)
    sprite = generated.crop(alpha.getbbox())
    ratio = min((WIDTH-4)/sprite.width, (HEIGHT-4)/sprite.height)
    sprite = sprite.resize((round(sprite.width*ratio), round(sprite.height*ratio)),
                           Image.Resampling.NEAREST)
    image = Image.new("RGBA", (WIDTH, HEIGHT))
    image.paste(sprite, ((WIDTH-sprite.width)//2, (HEIGHT-sprite.height)//2))
    inside = closed_surface(bytearray(a >= 128 for a in image.getchannel("A").tobytes()),
                            WIDTH, HEIGHT)
    indices = []
    for pos, rgba in enumerate(image.getdata()):
        if not inside[pos]:
            index = 3
        elif rgba[3] < 128:
            index = 2  # Opaque grass, never a transparent hole in the centre.
        else:
            index = min(range(3), key=lambda i: sum((rgba[k]-palette[i][k])**2 for k in range(3)))
        indices.append(index)
    assert indices[(HEIGHT//2)*WIDTH+WIDTH//2] == 2
    image.putdata([(*palette[i],255) if i<3 else (0,0,0,0) for i in indices])
    image.save(args.output)
    packed = bytearray((WIDTH*HEIGHT+3)//4)
    for pos,index in enumerate(indices):
        packed[pos>>2] |= index << ((pos&3)*2)
    data = bytes((2,1,WIDTH>>8,WIDTH&255,HEIGHT>>8,HEIGHT&255,127,127,20)) + original[9:15] + packed
    lines = ["#ifndef __FLOORPICWS_H__", "#define __FLOORPICWS_H__",
             "/* ImageGen adaptation of floorPic; 224x98, original 3-colour RGB565 palette. */",
             "const unsigned char floorPicWS[] = {"]
    for start in range(0,len(data),24):
        lines.append("    "+",".join(f"0x{x:02x}" for x in data[start:start+24])+("," if start+24<len(data) else ""))
    lines.extend(["};", "#endif", ""])
    args.header.write_text("\n".join(lines))
    print(f"floorPicWS: {WIDTH}x{HEIGHT}, {len(data)} bytes; centre is opaque grass.")


if __name__ == "__main__":
    main()
