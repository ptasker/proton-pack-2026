#!/usr/bin/env python3
"""Generates the wiring diagrams in this folder.

Run from anywhere:  python3 docs/images/generate_wiring.py

Pins come from include/Config.h, include/WandConfig.h and
include/simple/SimpleConfig.h. If you change a pin there, change it here and
run this again.
"""
from pathlib import Path
from xml.sax.saxutils import escape

OUT = Path(__file__).resolve().parent

ROW = 26        # height of one pin row
TITLE = 44      # height of a box's title area
FONT = "font-family='Helvetica, Arial, sans-serif'"

WIRE = {
    "data": "#1f6feb",   # NeoPixel data
    "i2s": "#8250df",    # amplifier
    "spi": "#0a7d6c",    # SD card
    "uart": "#1a7f37",   # wand-pack cable
    "input": "#7d5a2c",  # switches and button
    "i2c": "#bc4c00",    # bargraph driver
    "gnd": "#30363d",
}
TAG = {"5V": "#cf222e", "3V3": "#bf8700", "GND": "#30363d"}


class Box:
    """A component: a title, an optional subtitle, and pin rows.

    side is the edge its signal pins sit on ('l' or 'r'). rows are
    (label, kind) where kind is a WIRE key for a signal pin, a TAG key for a
    power pin, or None for a plain note row.
    """

    def __init__(self, key, x, y, w, title, sub, side, rows):
        self.key, self.x, self.y, self.w = key, x, y, w
        self.title, self.sub, self.side, self.rows = title, sub, side, rows
        self.h = TITLE + ROW * len(rows) + 8

    def pin_y(self, index):
        return self.y + TITLE + ROW * index + ROW / 2

    def edge(self, side):
        return self.x if side == "l" else self.x + self.w

    def svg(self, fill="#ffffff"):
        o = [f"<rect x='{self.x}' y='{self.y}' width='{self.w}' height='{self.h}' rx='8' "
             f"fill='{fill}' stroke='#57606a' stroke-width='1.5'/>"]
        cx = self.x + self.w / 2
        o.append(f"<text x='{cx}' y='{self.y + 19}' text-anchor='middle' {FONT} font-size='14' "
                 f"font-weight='700' fill='#1f2328'>{escape(self.title)}</text>")
        if self.sub:
            o.append(f"<text x='{cx}' y='{self.y + 35}' text-anchor='middle' {FONT} font-size='11.5' "
                     f"fill='#57606a'>{escape(self.sub)}</text>")
        for i, (label, kind) in enumerate(self.rows):
            y = self.pin_y(i)
            if kind in TAG:
                # Power pin: label on the signal side, coloured tag on the far side.
                lx = self.x + 12 if self.side == "l" else self.x + self.w - 12
                anchor = "start" if self.side == "l" else "end"
                tx = self.x + self.w - 50 if self.side == "l" else self.x + 8
                o.append(f"<text x='{lx}' y='{y + 4}' text-anchor='{anchor}' {FONT} font-size='12.5' "
                         f"fill='#1f2328'>{escape(label)}</text>")
                o.append(f"<rect x='{tx}' y='{y - 9}' width='42' height='18' rx='9' fill='{TAG[kind]}'/>")
                o.append(f"<text x='{tx + 21}' y='{y + 4}' text-anchor='middle' {FONT} font-size='11' "
                         f"font-weight='700' fill='#ffffff'>{kind}</text>")
            elif kind is None:
                o.append(f"<text x='{cx}' y='{y + 4}' text-anchor='middle' {FONT} font-size='11.5' "
                         f"fill='#57606a'>{escape(label)}</text>")
            else:
                lx = self.x + 12 if self.side == "l" else self.x + self.w - 12
                anchor = "start" if self.side == "l" else "end"
                o.append(f"<text x='{lx}' y='{y + 4}' text-anchor='{anchor}' {FONT} font-size='12.5' "
                         f"fill='#1f2328'>{escape(label)}</text>")
                o.append(f"<circle cx='{self.edge(self.side)}' cy='{y}' r='3.5' fill='{WIRE[kind]}'/>")
        return "\n".join(o)


