# Office Laptop Setup — UDS DID Tester (C)

Step-by-step guide to run this tool on a locked-down Windows office PC with **Peak hardware**.  
No Python required.

---

## What you need (one-time)

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

Edit these two files (Notepad / Excel are fine):

### `config\setup.csv`

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

### `config\test_cases.csv`

Columns:

```text
testid,action,session,did,did_name,writedata,expected,compare,timeout_ms,notes
```

Examples:

```text
TC001,Session,Extended,,,,50 03,startswith,,Enter extended
TC010,Read,Extended,0xF190,VIN,,62 F1 90,startswith,1000,Read VIN
TC020,Write,Extended,0xF1A0,Coding,01 02 03,6E F1 A0,exact,1000,Write coding
TC021,Read,Extended,0xF1A0,Verify,,62 F1 A0 01 02 03,exact,1000,Verify write
```

**Actions:** `Session` | `Read` | `Write`  
**Sessions:** `Default` | `Extended` | `Programming` (or raw like `0x03`)  
**Compare:** `exact` | `startswith` | `ignore_data`

Tip for day one: start with **Read-only** rows. Add writes after reads look correct.

---

## 8. Run against the real ECU

1. Peak connected, ECU powered, bitrate/IDs correct.
2. `interface=pcan` in `setup.csv`.
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

1. Edit `config\test_cases.csv` (add/change DIDs).
2. Connect Peak + power ECU.
3. Run `run.bat`.
4. Open the newest file in `reports\`.

---

## Troubleshooting

| Symptom | What to check |
|---------|----------------|
| `gcc not found` | Install MinGW and add to PATH, or use VS `cl` |
| `CAN_Initialize failed` | Channel name, drivers, cable, another app holding Peak |
| Timeouts on every request | `request_id` / `response_id`, bitrate, ECU power, ISO-TP addressing |
| NRC in report | Expected — ECU rejected the service; check session/DID/support |
| `interface=pcan` but “PCAN support was not compiled in” | Rebuild with `UDS_HAS_PCAN` + link `PCANBasic` (§6) |
| Mock works, Peak fails | Config/hardware issue, not the test CSV logic |
| Excel won’t open XML | Use the `.csv` report instead |

---

## Quick command cheat sheet

```bat
:: Offline proof
run.bat --mock

:: Create/overwrite default CSVs
build\uds_tester.exe --create-config

:: Normal hardware run (after pcan build + setup.csv)
run.bat

:: Custom paths
build\uds_tester.exe --setup config\setup.csv --cases config\test_cases.csv --reports reports
```

---

## Security / IT notes

- No Python install required.
- Tool is a local `.exe` + CSV config; it does not need internet to run tests.
- Keep the GitHub repo **private**; do not commit customer DID dumps or vehicle secrets into git.
- Prefer Read-only tests first on real ECUs.

---

## Repo

https://github.com/TejasA08/uds-did-tester-c
