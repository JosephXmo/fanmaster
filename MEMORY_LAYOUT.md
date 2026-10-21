# Firmware and Flash layout — 2026-10-18

This snapshot describes the flashed relay-power build, including the earlier THERMAL and TACH
outlier guard changes. The relay module was not connected during target tests;
software power state does not establish physical 12 V switching. Capacities use bytes and binary KiB/MiB. Build artifacts are in
ignored `build/power-prompt/build/` (including the wake-prompt wording correction);
the resource image remains in
`build/power-acceptance/fanmaster-assets.bin`. Sizes come from the final compiler,
ELF sections, binary partition table and resource index, rather than estimates
from source length. Hardware identification reported 4 MiB internal Flash and
2 MiB PSRAM on the connected ESP32-S3-Zero.

## Main application

| Measure | Bytes | Meaning |
| --- | ---: | --- |
| Arduino program usage | 1,038,711 | 79.25% of the 1,310,720-byte app slot |
| Remaining compiler program budget | 272,009 | Slot capacity minus program usage |
| Application `.bin` | 1,038,864 | Actual uncompressed image written at `0x10000` |
| Remaining app-slot bytes after image | 271,856 | Excludes any erase-sector rounding |
| Static RAM | 51,028 | 15.57% of the compiler's 327,680-byte RAM budget |
| Remaining compiler RAM budget | 276,652 | Not measured runtime free heap |
| Bootloader `.bin` | 19,968 | Written at `0x000000` |
| Partition table `.bin` | 3,072 | Written at `0x008000` |
| OTA initialization image | 8,192 | `boot_app0.bin`, written at `0x00e000` |
| Merged build artifact | 4,194,304 | Padded whole-address-space artifact; not used for this upload |

The application image is 153 bytes larger than the compiler program total due
to image framing/alignment/checksum metadata. Its SHA256 is
`c5606d9c826c920544d28c0858046e1d5c9352b22d2ce44276fb95f0b0dab62d`.
USB flashing writes bootloader, table, OTA initialization and app0 separately,
with hash verification. It does not write the entire merged image or erase NVS.
These four inputs total 1,070,096 bytes; this is upload payload, not total live
content across the chip. Transfer compression does not change stored image size.

The final ELF program total can be reconciled as follows:

| Program section | Bytes | Storage/use |
| --- | ---: | --- |
| `.flash.text` | 773,780 | Flash-mapped executable code |
| `.flash.rodata` | 141,172 | Flash-mapped read-only data |
| `.eh_frame` | 20,364 | Exception/unwind metadata |
| `.flash.appdesc` | 256 | Application descriptor |
| `.iram0.vectors` + `.iram0.text` | 80,271 | Code loaded from image into IRAM |
| `.dram0.data` | 22,868 | Initialized data stored in image and loaded into RAM |
| **Total program** | **1,038,711** | Arduino program statistic |

Static RAM is `.dram0.data` 22,868 plus `.dram0.bss` 28,160. ELF debug sections,
address-space dummy sections and `.flash.rodata_noload` must not be summed as
additional image contents. Runtime stacks, heap, network allocations and PSRAM
use are not measured by this compile-time statistic. RTC-specific sections
are separate from the reported DRAM total.

## ESP32 internal Flash: 4 MiB

The exact partition table is the default 4 MiB layout emitted by ESP32 Arduino
core 3.3.12 for `esp32:esp32:esp32s3:CDCOnBoot=cdc`. End addresses below are exclusive.

| Region | Start | End | Reserved bytes | Current evidence |
| --- | --- | --- | ---: | --- |
| Bootloader area | `0x000000` | `0x008000` | 32,768 | 19,968-byte bootloader; area also includes padding/gaps |
| Partition-table sector | `0x008000` | `0x009000` | 4,096 | 3,072-byte generated table image |
| `nvs` (data/nvs) | `0x009000` | `0x00e000` | 20,480 | Wi-Fi and operating settings; retained during upload |
| `otadata` (data/ota) | `0x00e000` | `0x010000` | 8,192 | OTA initialization image written this round |
| `app0` (ota_0) | `0x010000` | `0x150000` | 1,310,720 | Final application image: 1,038,864 bytes |
| `app1` (ota_1) | `0x150000` | `0x290000` | 1,310,720 | Reserved second slot; contents not inspected |
| `spiffs` (data/spiffs) | `0x290000` | `0x3f0000` | 1,441,792 | Present in table; application does not mount/use it for web assets |
| `coredump` (data/coredump) | `0x3f0000` | `0x400000` | 65,536 | Reserved; contents not inspected |
| **Total** | `0x000000` | `0x400000` | **4,194,304** | All address space accounted for |

