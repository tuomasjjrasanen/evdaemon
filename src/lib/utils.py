import sys

MAX_HEXLEN = len(hex(sys.maxint)[2:])

def hexstrs_to_int(hexstrs, max_hexlen=MAX_HEXLEN):
    hexsumstr = ''
    hexvals = [int(v, 16) for v in hexstrs]
    for hexval in hexvals:
        padded_hexval = format(hexval, "0%dx" % max_hexlen)
        hexsumstr += padded_hexval
    return int(hexsumstr, 16)

def hexline_to_int(hexline, max_hexlen=MAX_HEXLEN):
    return hexstrs_to_int(hexline.split(), max_hexlen)

def int_to_hexline(value):
    hexstr = format(value, "x")
    blocks = []
    block = []
    for char in reversed(hexstr):
        block.append(char)
        if len(block) == 16:
            if all([c == block[0] for c in block[1:]]):
                blocks.append("0")
            else:
                # format(int()) to remove leading zeros, hacky but works.
                blocks.append(format(int("".join(reversed(block)), 16), "x"))
            block = []
    if block:
        blocks.append("".join(reversed(block)))
    return " ".join(reversed(blocks))