def wire(x1, x2, y, kind, label=None, dashed=False):
    dash = " stroke-dasharray='6 5'" if dashed else ""
    o = [f"<line x1='{x1}' y1='{y}' x2='{x2}' y2='{y}' stroke='{WIRE[kind]}' stroke-width='2.2'{dash}/>"]
    if label:
        mx = (x1 + x2) / 2
        width = 7 * len(label) + 10
        o.append(f"<rect x='{mx - width / 2}' y='{y - 9}' width='{width}' height='18' rx='4' "
                 f"fill='#ffffff' stroke='{WIRE[kind]}' stroke-width='1'/>")
        o.append(f"<text x='{mx}' y='{y + 4}' text-anchor='middle' {FONT} font-size='11' "
                 f"fill='{WIRE[kind]}'>{escape(label)}</text>")
    return "\n".join(o)


def mcu(x, w, title, sub, left, right, power):
    """Controller box whose pins line up with the boxes they connect to.

    left/right are lists of (pin label, kind, y). power is a list of
    (label, tag) drawn along the bottom edge.
    """
    ys = [p[2] for p in left + right]
    top, bottom = min(ys) - TITLE - ROW / 2, max(ys) + ROW / 2 + 44
    o = [f"<rect x='{x}' y='{top}' width='{w}' height='{bottom - top}' rx='10' fill='#eef4ff' "
         f"stroke='#1f2328' stroke-width='2'/>"]
    cx = x + w / 2
    o.append(f"<text x='{cx}' y='{top + 20}' text-anchor='middle' {FONT} font-size='15' "
             f"font-weight='700' fill='#1f2328'>{escape(title)}</text>")
    o.append(f"<text x='{cx}' y='{top + 36}' text-anchor='middle' {FONT} font-size='11.5' "
             f"fill='#57606a'>{escape(sub)}</text>")
    for pins, edge, anchor, dx in ((left, x, "start", 12), (right, x + w, "end", -12)):
        for label, kind, y in pins:
            o.append(f"<circle cx='{edge}' cy='{y}' r='3.5' fill='{WIRE[kind]}'/>")
            o.append(f"<text x='{edge + dx}' y='{y + 4}' text-anchor='{anchor}' {FONT} font-size='12.5' "
                     f"font-weight='600' fill='#1f2328'>{escape(label)}</text>")
    px = cx - (len(power) * 92 - 12) / 2
    for label, tag in power:
        o.append(f"<text x='{px}' y='{bottom - 14}' {FONT} font-size='12' fill='#1f2328'>{escape(label)}</text>")
        o.append(f"<rect x='{px + 34}' y='{bottom - 27}' width='42' height='18' rx='9' fill='{TAG[tag]}'/>")
        o.append(f"<text x='{px + 55}' y='{bottom - 14}' text-anchor='middle' {FONT} font-size='11' "
                 f"font-weight='700' fill='#ffffff'>{tag}</text>")
        px += 92
    return "\n".join(o), bottom


def legend(x, y, kinds):
    names = {"data": "NeoPixel data", "i2s": "I2S audio", "spi": "SPI (SD card)",
             "uart": "UART (wand cable)", "input": "Switch input", "i2c": "I2C"}
    o = []
    for kind in kinds:
        o.append(f"<line x1='{x}' y1='{y}' x2='{x + 28}' y2='{y}' stroke='{WIRE[kind]}' stroke-width='2.5'/>")
        o.append(f"<text x='{x + 34}' y='{y + 4}' {FONT} font-size='12' fill='#1f2328'>{names[kind]}</text>")
        x += 48 + 7 * len(names[kind])
    return "\n".join(o)


def notes(x, y, lines):
    return "\n".join(f"<text x='{x}' y='{y + 18 * i}' {FONT} font-size='12' fill='#57606a'>{escape(t)}</text>"
                     for i, t in enumerate(lines))


def page(name, width, height, title, body):
    svg = (f"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 {width} {height}' "
           f"width='{width}' height='{height}' role='img' aria-label='{escape(title)}'>\n"
           f"<rect width='{width}' height='{height}' fill='#ffffff'/>\n"
           f"<text x='30' y='38' {FONT} font-size='20' font-weight='700' fill='#1f2328'>{escape(title)}</text>\n"
           + "\n".join(body) + "\n</svg>\n")
    (OUT / name).write_text(svg)


