# Firmware and Flash layout

## Last deployed images — 2026-11-16

These measurements describe the last flashed 1.5 images, including Web Panel
cancellation. Subsequent source-only CSS or documentation edits do not change
the bytes already on the board. Repacking or rebuilding produces new artifacts
and requires measuring them again.

The resident-updater implementation keeps the default 4 MiB partition table.
app0 is assigned to Fanmaster and app1 to ResourceUploader. The following are
final build measurements with ESP32 Arduino core 3.3.12 and
`esp32:esp32:esp32s3:CDCOnBoot=cdc`. Both application images were flashed with
hash verification, and all seven resources served from SPI Flash matched the
source files after the verified upload. See [hardware validation](validation.md).
Artifacts and manifests are in ignored `build/v1.5/`.

| Artifact | Program usage | Static RAM | Image bytes | Slot/footprint |
| --- | ---: | ---: | ---: | --- |
| app0 Fanmaster 1.5 | 1,036,691 | 50,684 | 1,036,832 | 79.10% of 1,310,720 bytes |
| app1 ResourceUploader | 367,098 | 24,336 | 367,248 | 28.02% of 1,310,720 bytes |
| W25Q64 assets | — | — | 38,267 | Ten 4 KiB sectors = 40,960 bytes |

Only one application executes at a time; their static RAM figures are not added.
The main ELF program sum is `.flash.text` 772,432 + `.flash.rodata` 140,864 +
`.eh_frame` 20,216 + `.flash.appdesc` 304 + IRAM vectors/text 80,087 + initialized
DRAM 22,788 = 1,036,691 bytes. Static RAM adds BSS 27,896 to initialized DRAM.
The application descriptor section includes the new 40-byte role record plus
alignment; the role record itself begins at image offset 288 in both binaries.

SHA256 values:

- app0: `49d035b54338c0eb65b6e3ab5d3fbaec8a04f7e86371917f66190508db0f1409`
- app1: `937835db917846705f8af806e63dea7cbc507fa50eb4e25a6cebb2083b6c398b`
- SPI image: `39d44d344a17c71c7949132a73906e4b1309125be538fc68cf41e43451a982a0`

SPI format remains 32-byte header + seven 160-byte entries (1,120 bytes) +
37,115 payload bytes. The image ends at `0x00957b`, with 2,693 unused bytes in
the last erased sector; the sector footprint ends at `0x00a000`. The whole-image
CRC32 is `4d9e5c4e`. No filesystem or credential storage is added to W25Q64.

| Resource | Offset | Bytes | CRC32 |
| --- | --- | ---: | --- |
| `/assets/panel.css` | `0x000480` | 11,705 | `20becb2b` |
| `/assets/panel.js` | `0x003239` | 8,059 | `d5fd833f` |
| `/assets/setup.css` | `0x0051b4` | 692 | `3cef11e2` |
| `/assets/setup.js` | `0x005468` | 2,908 | `4ea1eb5e` |
| `/assets/thermal.js` | `0x005fc4` | 5,217 | `d093ad2a` |
| `/index.html` | `0x007425` | 7,426 | `4dc7411f` |
| `/setup.html` | `0x009127` | 1,108 | `ce030f52` |

### Current ESP32 internal Flash map (4 MiB)

End addresses are exclusive. Reserved capacity and actual image bytes are
separate: this is not a dump of every live byte on the chip.

| Region | Start | End | Capacity | Current use/evidence |
| --- | --- | --- | ---: | --- |
| Bootloader area | `0x000000` | `0x008000` | 32,768 | 19,968-byte bootloader image |
| Partition-table sector | `0x008000` | `0x009000` | 4,096 | 3,072-byte generated table |
| NVS | `0x009000` | `0x00e000` | 20,480 | Wi-Fi and operating settings retained; live occupancy not queried |
| OTA metadata | `0x00e000` | `0x010000` | 8,192 | SDK app0/app1 boot selection |
| app0 | `0x010000` | `0x150000` | 1,310,720 | Fanmaster: 1,036,832 image bytes; 273,888 spare |
| app1 | `0x150000` | `0x290000` | 1,310,720 | ResourceUploader: 367,248 image bytes; 943,472 spare |
| SPIFFS | `0x290000` | `0x3f0000` | 1,441,792 | Not mounted for web resources; contents not inspected |
| Coredump | `0x3f0000` | `0x400000` | 65,536 | Reserved; contents not inspected |
| **Total** | `0x000000` | `0x400000` | **4,194,304** | Entire address space accounted for |

Both application images total **1,404,080 bytes (33.48% of the chip)**. They do
not consume both applications' RAM simultaneously. app0 uses **79.10%** of its
slot and app1 **28.02%**. These figures exclude bootloader/table/metadata and
unknown live NVS, SPIFFS or coredump contents. The FMO2 operating-settings blob
is 28 bytes before NVS metadata; credentials are separate, and their sizes were
not read. Unused space in a different partition does not enlarge an app slot.

### Current W25Q64 schema (8 MiB)

```text
0x000000  Header                         32 bytes
0x000020  Index: 7 × 160-byte entries  1,120 bytes
0x000480  HTML / CSS / JS payload     37,115 bytes
0x00957b  Last erased-sector tail      2,693 bytes
0x00a000  Outside current footprint 8,347,648 bytes
0x800000  End of W25Q64
```

Each index entry contains path[96], MIME[48], offset, length, CRC32 and reserved
fields. The **38,267-byte** image uses **0.4562%** of W25Q64; its **40,960-byte**
erase footprint uses **0.4883%**. The logical space outside the image is
8,350,341 bytes. Neither free-space number proves that older unreferenced bytes
are erased. This is a single resource image with a header committed last,
without a filesystem or a second asset bank. Wi-Fi credentials and operating
settings remain in ESP32 NVS.

The default partition addresses/capacities remain unchanged.
The resident uploader now occupies the previously unassigned app1 slot. Runtime
selection writes otadata only; the installer writes app1 only by default, or
bootloader/table/otadata/app0/app1 for an initial `--install-both` deployment.
NVS, SPIFFS and coredump are not written by that deployment. See
[resident updater](updater.md) and [installation and assets](resources.md).

## Historical measurements

The earlier relay-power build and its resource image are preserved in the
[2026-10-18 storage snapshot](storage-2026-10-18.md). These are historical measurements, not the latest images.
