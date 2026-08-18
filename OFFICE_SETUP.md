# Office Laptop Setup — UDS DID Tester (C)

Step-by-step guide to run this tool on a locked-down Windows office PC with **Peak hardware**.  
No Python required.

---

## Important: `uds_tester.exe` is not in Git

After cloning you will **not** see `build\uds_tester.exe`. That is normal.

The executable is **built on your laptop** the first time you run:

```bat
run.bat --mock
```

See the FAQ at the end of this file for details.


| Item | Why | Notes |
|------|-----|--------|
| This repo | Source + config | Clone from GitHub (private) |
| Peak USB adapter | Talk to ECU | Same hardware you use today |
| PEAK drivers | OS sees the device | Usually already installed if PCAN-View/Busmaster works |
| **PCAN-Basic** API | C library for Peak | Download from PEAK-System if not installed |
| C compiler | Build the `.exe` | MinGW-w64 **or** Visual Studio Build Tools |
| Excel (optional) | Open reports | CSV/XML open in Excel fine |

**Not required:** Python, CAPL, CANoe.

**Optional:** Busmaster — only to *watch* CAN traffic while this tool runs.

---

## 1. Clone the repo

```bat
git clone https://github.com/TejasA08/uds-did-tester-c.git
cd uds-did-tester-c
```

If Git is blocked, copy the project folder from a USB stick / approved share instead.

---

## 2. Confirm Peak is working

1. Plug in the Peak adapter and power the ECU/bench.
2. Open **PCAN-View** or **Busmaster**.
3. Confirm you can see bus traffic (or manually send a known UDS request).

If the adapter does not show up here, fix drivers/bitrate/wiring **before** using this tool.

---

## 3. Install a C compiler (if missing)

### Option A — MinGW-w64 (simple for `run.bat`)

1. Install MinGW-w64 (or MSYS2 + `mingw-w64-gcc`).
2. Ensure `gcc` is on `PATH`:

```bat
gcc --version
```

### Option B — Visual Studio / Build Tools

Install “Desktop development with C++” or Build Tools, then use the **x64 Native Tools** command prompt to compile (see §6).

---

## 4. Install PCAN-Basic (for real hardware)

1. Download **PCAN-Basic** from PEAK-System.
2. Install it. Typical path:

```text
C:\Program Files\PEAK-System\PCAN-Basic API
```

You need:

- `PCANBasic.h` (Include)
- `PCANBasic.lib` / `PCANBasic.dll` (x64)

Keep `PCANBasic.dll` either on system PATH or next to `uds_tester.exe` when running.

---

## 5. First run without ECU (sanity check)

From the project folder:

```bat
run.bat --mock
```

This will:

1. Build `build\uds_tester.exe` (if needed)
2. Run the sample tests against a **fake ECU**
3. Write reports under `reports\`

You should see **PASS** for the sample cases.  
If this fails, fix the compiler/`PATH` first — do not move to Peak yet.

Manual equivalent:

```bat
make test
make run-mock
```

---

## 6. Build with real Peak support

Default `run.bat` builds **without** PCAN linked (mock works; `interface=pcan` will fail until rebuilt with PCAN).

### MinGW / make example

```bat
make clean
make PCAN_SDK="C:\Program Files\PEAK-System\PCAN-Basic API"
```

Adjust `PCAN_SDK` if your install path differs.  
You may need to tweak `Makefile` library folder (`x64/VC_LIB` vs MinGW libs) to match what PEAK shipped.

### Visual Studio `cl` example

In **x64 Native Tools Command Prompt**:

```bat
mkdir build
cl /nologo /O2 /Iinclude /I"C:\Program Files\PEAK-System\PCAN-Basic API\Include" /DUDS_HAS_PCAN ^
  src\main.c src\hexutil.c src\config_csv.c src\bus.c src\isotp.c src\pcan_bus.c src\runner.c src\report.c ^
  /Fe:build\uds_tester.exe /link /LIBPATH:"C:\Program Files\PEAK-System\PCAN-Basic API\x64\VC_LIB" PCANBasic.lib