def connect(boxes, links, mx, mw):
    """Wires from each box's signal pins straight across to the controller.

    links maps (box key, row index) to (controller pin label, wire label).
    Returns the wire SVG and the controller's left and right pin lists.
    """
    out, left, right = [], [], []
    for box in boxes:
        for i, (_, kind) in enumerate(box.rows):
            if (box.key, i) not in links:
                continue
            pin, label, *rest = links[(box.key, i)]
            y = box.pin_y(i)
            if box.side == "r":      # box is left of the controller
                out.append(wire(box.x + box.w, mx, y, kind, label, bool(rest)))
                left.append((pin, kind, y))
            else:                    # box is right of the controller
                out.append(wire(mx + mw, box.x, y, kind, label, bool(rest)))
                right.append((pin, kind, y))
    return out, left, right


RES = "330-470 Ω"
POWER_NOTES = [
    "Coloured tags are power connections: 5V = regulated 5V supply, 3V3 = the board's 3.3V pin, GND = common ground.",
    "Every ground is joined. Pixels and the amp should take 5V straight from the supply, not through the controller board.",
]


def pack():
    LX, LW, MX, MW, RX, RW = 30, 250, 400, 190, 710, 260
    boxes = [
        Box("ring", LX, 70, LW, "Cyclotron ring", "40 x WS2812", "r",
            [("DIN", "data"), ("5V", "5V"), ("GND", "GND")]),
        Box("cell", LX, 210, LW, "Power cell", "15 x WS2812", "r",
            [("DIN", "data"), ("5V", "5V"), ("GND", "GND")]),
        Box("sd", LX, 350, LW, "microSD reader", "SPI, 3.3V module", "r",
            [("SCK", "spi"), ("MISO", "spi"), ("MOSI", "spi"), ("CS", "spi"), ("VCC", "3V3"), ("GND", "GND")]),
        Box("amp", RX, 70, RW, "MAX98357A amplifier", "I2S, mono", "l",
            [("BCLK", "i2s"), ("LRC", "i2s"), ("DIN", "i2s"), ("VIN", "5V"), ("GND", "GND"),
             ("Speaker + / - go to the speaker only", None)]),
        Box("wand", RX, 330, RW, "Cable to wand", "3 wires, 3.3V logic, 9600 baud", "l",
            [("Wand GPIO6 (RX)", "uart"), ("Wand GPIO7 (TX)", "uart"), ("Wand GND", "gnd")]),
    ]
    links = {
        ("ring", 0): ("GPIO22", RES), ("cell", 0): ("GPIO23", RES),
        ("sd", 0): ("GPIO18", None), ("sd", 1): ("GPIO19", None), ("sd", 2): ("GPIO13", None), ("sd", 3): ("GPIO4", None),
        ("amp", 0): ("GPIO26", None), ("amp", 1): ("GPIO27", None), ("amp", 2): ("GPIO14", None),
        ("wand", 0): ("GPIO25 (TX)", None), ("wand", 1): ("GPIO35 (RX)", None), ("wand", 2): ("GND", None),
    }
    wires, left, right = connect(boxes, links, MX, MW)
    body, bottom = mcu(MX, MW, "ESP32-WROOM", "main pack controller", left, right, [("5V", "5V"), ("GND", "GND")])
    y = max(bottom, max(b.y + b.h for b in boxes)) + 40
    page("wiring-main-pack.svg", 1000, y + 130, "Main pack wiring",
         wires + [body] + [b.svg() for b in boxes]
         + [legend(30, y, ["data", "spi", "i2s", "uart"]),
            notes(30, y + 30, POWER_NOTES + [
                "The data-line resistors are recommended, as is a 500-1000 uF capacitor across the ring's power input.",
                "Ground the amp's SD (shutdown) pin to mute it during uploads. GPIO35 is input-only.",
            ])])