The app0 image occupies 24.77% of the physical 4 MiB chip, but **79.26% of its
own slot**. Unused app0 space, app1 and SPIFFS cannot automatically enlarge the
current app slot; that requires an explicit partition-layout change. A reserved
partition is not proof that it is populated or erased. This report does not
read back private Wi-Fi records or infer historical app1/SPIFFS/coredump usage.
No OTA update endpoint is implemented merely by having two app slots.

NVS stores credentials separately from the `fanmaster-op` namespace. The latter
has one `settings` blob: the current FMO2 payload is **28 bytes**, containing
magic/version marker, target RPM, manual duty, three temperatures (uint16 tenths
of °C), three integer duties, reserved padding and CRC32. Legacy FMO1 was 16 bytes.
The active mode is not stored. NVS adds namespace/key/blob metadata, page headers,
alignment, obsolete entries and garbage-collection reserve; 28 bytes is therefore
not total NVS consumption. Live used/free NVS entry counts were not queried, and
credential payload sizes were not read. The 20 KiB partition capacity is exact;
its current physical/live occupancy is not claimed.

## W25Q64 SPI Flash: 8 MiB

A single indexed, read-only resource image starts at offset zero. There is no
filesystem or dynamic allocation here. The upload firmware erases/programs the
image area, checks CRCs, and commits the header last. The main application reads
indexed resources on demand. Credentials and operating settings remain in ESP32
NVS, not this image. Resource format version 1 is independent of NVS format FMO2.

```text
0x000000 ┌───────────────────────────────────────────────┐
         │ Header: 32 bytes (FMASSET1, version, count,   │
         │ length, index CRC, payload CRC, header CRC)   │
0x000020 ├───────────────────────────────────────────────┤
         │ 7 index entries × 160 bytes = 1,120 bytes     │
         │ path[96], MIME[48], offset/length/CRC/reserved │
0x000480 ├───────────────────────────────────────────────┤
         │ Seven HTML/CSS/JS payloads: 35,427 bytes      │
0x008ee3 ├───────────────────────────────────────────────┤
         │ Final erased-sector tail: 285 bytes          │
0x009000 ├───────────────────────────────────────────────┤
         │ Outside current image's 9 erase sectors      │
         │ 8,351,744 bytes                              │
0x800000 └───────────────────────────────────────────────┘
```

| Resource | Offset | Bytes | CRC32 |
| --- | --- | ---: | --- |
| `/assets/panel.css` | `0x000480` | 11,667 | `b9081e2f` |
| `/assets/panel.js` | `0x003213` | 7,007 | `28380f12` |
| `/assets/setup.css` | `0x004d72` | 692 | `3cef11e2` |
| `/assets/setup.js` | `0x005026` | 2,908 | `4ea1eb5e` |
| `/assets/thermal.js` | `0x005b82` | 5,217 | `d093ad2a` |
| `/index.html` | `0x006fe3` | 6,828 | `3470804f` |
| `/setup.html` | `0x008a8f` | 1,108 | `ce030f52` |

The image is **36,579 bytes (35.722 KiB), 0.4361%** of 8,388,608 bytes, with
whole-image CRC32 `45faa6c5`. Header/index overhead is 1,152 bytes. Programming
covers **nine 4 KiB erase sectors = 36,864 bytes (36 KiB), 0.4395%**. Logical space
outside the image is 8,352,029 bytes; excluding its rounded erase-sector footprint
leaves 8,351,744 bytes. Neither number proves the remainder is all erased: older
unreferenced bytes may remain outside the newly erased area. No other live data
structure is currently assigned there. `/` is a route alias to `/index.html`, not
a duplicate payload. The uploader returned `DONE verified`.
Target HTTP payload checks are recorded
in [HARDWARE_TEST_REPORT.md](HARDWARE_TEST_REPORT.md).