```

Copy `PCANBasic.dll` (x64) into `build\` if Windows cannot find it.

---

## 7. Configure your ECU

Edit **`config\test_cases.xlsx`** in Excel (two sheets):

### Sheet `Setup`

| Field | What to set |
|--------|-------------|
| `peak_channel` | e.g. `PCAN_USBBUS1` |
| `interface` | `pcan` for hardware, `mock` for offline |
| `can_type` | `classic` or `fd` |
| `bitrate` | e.g. `500000` |
| `fd_data_bitrate` | only if FD |
| `request_id` | Tester → ECU (e.g. `0x7E0`) |
| `response_id` | ECU → Tester (e.g. `0x7E8`) |
| `timeout_ms` | e.g. `1000` |
| `tester_present` | `yes` / `no` |
| `ecu_name` | Name shown on the report |

### Sheet `TestCases`

Columns:

`TestID | Action | Session | DID | DID_Name | WriteData | Expected | Compare | TimeoutMs | Notes`

Examples (one row per test):

| TestID | Action | Session | DID | WriteData | Expected | Compare |
|--------|--------|---------|-----|-----------|----------|---------|
| TC001 | Session | Extended | | | 50 03 | startswith |
| TC010 | Read | Extended | 0xF190 | | 62 F1 90 | startswith |
| TC020 | Write | Extended | 0xF1A0 | 01 02 03 | 6E F1 A0 | exact |
| TC021 | Read | Extended | 0xF1A0 | | 62 F1 A0 01 02 03 | exact |

**Actions:** `Session` | `Read` | `Write`  
**Sessions:** `Default` | `Extended` | `Programming`  
**Compare:** `exact` | `startswith` | `ignore_data`

Tip for day one: start with **Read-only** rows. Add writes after reads look correct.

If the xlsx is missing:

```bat
build\uds_tester.exe --create-config
```

(Legacy CSV via `--setup` / `--cases` still works, but Excel is the normal path.)

---

## 8. Run against the real ECU

1. Peak connected, ECU powered, bitrate/IDs correct.
2. `interface=pcan` in `config\test_cases.xlsx` (Setup sheet).
3. From project root:

```bat
run.bat
```

Or:

```bat
build\uds_tester.exe
```

Console shows PASS/FAIL per case.  
Reports are written under `reports\`.

---

## 9. Reports (Excel)

Each run creates:

| File | How to open |
|------|-------------|
| `reports\UDS_Report_<ECU>_<timestamp>.csv` | Double-click → Excel |
| `reports\UDS_Report_<ECU>_<timestamp>.xml` | Open with Excel (SpreadsheetML) |

Columns include: Request, Response, Expected, Actual, Pass/Fail, NRC, duration, errors.

---

## 10. Using Busmaster together (optional)

1. Start Busmaster, connect the same Peak channel **only if** your setup allows sharing — many benches use **one app owns the Peak device at a time**.
2. Practical approach:
   - Run **this tool** for automation (it opens PCAN-Basic).
   - Or run **Busmaster** for manual tracing — not both on the same channel unless you know your Peak/driver setup supports it.

If open fails with “busy” / init error, close Busmaster/PCAN-View and retry.

---

## Daily workflow (after first setup)

1. Edit `config\test_cases.xlsx` (Setup + TestCases sheets).
2. Connect Peak + power ECU.
3. Run `run.bat`.
4. Open the newest file in `reports\`.

---

## Troubleshooting

| Symptom | What to check |
|---------|----------------|
| `gcc not found` | Install MinGW and add to PATH, or use VS `cl` |
| No `uds_tester.exe` after clone | Expected — run `run.bat` once to build it (see FAQ below) |
| `CAN_Initialize failed` | Channel name, drivers, cable, another app holding Peak |
| Timeouts on every request | `request_id` / `response_id`, bitrate, ECU power, ISO-TP addressing |
| NRC in report | Expected — ECU rejected the service; check session/DID/support |
| `interface=pcan` but “PCAN support was not compiled in” | Rebuild with `UDS_HAS_PCAN` + link `PCANBasic` (§6) |
| Mock works, Peak fails | Config/hardware issue, not the Excel test logic |
| Excel won’t open XML | Use the `.csv` report instead |
| `PCANBasic.dll` not found | Copy x64 DLL next to `build\uds_tester.exe` |

---

## Quick command cheat sheet

```bat
:: Offline proof (also builds the .exe the first time)
run.bat --mock