def wand():
    LX, LW, MX, MW, RX, RW = 30, 270, 420, 200, 740, 250
    boxes = [
        Box("pack", LX, 70, LW, "Cable to pack", "3 wires, 3.3V logic, 9600 baud", "r",
            [("Pack GPIO25 (TX)", "uart"), ("Pack GPIO35 (RX)", "uart"), ("Pack GND", "gnd")]),
        Box("act", LX, 210, LW, "Activate toggle", "left switch", "r",
            [("Switch", "input"), ("Other leg", "GND")]),
        Box("vent", LX, 324, LW, "Vent toggle", "top right; disabled in firmware", "r",
            [("Switch", "input"), ("Other leg", "GND")]),
        Box("wsw", LX, 438, LW, "Wand toggle", "lower right; also acts as Vent", "r",
            [("Switch", "input"), ("Other leg", "GND")]),
        Box("fire", LX, 552, LW, "Fire button", "momentary, normally open", "r",
            [("Button", "input"), ("Other leg", "GND")]),
        Box("px", RX, 70, RW, "NeoPixel chain", "34 x WS2812, order shown below", "l",
            [("DIN", "data"), ("5V", "5V"), ("GND", "GND")]),
        Box("ls", RX, 250, RW, "BSS138 level shifter", "3.3V side (LV)  |  5V side (HV)", "l",
            [("LV1  >  HV1: SDA", "i2c"), ("LV2  >  HV2: SCL", "i2c"), ("LV", "3V3"), ("HV", "5V"), ("GND", "GND")]),
    ]
    links = {
        ("pack", 0): ("GPIO6 (RX)", None), ("pack", 1): ("GPIO7 (TX)", None), ("pack", 2): ("GND", None),
        ("act", 0): ("GPIO0", None), ("vent", 0): ("GPIO1", "not read", True),
        ("wsw", 0): ("GPIO20", None), ("fire", 0): ("GPIO2", None),
        ("px", 0): ("GPIO21", RES), ("ls", 0): ("GPIO4 (SDA)", None), ("ls", 1): ("GPIO5 (SCL)", None),
    }
    wires, left, right = connect(boxes, links, MX, MW)
    body, bottom = mcu(MX, MW, "ESP32-C3 Super Mini", "wand controller", left, right, [("5V", "5V"), ("GND", "GND")])

    # Bargraph driver and display hang off the 5V side of the level shifter.
    ls = boxes[-1]
    drv = Box("drv", RX, ls.y + ls.h + 46, RW, "HT16K33 LED driver", "I2C address 0x70", "l",
              [("SDA, SCL from HV1, HV2", None), ("VDD", "5V"), ("GND", "GND")])
    bar = Box("bar", RX, drv.y + drv.h + 46, RW, "28-segment bargraph", "Barmeter BL28Z-3005SA04Y", "l",
              [("A0-A6 > pins 22, 1, 19, 18, 7, 10, 11", None), ("C0-C3 > pins 21, 15, 13, 16", None)])
    cx = RX + RW / 2
    extra = []
    for a, b, text in ((ls, drv, "SDA, SCL at 5V"), (drv, bar, "11 wires")):
        extra.append(f"<line x1='{cx}' y1='{a.y + a.h}' x2='{cx}' y2='{b.y}' stroke='{WIRE['i2c']}' stroke-width='2.2'/>")
        extra.append(f"<text x='{cx + 10}' y='{(a.y + a.h + b.y) / 2 + 4}' {FONT} font-size='11.5' "
                     f"fill='{WIRE['i2c']}'>{text}</text>")

    # Pixel chain order.
    y = max(bottom, bar.y + bar.h, boxes[4].y + boxes[4].h) + 44
    chain = [("Slo-blo", "pixel 0"), ("Vent jewel", "1-7"), ("Hat lights", "8-9"),
             ("Clippard", "10"), ("Firing strip", "11-33, folded")]
    extra.append(f"<text x='30' y='{y}' {FONT} font-size='13' font-weight='700' fill='#1f2328'>Pixel chain order</text>")
    x = 30
    for i, (name, rng) in enumerate(chain):
        extra.append(f"<rect x='{x}' y='{y + 12}' width='150' height='44' rx='6' fill='#ffffff' stroke='{WIRE['data']}' stroke-width='1.5'/>")
        extra.append(f"<text x='{x + 75}' y='{y + 31}' text-anchor='middle' {FONT} font-size='12.5' font-weight='600' fill='#1f2328'>{name}</text>")
        extra.append(f"<text x='{x + 75}' y='{y + 47}' text-anchor='middle' {FONT} font-size='11' fill='#57606a'>{rng}</text>")
        if i < len(chain) - 1:
            extra.append(f"<line x1='{x + 150}' y1='{y + 34}' x2='{x + 186}' y2='{y + 34}' stroke='{WIRE['data']}' stroke-width='2.2'/>")
            extra.append(f"<path d='M{x + 186},{y + 34} l-8,-5 v10 z' fill='{WIRE['data']}'/>")
        x += 186

    y += 96
    page("wiring-wand.svg", 1020, y + 130, "Wand wiring",
         wires + [body] + [b.svg() for b in boxes] + [drv.svg(), bar.svg()] + extra
         + [legend(30, y, ["uart", "input", "data", "i2c"]),
            notes(30, y + 30, POWER_NOTES + [
                "Switches connect a GPIO to ground and use the internal pull-ups. The Vent toggle is wired to GPIO1 but",
                "the firmware ignores it (VENT_SWITCH_PIN = -1). GPIO2 is a strapping pin: keep fire released at reset.",
            ])])


