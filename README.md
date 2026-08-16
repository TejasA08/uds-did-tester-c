# UDS DID Tester (C)

Peak/PCAN UDS automation with CSV test config and Excel-compatible reports.

## Quick start (no hardware)

```bash
make test          # offline self-check
make run-mock      # run sample suite against mock ECU
```

Windows (MinGW):

```bat
run.bat --mock
```

## Real Peak hardware (Windows)

1. Install PEAK drivers + **PCAN-Basic** API.
2. Edit `config/setup.csv`:
   - `interface` = `pcan`
   - `request_id` / `response_id` / `bitrate` / `peak_channel`
3. Edit `config/test_cases.csv` with your DID rows.
4. Build with PCAN:

```bat
make pcan PCAN_SDK="C:\Program Files\PEAK-System\PCAN-Basic API"
```

Or with MinGW after adjusting include/lib paths, define `UDS_HAS_PCAN` and link `PCANBasic`.

5. Run:

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
- `config/` — setup + test cases CSV
- `reports/` — generated outputs
