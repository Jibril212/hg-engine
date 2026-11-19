import hashlib
import os
import shutil
import struct
import sys

ARM9_PATH = "base/arm9.bin"
Y9_PATH = "base/overarm9.bin"
OV12_PATH = "base/overlay/overlay_0012.bin"
OV12_BASE = 0x022378C0
OV123_PATH = "base/overlay/overlay_0123.bin"

VANILLA_OV12 = {
    0x0223DEB0: "38b50c1c051c201c",  # BtlCmd_PrintAttackMessage
    0x0223DEF0: "30b591b00c1c051c",  # BtlCmd_PrintMessage
    0x0223DF24: "30b591b00c1c051c",  # BtlCmd_PrintGlobalMessage
    0x0223DF64: "38b50c1c051c201c",  # BtlCmd_PrintBufferedMessage
    0x0223DF84: "38b588b00c1c051c",  # BtlCmd_BufferMessage
    0x0223DFB0: "78b591b00c1c051c",  # BtlCmd_BufferLocalMessage
}

OTHER_STALE_HOOKS = [
    ("base/overlay/overlay_0001.bin", 0x021E5900, 0x02202C7C, "2f4a90420edc3eda",
     "SpeciesAndFormeToWazaOshieIndex, removed 08d2f932d"),
    ("base/arm9.bin", 0x02000000, 0x0207C29C, "48482a584848118d",
     "UseItemMonAttrChangeCheck_hook, removed 6945ea3eb"),
    ("base/overlay/overlay_0012.bin", 0x022378C0, 0x022408AC, "381cfbf773fa1149",
     "load_max_move_num_metronome, removed adbefdc45"),
]

STALE_OV12_PATCHES = [
    (0x02251D28, "60b4044d76462e6060bc7ff12bfe014909688f46401d2502", "f8b58cb014980d1c1490159802921590101c1c1c129f139e",
     "ServerDoTypeCalcMod old-style hook, removed 7e11a8cc6"),
    (0x02257D9C, "982c3d02", "04cc2602", "sMetronomeMimicMoveBanList repoint, removed 5c4d99094"),
    (0x02257DF4, "982c3d02", "04cc2602", "sMetronomeMimicMoveBanList repoint, removed 5c4d99094"),
    (0x02245148, "ba", "67", "byte left by the >256 abilities change, reverted 407dfb326"),
]

OLD_ANTIPIRACY_ARM9 = {
    0x300: ("1c009fe5001090e518209fe5020051e17210a0030810c0058e10a003c410c0051eff2fe1a0f02502345d09e0",
            "0447d161b5635dc3b0b117486d4c0b5bfd1166e970adf45c45a7e39c3d29a278e4dad91a9e5583725b0f6427"),
    0xA18: ("38feffea", "1eff2fe1"),
}

OV123_RAM_SIZE_OFFSET = 123 * 0x20 + 0x08
OV123_BAD_RAM_SIZE = 0x0F78
OV123_GOOD_RAM_SIZE = 0x1BE0

OV123_SIZE = 7136
OV123_MASKED = (0xAC, 0x164)
OV123_GOOD_MASKED_MD5 = "05c51fa6102a2c3f5b022f5b9f328232"
OV123_DECRYPTED_MASKED_MD5 = "51bad6acc950da0ee99cf6d4f7aa6c79"  # from Avalonia DSPRE extractions
OV123_GOOD_FILE = "ov123.bin"
OV123_GOOD_FILE_MD5 = "4d63007642ef00cb211942bf6ad46577"
DSROM_OVERLAYS_YAML = "base_dsrom/arm9_overlays/overlays.yaml"

EVENTS_PATH = "base/root/a/0/3/2"