def simple():
    LX, LW, MX, MW, RX, RW = 30, 250, 400, 190, 710, 260
    boxes = [
        Box("cyc", LX, 70, LW, "Cyclotron", "4 x 7-pixel WS2812 jewels, chained", "r",
            [("DIN", "data"), ("5V", "5V"), ("GND", "GND")]),
        Box("cell", LX, 210, LW, "Power cell", "12 x WS2812", "r",
            [("DIN", "data"), ("5V", "5V"), ("GND", "GND")]),
        Box("wand", LX, 350, LW, "Wand pixel chain", "10 x WS2812", "r",
            [("DIN", "data"), ("5V", "5V"), ("GND", "GND")]),
        Box("sd", LX, 490, LW, "microSD reader", "SPI, 3.3V module", "r",
            [("SCK", "spi"), ("MISO", "spi"), ("MOSI", "spi"), ("CS", "spi"), ("VCC", "3V3"), ("GND", "GND")]),
        Box("amp", RX, 70, RW, "MAX98357A amplifier", "I2S, mono", "l",
            [("BCLK", "i2s"), ("LRC", "i2s"), ("DIN", "i2s"), ("VIN", "5V"), ("GND", "GND"),
             ("Speaker + / - go to the speaker only", None)]),
        Box("act", RX, 330, RW, "Activate toggle", "", "l",
            [("Switch", "input"), ("Other leg", "GND")]),
        Box("fire", RX, 444, RW, "Fire button", "momentary, normally open", "l",
            [("Button", "input"), ("Other leg", "GND")]),
    ]
    links = {
        ("cyc", 0): ("GPIO22", RES), ("cell", 0): ("GPIO23", RES), ("wand", 0): ("GPIO21", RES),
        ("sd", 0): ("GPIO18", None), ("sd", 1): ("GPIO19", None), ("sd", 2): ("GPIO13", None), ("sd", 3): ("GPIO4", None),
        ("amp", 0): ("GPIO26", None), ("amp", 1): ("GPIO27", None), ("amp", 2): ("GPIO14", None),
        ("act", 0): ("GPIO33", None), ("fire", 0): ("GPIO32", None),
    }
    wires, left, right = connect(boxes, links, MX, MW)
    body, bottom = mcu(MX, MW, "ESP32-WROOM", "simple pack controller", left, right,
                       [("5V", "5V"), ("GND", "GND")])
    y = max(bottom, max(b.y + b.h for b in boxes)) + 40
    page("wiring-simple-pack.svg", 1000, y + 130, "Simple pack wiring",
         wires + [body] + [b.svg() for b in boxes]
         + [legend(30, y, ["data", "spi", "i2s", "input"]),
            notes(30, y + 30, POWER_NOTES + [
                "Wand chain order: 4-pixel bargraph, two body lights, vent, 3-pixel firing strip. Data-line resistors are recommended.",
                "Give the ESP32 its own 5V supply or regulator: sharing the amp's feed caused resets while firing.",
            ])])


if __name__ == "__main__":
    pack()
    wand()
    simple()
    print("wrote", ", ".join(sorted(p.name for p in OUT.glob("wiring-*.svg"))))