:: Create/overwrite default Excel config from template
build\uds_tester.exe --create-config

:: Normal hardware run (after pcan build + Excel Setup sheet)
run.bat

:: Custom Excel config path
build\uds_tester.exe --config config\test_cases.xlsx --reports reports
```

---

## Security / IT notes

- No Python install required.
- Tool is built locally to `build\uds_tester.exe`; Excel config is `config\test_cases.xlsx`.
- Keep the GitHub repo **private**; do not commit customer DID dumps or vehicle secrets into git.
- Prefer Read-only tests first on real ECUs.

---

## Repo

https://github.com/TejasA08/uds-did-tester-c

---

## FAQ — office setup notes (18 Aug 2026)

### Why is `uds_tester.exe` missing after `git clone`?

That is **intentional**.

- The repo stores **source code + Excel template + docs**, not a Windows binary.
- `build/` and `*.exe` are in `.gitignore` so each PC builds its own executable.
- A Mac/Linux build cannot produce a valid Windows `.exe` for your office laptop.

**What to do:** from the cloned folder, run:

```bat
run.bat --mock
```

`run.bat` will compile and create:

```text
uds-did-tester-c\build\uds_tester.exe
```

After that, the file will be there locally (still not uploaded to GitHub).

### What files must be in the repo? (already included)

| Path | Purpose |
|------|---------|
| `src/`, `include/` | C source |
| `third_party/miniz/` | Excel `.xlsx` reader support |
| `config/test_cases.xlsx` | Your editable Setup + TestCases workbook |
| `templates/test_cases.xlsx` | Template for `--create-config` |
| `run.bat` / `Makefile` | Build + run helpers |
| `OFFICE_SETUP.md` | This guide |
| `build/.gitkeep` | Keeps empty `build\` folder placeholder |

You do **not** need `uds_tester.exe` in Git to set up the project.

### Step 4 — where do PCAN-Basic files go?

Install **PCAN-Basic** with the PEAK installer. Do **not** dump the whole SDK into the git repo.

Typical install location:

```text
C:\Program Files\PEAK-System\PCAN-Basic API\
  Include\PCANBasic.h          ← used when compiling
  x64\VC_LIB\PCANBasic.lib      ← used when linking
  (somewhere)\PCANBasic.dll    ← needed when running
```

| File | Where it stays |
|------|----------------|
| `PCANBasic.h` | PEAK install `Include\` (referenced by `PCAN_SDK` at build time) |
| `PCANBasic.lib` | PEAK install `x64\VC_LIB\` (linked at build time) |
| `PCANBasic.dll` (x64) | On PATH from install, **or copy next to** `build\uds_tester.exe` |

Build example:

```bat
make PCAN_SDK="C:\Program Files\PEAK-System\PCAN-Basic API"
```

If Windows says the DLL is missing when you run the tool, copy **only** `PCANBasic.dll` into:

```text
uds-did-tester-c\build\PCANBasic.dll
```

### Clone → first successful mock run (short path)

1. `cd` into `uds-did-tester-c`
2. Install MinGW (`gcc --version` works) **or** use VS Build Tools
3. Run `run.bat --mock`
4. Confirm PASS lines and new files under `reports\`
5. Open `config\test_cases.xlsx` and edit **Setup** + **TestCases**
6. For real Peak: install PCAN-Basic, rebuild with `UDS_HAS_PCAN`, set `interface=pcan`, run `run.bat`

### Busmaster / PCAN-View tip

Often **only one app** can open the Peak channel. If `CAN_Initialize` fails, close Busmaster/PCAN-View and retry.