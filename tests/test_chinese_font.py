"""Check the GB2312 12-pixel glyph data used by compact screen layouts."""

from pathlib import Path
import re
import unittest


FONT = Path(__file__).resolve().parents[1] / "components/GVIEW/GB2312.h"


class ChineseFontTest(unittest.TestCase):
    def test_glyph_table_is_present_and_addressable(self):
        source = FONT.read_bytes()
        rows = re.findall(rb"^\{\{([^}]*)\}\}", source, re.MULTILINE)
        self.assertEqual(len(rows), 6763)

        glyphs = [tuple(int(n, 16) for n in re.findall(rb"0x([0-9a-fA-F]+)", row)) for row in rows]
        self.assertTrue(all(len(glyph) == 24 for glyph in glyphs))

        for character in "月日周城市温度设置天气":
            lead, trail = character.encode("gb2312")
            index = (lead - 0xB0) * 94 + trail - 0xA1
            self.assertTrue(any(glyphs[index]), character)


if __name__ == "__main__":
    unittest.main()