# upstream 8c40fe195 moved MON_OVERWORLD_TAG_START from 1050 to 2500
OVERWORLD_TAG_REMAP = [
    (57, 4, 561, 1143, 2593),  # SPECIES_HAUNTER
    (57, 10, 1462, 1850, 3300),  # SPECIES_MUDSDALE
    (57, 11, 540, 1128, 2578),  # SPECIES_RAPIDASH
    (57, 12, 680, 1245, 2695),  # SPECIES_QUAGSIRE
    (57, 13, 531, 1123, 2573),  # SPECIES_TENTACRUEL
    (57, 14, 557, 2191, 3641),  # SPECIES_MUK_ALOLAN
    (57, 15, 480, 1081, 2531),  # SPECIES_NIDOQUEEN
    (57, 16, 990, 1504, 2954),  # SPECIES_TOXICROAK
    (60, 1, 586, 1163, 2613),  # SPECIES_CHANSEY
    (66, 2, 586, 1163, 2613),  # SPECIES_CHANSEY
    (94, 2, 586, 1163, 2613),  # SPECIES_CHANSEY
    (119, 6, 871, 1400, 2850),  # SPECIES_MILOTIC
    (153, 2, 586, 1163, 2613),  # SPECIES_CHANSEY
    (155, 0, 754, 1285, 2735),  # SPECIES_SMEARGLE
    (415, 1, 455, 1075, 2525),  # SPECIES_PIKACHU
    (415, 2, 621, 1193, 2643),  # SPECIES_SNORLAX
    (416, 0, 627, 2216, 3666),  # SPECIES_MOLTRES_GALARIAN
    (419, 1, 636, 2528, 4037),  # SPECIES_MEGANIUM_OVERWORLD_FEMALE
    (419, 2, 639, 1207, 2657),  # SPECIES_TYPHLOSION
    (419, 3, 643, 1210, 2660),  # SPECIES_FERALIGATR
    (466, 2, 586, 1163, 2613),  # SPECIES_CHANSEY
    (491, 4, 1084, 1535, 2985),  # SPECIES_HEATRAN
]


def is_hg_engine_hook(chunk):
    # make.py writes "ldr rX, [pc]; bx rX" followed by the target address
    return chunk[1] & 0xF8 == 0x48 and chunk[3] == 0x47


def fix_ov12():
    with open(OV12_PATH, "rb+") as f:
        data = bytearray(f.read())
        for addr, vanilla_hex in VANILLA_OV12.items():
            offset = addr - OV12_BASE
            current = data[offset:offset + 8]
            vanilla = bytes.fromhex(vanilla_hex)
            if current == vanilla:
                print(f"ov12 {addr:#010x}: already vanilla")
            elif is_hg_engine_hook(current):
                target = struct.unpack_from("<I", current, 4)[0]
                data[offset:offset + 8] = vanilla
                print(f"ov12 {addr:#010x}: removed stale hook to {target:#010x}")
            else:
                sys.exit(f"ov12 {addr:#010x}: unexpected bytes {current.hex()}, refusing to touch it")
        f.seek(0)
        f.write(data)


def fix_other_stale_hooks():
    for path, load_addr, addr, vanilla_hex, name in OTHER_STALE_HOOKS:
        with open(path, "rb+") as f:
            data = bytearray(f.read())
            offset = addr - load_addr
            current = data[offset:offset + 8]
            vanilla = bytes.fromhex(vanilla_hex)
            if current == vanilla:
                print(f"{path} {addr:#010x}: already vanilla")
                continue
            if not is_hg_engine_hook(current):
                print(f"{path} {addr:#010x}: unexpected bytes {current.hex()}, left as is")
                continue
            target = struct.unpack_from("<I", current, 4)[0]
            data[offset:offset + 8] = vanilla
            f.seek(0)
            f.write(data)
            print(f"{path} {addr:#010x}: removed stale hook to {target:#010x} ({name})")


def fix_stale_ov12_patches():
    with open(OV12_PATH, "rb+") as f:
        data = bytearray(f.read())
        for addr, stale_hex, vanilla_hex, name in STALE_OV12_PATCHES:
            stale, vanilla = bytes.fromhex(stale_hex), bytes.fromhex(vanilla_hex)
            offset = addr - OV12_BASE
            current = data[offset:offset + len(stale)]
            if current == vanilla:
                print(f"ov12 {addr:#010x}: already vanilla")
            elif current == stale:
                data[offset:offset + len(stale)] = vanilla
                print(f"ov12 {addr:#010x}: restored vanilla ({name})")
            else:
                print(f"ov12 {addr:#010x}: unexpected bytes {current.hex()}, left as is")
        f.seek(0)
        f.write(data)


def fix_old_antipiracy():
    with open(ARM9_PATH, "rb+") as f:
        data = bytearray(f.read())
        for offset, (old_hex, vanilla_hex) in OLD_ANTIPIRACY_ARM9.items():
            old, vanilla = bytes.fromhex(old_hex), bytes.fromhex(vanilla_hex)
            current = data[offset:offset + len(old)]
            if current == vanilla:
                print(f"arm9 {0x02000000 + offset:#010x}: already vanilla")
            elif current == old:
                data[offset:offset + len(old)] = vanilla
                print(f"arm9 {0x02000000 + offset:#010x}: removed old antipiracy bytes")
            else:
                print(f"arm9 {0x02000000 + offset:#010x}: unexpected bytes {current.hex()}, left as is")
        f.seek(0)
        f.write(data)


