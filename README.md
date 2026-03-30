# resource_monitor_lnx
A complete Linux CLI tool to monitor system resources using /proc filesystem

## Features

- Real-time CPU usage with color coding
- Memory and Swap statistics
- Load averages (1/5/15 min)
- Running processes count
- Clean terminal UI with progress bar
- Very low overhead
- No external dependencies

## Requirements

- Linux kernel (any modern version)
- gcc compiler

## Installation

### From source:

```bash
git clone https://github.com/Salim-onc/resource_monitor_lnx.git
cd resource_monitor_lnx
make
sudo make install
```

## How to build and run right now
```bash
1. Save the code as sysmon.c
2. Save Makefile
3. Run:
make
./sysmon
```
