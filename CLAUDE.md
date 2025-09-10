# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the Monitor Application, a Qt6-based real-time data visualization tool designed to display incoming packet data through various UI widgets (Grid, GridLogger, LineChart, PieChart, BarChart, 3D Chart). The application processes structured packet data from Ethernet or offline sources and displays field values in real-time.

## Build Commands

### macOS Build (Qt 6.9.1)
```bash
# Configure the build (from project root)
cmake -B build/Qt_6_9_1_for_macOS-Debug -DCMAKE_BUILD_TYPE=Debug

# Build the project
cd build/Qt_6_9_1_for_macOS-Debug && make

# Run the application
./Gorkemv5.app/Contents/MacOS/Gorkemv5
```

### Clean Build
```bash
# Clean build artifacts
cd build/Qt_6_9_1_for_macOS-Debug && make clean

# Complete rebuild
rm -rf build/Qt_6_9_1_for_macOS-Debug
cmake -B build/Qt_6_9_1_for_macOS-Debug -DCMAKE_BUILD_TYPE=Debug
cd build/Qt_6_9_1_for_macOS-Debug && make
```

## Architecture Overview

### Core Components (To Be Implemented)

1. **Packet Processing System**
   - Header-based packet identification using configurable ID field
   - Support for nested structures and complex data types (bitfields, unions)
   - Offset-based field extraction for performance
   - Real-time parsing at >200Hz per packet type

2. **UI Widget System**
   - Drag-and-drop field assignment from Struct Window
   - Widget subscription to packet types
   - Configurable data transformations and triggers
   - Independent widget windows within tabs

3. **Structure Management**
   - Reusable structs as building blocks
   - Packet structs with unique IDs
   - C++ structure parsing with alignment/padding support
   - JSON-based storage format

4. **Data Sources**
   - Ethernet mode with UDP/TCP support
   - Offline file playback with controls
   - Simulation mode for development testing

5. **Test Framework**
   - Real-time validation of packet data
   - Visual test builder and script editor
   - Performance metrics and failure tracking

## Key Implementation Requirements

### Performance Critical
- **NO POLLING**: Pure event-driven architecture required
- Multi-threaded design with dedicated threads for network, parsing, and UI
- Lock-free data structures for inter-thread communication
- Pre-allocated memory pools for packet buffers
- Target: 10,000 packets/second throughput

### Thread Architecture
- Network Thread: Packet reception only
- Parser Thread Pool: Parallel packet parsing
- Widget Processing Threads: One per widget type
- UI Thread: Qt main thread for GUI only

### Memory Management
- Offset-based field access (never use field names for processing)
- Zero-copy mechanisms where possible
- Ring buffers for packet queuing
- Custom allocators for hot paths

## Current Status

The project currently has:
- Basic Qt6 application skeleton with MainWindow
- CMake build configuration for Qt6
- Project structure ready for implementation

Next implementation priorities:
1. Core packet structure definitions
2. Basic packet parsing engine
3. Struct Window with tree view
4. Grid widget with drag-drop support
5. Ethernet packet reception

## Important Notes

- **Qt Version**: Exclusively Qt6 (not Qt5)
- **Platforms**: macOS and Windows support required
- **Offline Operation**: No internet connectivity required
- **Real-time**: Hard real-time requirements for safety-critical operations
- **Specification**: Full details in `monitor-app-specification.md`
- always read @monitor-app-specification.md for understanding the app.
- always read @docs/ROADMAP.md for the roadmap.
- use @docs/ROADMAP.md to inspect and update the roadmap, also, create additional files for after the phase end, explaining what had done in the particular phase. also, reference the finished phase document in this file.
- the summary of all the phases that completed are in the @docs/ROADMAP_DONE.md
- when completing a phase, ALWAYS create extensive test and dont count the phase as finished until the app is compiled without errors and there is no failed tasks.

