#!/usr/bin/python

'''
  Tool to build RigDfu firmware images from hex files
  
  @copyright (c) Rigado, LLC. All rights reserved.

  Source code licensed under BMD-200 Software License Agreement.
  You should have received a copy with purchase of BMD-200 product.
  If not, contact info@rigado.com for for a copy. 
'''

import sys
import struct
import ihex

class RigError(Exception):
    pass

def int2byte(i):
    if sys.version_info < (3,):
        return chr(i)
    return bytes((i,))

def byte2int(v):
    if isinstance(v, int):
        return v
    return ord(v)

class MultiHexFile(object):
    """Some tools to help manage multiple hex files and pull padded regions of
    data out of them."""
    def __init__(self, hexfiles, pad):
        self.data = ihex.IHex();
        for hexfile in hexfiles:
            ih = ihex.IHex.read_file(hexfile)
            # Combine new file with self.data
            for start, data in sorted(ih.areas.items()):
                end = start + len(data)
                if start == end:
                    continue
                # Check for overlap
                for ostart, odata in sorted(self.data.areas.items()):
                    oend = ostart + len(odata)
                    if start < oend and end >= ostart:
                        # Don't complain if the overlapping data is the same
                        if start == ostart and end == oend and data == odata:
                            continue
                        raise RigError(("data region [%x-%x] in %s "
                                        "overlaps earlier data") %
                                       (start, end, hexfile))
                self.data.insert_data(start, data)

    def extents(self, minaddr, maxaddr, round = 4):
        """Return a tuple containing the extents of data that are
        present between 'minaddr' and 'maxaddr'.  Addresses are
        rounded to 'round' bytes."""
        ext_min = None
        ext_max = None
        for start, data in sorted(self.data.areas.items()):
            end = start + len(data)
            if minaddr < end and ext_min is None:
                ext_min = max(minaddr, start)
            if maxaddr > start and ext_min is not None:
                ext_max = min(maxaddr, end)
        if ext_min is None or ext_max is None:
            return (None, None)
        def round_down(x, n):
            return x - (x % n)
        ext_min = round_down(ext_min, round)
        ext_max = round_down(ext_max + (round - 1), round)
        return (ext_min, ext_max)

    def extract(self, minaddr, maxaddr, pad = 0xff):
        """Return all data in the specified range, padding missing values"""
        buf = b''
        addr = minaddr
        for start, data in sorted(self.data.areas.items()):
            end = start + len(data)
            if addr > end:
                continue
            if maxaddr <= start:
                continue
            if addr < start:
                buf += int2byte(pad) * (start - addr)
                addr = start
            if end > maxaddr:
                end = maxaddr
            buf += data[addr-start:end-start]
            addr = end
        buf += int2byte(pad) * (maxaddr - addr)
        return buf

    def uint32le(self, addr):
        return struct.unpack("<I", self.extract(addr, addr+4))[0]