def fix_ov123_ram_size():
    with open(Y9_PATH, "rb+") as f:
        f.seek(OV123_RAM_SIZE_OFFSET)
        ram_size = struct.unpack("<I", f.read(4))[0]
        if ram_size == OV123_BAD_RAM_SIZE:
            f.seek(OV123_RAM_SIZE_OFFSET)
            f.write(struct.pack("<I", OV123_GOOD_RAM_SIZE))
            print(f"overarm9 ov123 ram_size: {ram_size:#x} -> {OV123_GOOD_RAM_SIZE:#x}")
        else:
            print(f"overarm9 ov123 ram_size: {ram_size:#x}, left as is")


def masked_md5(data):
    data = bytearray(data)
    for offset in OV123_MASKED:
        data[offset:offset + 4] = bytes(4)
    return hashlib.md5(data).hexdigest()


def dsrom_reencrypts_ov123():
    try:
        with open(DSROM_OVERLAYS_YAML) as f:
            return "dsprot:" in f.read()
    except OSError:
        return False


def fix_ov123_file():
    with open(OV123_PATH, "rb") as f:
        data = f.read()

    if len(data) == OV123_SIZE and masked_md5(data) == OV123_GOOD_MASKED_MD5:
        print(f"{OV123_PATH}: good (retail ov123)")
        return

    if len(data) == OV123_SIZE and masked_md5(data) == OV123_DECRYPTED_MASKED_MD5:
        if dsrom_reencrypts_ov123():
            print(f"{OV123_PATH}: decrypted, as expected: dsrom re-encrypts it on build")
            return
        problem = "DS Protect already decrypted (copied from an Avalonia DSPRE project)"
    elif len(data) == 3960:
        problem = "truncated to 3960 bytes (a DSPRE 2.3.4 save cut it)"
    else:
        problem = f"unknown content ({len(data)} bytes)"
    print(f"{OV123_PATH}: BAD, {problem}")

    if not os.path.exists(OV123_GOOD_FILE):
        print(f"  put the good {OV123_GOOD_FILE} (md5 {OV123_GOOD_FILE_MD5}) next to this script and run it again")
        return
    with open(OV123_GOOD_FILE, "rb") as f:
        good = f.read()
    if hashlib.md5(good).hexdigest() != OV123_GOOD_FILE_MD5:
        print(f"  {OV123_GOOD_FILE} is not the good one (md5 should be {OV123_GOOD_FILE_MD5}), not replacing")
        return
    shutil.copyfile(OV123_PATH, OV123_PATH + ".bak")
    shutil.copyfile(OV123_GOOD_FILE, OV123_PATH)
    print(f"  replaced with {OV123_GOOD_FILE} (old one saved as {OV123_PATH}.bak)")


def narc_member_offsets(narc):
    count = struct.unpack_from("<I", narc, 0x18)[0]
    fnt = 0x1C + 8 * count
    fimg = fnt + struct.unpack_from("<I", narc, fnt + 4)[0]
    return [fimg + 8 + struct.unpack_from("<I", narc, 0x1C + 8 * i)[0] for i in range(count)]


def fix_overworld_tags():
    with open(EVENTS_PATH, "rb+") as f:
        narc = bytearray(f.read())
        starts = narc_member_offsets(narc)
        for event, index, old_tag, stale_tag, new_tag in OVERWORLD_TAG_REMAP:
            event_start = starts[event]
            bg_count = struct.unpack_from("<I", narc, event_start)[0]
            tag_offset = event_start + 4 + bg_count * 0x14 + 4 + index * 0x20 + 2
            tag = struct.unpack_from("<H", narc, tag_offset)[0]
            if tag == new_tag:
                print(f"event {event} npc {index}: already tag {new_tag}")
            elif tag in (old_tag, stale_tag):
                struct.pack_into("<H", narc, tag_offset, new_tag)
                print(f"event {event} npc {index}: tag {tag} -> {new_tag}")
            else:
                print(f"event {event} npc {index}: tag is {tag}, expected {old_tag}, left as is")
        f.seek(0)
        f.write(narc)


if __name__ == "__main__":
    fix_ov12()
    fix_other_stale_hooks()
    fix_stale_ov12_patches()
    fix_old_antipiracy()
    fix_ov123_ram_size()
    fix_ov123_file()
    fix_overworld_tags()