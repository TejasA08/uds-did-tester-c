# UDS DID Tester (C)

Peak/PCAN UDS automation with Excel test config and Excel-compatible reports.

## Quick start (no hardware)

```bash
make test          # offline self-check
make run-mock      # run sample suite against mock ECU
```

Windows (MinGW):

```bat
run.bat --mock
```

Edit tests in Excel: **`config/test_cases.xlsx`** (sheets **Setup** and **TestCases**).

## Real Peak hardware (Windows)

1. Install PEAK drivers + **PCAN-Basic** API.
2. Edit `config/test_cases.xlsx`:
   - Sheet **Setup**: `interface` = `pcan`, plus IDs / bitrate / channel
   - Sheet **TestCases**: your DID rows
3. Build with PCAN:

```bat
make pcan PCAN_SDK="C:\Program Files\PEAK-System\PCAN-Basic API"
```

Or with MinGW after adjusting include/lib paths, define `UDS_HAS_PCAN` and link `PCANBasic`.

4. Run:

```bat
run.bat
```

Reports:
- `reports/UDS_Report_*.csv` — open in Excel
- `reports/UDS_Report_*.xml` — Excel SpreadsheetML (open in Excel)

## Busmaster

Optional: run Busmaster alongside this tool to watch CAN traffic. This executable talks to Peak via **PCAN-Basic** directly.

## Layout

- `src/` — C sources
- `include/uds_tester.h` — shared types
- `config/test_cases.xlsx` — Setup + TestCases (edit in Excel)
- `templates/test_cases.xlsx` — template used by `--create-config`
- `reports/` — generated outputs