class RigDfuGen(object):

    def __init__(self, inputs, sd, bl, app, sd_addr, bl_addr, app_addr,
                 verbose = True):
        """Parse hex files for image data.

        inputs: a list of hex files to load.

        sd, bl, app: If any are True, include
           them in the image.  If all are False, determine which ones
           to include in the image based on what's found in the input
           files.

        sd_addr, bl_addr, app_addr: tuples containing fixed (min, max)
           addresses to use for each of the 3 segments, rather than
           searching.  Data is (max-min) bytes starting at min.
        """
        self.inputs = inputs
        self.sd = sd
        self.bl = bl
        self.app = app
        self.sd_addr = sd_addr
        self.bl_addr = bl_addr
        self.app_addr = app_addr
        self.verbose = verbose

        self.data = MultiHexFile(inputs, pad = 0xff)

        self.find_sd()
        self.find_bl()
        self.find_app()

        search = False
        if not (self.sd or self.bl or self.app):
            # Guess which ones are present based on addresses we have
            search = True

        def check(which, flag, addr):
            if getattr(self, flag) or (search and getattr(self, addr)):
                setattr(self, flag, True)
                a = getattr(self, addr)
                if not a:
                    raise RigError("Want %s, but can't find it" % which)
                if self.verbose:
                    sys.stderr.write("%12s: 0x%05x - 0x%05x (%d bytes)\n"
                                     % (which, a[0], a[1], a[1] - a[0]))

        # Make sure we have addresses for everything we want to write
        check("Softdevice", 'sd', 'sd_addr')
        check("Bootloader", 'bl', 'bl_addr')
        check("Application", 'app', 'app_addr')

        if not (self.sd or self.bl or self.app):
            raise RigError("No softdevice, bootloader, or application found")

        if self.app and (self.sd or self.bl):
            raise RigError("Unsupported image combination; application must be "
                           "updated alone")

    def gen_image(self):
        """Generate output image, returning it as a byte stream"""
        sd_data = self.data.extract(*self.sd_addr) if self.sd else b''
        bl_data = self.data.extract(*self.bl_addr) if self.bl else b''
        app_data = self.data.extract(*self.app_addr) if self.app else b''
        header = struct.pack('<3I', len(sd_data), len(bl_data), len(app_data))
        iv = int2byte(0) * 16
        tag = int2byte(0) * 16
        data = sd_data + bl_data + app_data
        return header + iv + tag + data

    def find_sd(self):
        """Look for a Softdevice"""
        if self.sd_addr:
            return True
        # Must have data starting at 0x1000 to at least 0x300c
        (minaddr, maxaddr) = self.data.extents(0x1000, 0x300c)
        if minaddr != 0x1000 or maxaddr != 0x300c:
            return False
        # Size is stored at 0x3008
        end = self.data.uint32le(0x3008)
        # Valid code has to exist
        if not self.valid_code(0x1000, end):
            return False
        # Address of SD is the extent of valid code in its size
        self.sd_addr = (0x1000, self.data.extents(0x1000, end)[1])
        return True

    def find_bl(self):
        """Look for a bootloader"""
        found = False
        max_extent = 0
        if self.bl_addr:
            return True
        # Find valid code at the beginning of a page somewhere between
        # 0x30000 and 0x3f800
        size = 0x1000
        (minaddr, maxaddr) = self.data.extents(0x70000, 0x7e000, size)
        max_extent = 0x7e000
        if minaddr is None:
            size = 0x400
            (minaddr, maxaddr) = self.data.extents(0x37000, 0x3f800, size)
            max_extent = 0x3f800
            if minaddr is None:
                return False

        for addr in range(minaddr, maxaddr, size):
            if self.valid_code(addr, maxaddr):
                found = True
                break

        if not found:
            return False

        # Address of BL is the extent of valid code we found
        self.bl_addr = self.data.extents(addr, max_extent)
        return True

    def find_app(self):
        """Look for an application"""
        if self.app_addr:
            return True
        # Find valid code at the beginning of a page somewhere between
        # 0x10000 and 0x3f000.
        (minaddr, maxaddr) = (0x10000, 0x49000)
        # If we have a bootloader, we know the app must end before it
        if self.bl_addr:
            maxaddr = self.bl_addr[0]
        # If we have a softdevice, it tells us where the app starts.
        if self.sd_addr:
            sd_end = self.data.uint32le(self.sd_addr[0] + 0x2008)
            if sd_end > 0x10000 and sd_end < 0x30000:
                minaddr = sd_end
        # Find valid code in an aligned page in the range
        (minaddr, maxaddr) = self.data.extents(minaddr, maxaddr, 0x1000)
        if minaddr is None:
            return False
        for addr in range(minaddr, maxaddr, 0x1000):
            if self.valid_code(addr, maxaddr):
                break
        else:
            return False
        # Address of app is the extent of valid code we found
        self.app_addr = self.data.extents(addr, maxaddr)
        return True

    def valid_code(self, minaddr, maxaddr):
        """Return True if the initial SP and reset values provided
        correspond to a valid application residing between 'minaddr'
        and 'maxaddr'"""
        initial_sp = self.data.uint32le(minaddr + 0);
        reset = self.data.uint32le(minaddr + 4);
        # unaligned?
        if (initial_sp % 4) != 0:
            return False
        # invalid SP?  assume 64k max RAM
        if initial_sp < 0x20000000 or initial_sp >= (0x20000000 + 64 * 1024):
            return False
        # non-thumb reset vector?
        if (reset % 2) != 1:
            return False
        # reset vector pointing outside address range?
        if reset < minaddr or reset >= maxaddr:
            return False
        return True

if __name__ == "__main__":
    import argparse

    description = "Generate images for RigDFU bootloader"
    parser = argparse.ArgumentParser(description = description)

    parser.add_argument("hexfile", metavar = "HEXFILE", nargs = "+",
                        help = "Hex file(s) to load")

    parser.add_argument("--output", "-o", metavar = "BIN",
                        help = "Output file")
    parser.add_argument("--quiet", "-q", action = "store_true",
                        help = "Print less output")

    group = parser.add_argument_group(
        "Images to include",
        "If none are specified, images are determined automatically based "
        "on the hex file contents.")
    group.add_argument("--softdevice", "-s", action = "store_true",
                       help = "Include softdevice")
    group.add_argument("--bootloader", "-b", action = "store_true",
                       help = "Include bootloader")
    group.add_argument("--application", "-a", action = "store_true",
                       help = "Include application")

    group = parser.add_argument_group(
        "Image locations in the HEX files",
        "If unspecified, locations are guessed heuristically.  Format "
        "is LOW-HIGH, for example 0x1000-0x16000 and 0x16000-0x3b000.")
    group.add_argument("--softdevice-addr", "-S", metavar="LOW-HIGH",
                       help = "Softdevice location")
    group.add_argument("--bootloader-addr", "-B", metavar="LOW-HIGH",
                       help = "Bootloader location")
    group.add_argument("--application-addr", "-A", metavar="LOW-HIGH",
                       help = "Application location")

    args = parser.parse_args()

    if not args.output:
        parser.error("must specify --output file")

    def parse_addr(s):
        if not s:
            return None
        (l, h) = s.split('-')
        return (int(l, 0), int(h, 0))

    try:
        rigdfugen = RigDfuGen(inputs = args.hexfile,
                              sd = args.softdevice,
                              bl = args.bootloader,
                              app = args.application,
                              sd_addr = parse_addr(args.softdevice_addr),
                              bl_addr = parse_addr(args.bootloader_addr),
                              app_addr = parse_addr(args.application_addr),
                              verbose = not args.quiet)
        img = rigdfugen.gen_image()
        with open(args.output, "wb") as f:
            f.write(rigdfugen.gen_image())
        if not args.quiet:
            sys.stderr.write("Wrote %d bytes to %s\n" % (len(img), args.output))
    except RigError as e:
        sys.stderr.write("Error: %s\n" % str(e))
        raise SystemExit(1)
