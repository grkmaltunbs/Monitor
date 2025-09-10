# The Monitor Application - Complete Specification

## Overview

The Monitor app serves the purpose of displaying the values of incoming packets in the form of Grid, GridLogger, LineChart, PieChart, BarChart, and 3D Chart. These will be called "UI Widgets". The UI widgets will show the relevant bitfield, byte, word, or any other primitive type or types. The app will be built using Qt6 exclusively (not Qt5) and will be available for macOS and Windows.

The main purpose of the app is to save the types of incoming packets and parse them in order to display them in the UI Widgets. All incoming packets will have the same header structure that will be determined in the app. The app will parse the header to determine the incoming packet type. The type of the packet will be determined by the ID field in the header. After determining the packet type, the UI widgets will display the values of the fields that are specified for them in their types. For example, an integer field will be shown as an integer and a double field will be shown as a double, unless specified otherwise. The app will also have the capability to play offline data (same rules apply: parse the header and determine the packet).

## THE USER INTERFACE

### Main UI Components

The UI of the app will consist of several tabs and windows inside these tabs. Users will be able to create and delete tabs as well as rename the tabs and change their order. Above the tabs there will be several buttons for different purposes:

- **"Add Struct Button"**: This will open the "Add Struct Window" that will serve the purpose of saving "reusable structs" and "packet structs"
- **"Test Framework Button"**: Opens the "Real-Time Test Manager" window for creating and managing automated tests. Shows status indicator (green = all passing, red = failures, yellow = paused)
- **"Simulation Buttons"**: There will be two buttons, Start Simulation and Stop Simulation, respectively. These will work together as a radio button group. These buttons will only be available in the development stage of the app.
- **"Ethernet and Offline buttons"**: These will work together as a radio button group. The Ethernet button will be lit up when the Ethernet port is opened, and the Offline button will be lit up when offline mode is selected. Also, the Port Settings button will appear when the Ethernet port is selected (the Ethernet Mode is selected)
- **"Offline Playback Controls"**: A group of buttons for offline mode control:
  - Play/Pause button
  - Stop button (return to beginning)
  - Step Forward (advance one packet)
  - Step Backward (go back one packet)
  - Jump to Time (opens dialog for specific timestamp)
  - Playback Speed slider (0.1x to 10x speed)
  - Progress bar showing current position in file
- **"UI widget buttons"**: There will be one button for every UI widget.
- **"Performance Dashboard Button"**: Opens the real-time performance monitoring dashboard

### The Tabs
The tabs of the app can be renamed, created, renamed or deleted. Also, the order of the tabs will be changeable. Every tab will have one or more windows that are the "UI Widgets".

### The Windows
The windows will reside in the tabs. The "Struct Window" will be present in every tab. Other than that, any window will only be inside one tab. The windows can be resized, dragged, minimized, closed or extended to full. Only the "Struct Window" will not be able to be closed (as it serves as the primary source for drag-and-drop operations). A new window will be opened when any of the "UI widget buttons" is clicked with the relevant UI widget.

### The Struct Window
The Struct Window will show all the saved structures in a tree pattern. The structs that are shown in the Struct Window will be expandable and shrinkable. The window will show the type and name of the struct or field side by side. For example:

```c
typedef struct {
    int x;
    int y;
    int z;
}Field3D;

typedef struct {
    Field3D velocity;
    Field3D acceleration;
    char name[4];
    float time;
}DUMMY;
```

For the DUMMY struct, the widget will show the fields velocity (Field3D), acceleration (Field3D), name (char[4]) and time (float) when DUMMY struct is expanded. After that, the x (int), y (int) and z (int) fields will be shown when either of the velocity or acceleration values are expanded. The type names that are shown in parentheses will be shown in a different column or will be shown with a different alignment.

## UI WIDGETS DETAILED SPECIFICATIONS

### The Grid Widget

The Grid widget will be created inside a newly created window when the Grid widget button is clicked. At first, it will not show anything and direct the user to drag and drop any field or struct from the Struct Window. There will be 2 cells for each field and there will be one row for each field. The first cell will show the name of the field and the second one will show the value of it. It will show the most primitive types of the dragged and dropped structs.

For instance, assume the user dragged and dropped the "time" field in the DUMMY struct. Then the Grid widget will show the "time" cell and its value next to it. But after that, assume that the user has dragged and dropped the velocity struct to the Grid widget. Then the Grid widget will create 3 more fields named x, y, and z and show their values separately (not as a whole struct). Remember that the depth of the nested structs (struct inside struct) can be infinite. But the Grid widget will show only the primitive values, meaning that it will go inside every struct inside struct to find the primitive types.

### The GridLogger Widget

The GridLogger widget will be created inside a newly created window when the GridLogger widget button is clicked. Similar to the Grid widget, it will initially direct the user to drag and drop any field or struct from the Struct Window. However, unlike the Grid widget, the GridLogger displays data in a table format similar to an Excel spreadsheet. Field names are displayed as column headers in the same row, and incoming values are added as new rows below their respective field columns. Each new packet creates a new row in the table, allowing users to see the history of values over time.

The GridLogger will show the most primitive types of the dragged and dropped structs. For instance, if the user drags and drops the velocity struct, the GridLogger will create 3 columns named x, y, and z, displaying their values in separate columns. The depth of nested structs can be infinite, but the GridLogger will only show primitive values.

### The Line Chart Widget

The Line Chart widget will be created inside a newly created window when the Line Chart widget button is clicked. At first, it will not show anything and direct the user to drag and drop any field or struct from the Struct Window. There will be one line for every field, and the Line Chart will show the most primitive types of the dragged and dropped structs.

Every line in the Line Chart will have a different color and the names of the fields will be shown in the right side of the Line Chart, not overlapping the chart. The chart will automatically scale itself to show all incoming values properly. When hovering over any point on the lines with the mouse, the exact value at that point will be displayed as a tooltip. Users can zoom in by drawing a rectangle with the mouse to focus on a specific area of the chart.

### The 3D Chart Widget

The 3D Chart widget is essentially a three-dimensional version of the Line Chart. It will be created inside a newly created window when the 3D Chart widget button is clicked. Users can drag and drop fields from the Struct Window.

The key differences from the Line Chart are:
- Users can assign fields to both X and Y axes (instead of just X in Line Chart)
- By default, all fields will be plotted on the Z-axis
- The chart will automatically scale itself to show all incoming values in 3D space
- When hovering over any point with the mouse, the exact value (x, y, z coordinates) will be displayed as a tooltip

### The Pie Chart Widget

The Pie Chart widget will be created inside a newly created window when the Pie Chart widget button is clicked. It displays the current instant values of fields as pie slices, showing the proportional relationship between different field values. The chart will automatically scale and adjust the slices based on incoming data. When hovering over any slice with the mouse, the exact value and percentage will be displayed as a tooltip.

### The Bar Chart Widget

The Bar Chart widget will be created inside a newly created window when the Bar Chart widget button is clicked. Similar to the Grid widget, it shows current values but displays them as vertical bars instead of a grid. Each field will have its own bar, and the chart will automatically scale to accommodate all values. When hovering over any bar with the mouse, the exact value will be displayed as a tooltip.

## WIDGET SETTINGS AND CONFIGURATION

### Grid Widget Settings:
When the user right-clicks on the Grid Widget, several clickable options will be shown:

**"Settings"**: Opens the settings window with the following tabs:
- **General Tab**:
  - Type conversion options for each field (integer, double, hex, binary, string)
  - Mathematical transformations (multiply, divide, add constant, modulo)
  - Function applications (diff, cumulative sum, moving average with window size)
  - Prefix/postfix text configuration with custom strings
  - Number format settings (decimal places, scientific notation, thousands separator)
  - Field visibility toggles (show/hide specific fields)
- **Trigger Option Tab**:
  - Conditional expression builder with drag-and-drop field names
  - Logical operators (AND, OR, NOT, XOR)
  - Comparison operators (<, >, <=, >=, ==, !=)
  - Mathematical expressions support
  - Trigger enable/disable toggle
  - Trigger history log (last N trigger events)
  - Example: "velocity.x > 0 && time > 5000.0"

### GridLogger Widget Settings:
- **General Tab**: (Same as Grid Widget)
- **Logger Configuration Tab**:
  - Maximum row count (with auto-scroll or auto-delete oldest)
  - Export options (CSV, Excel, JSON)
  - Row highlighting rules based on conditions
  - Timestamp column configuration (show/hide, format)
  - Auto-save interval settings
- **Trigger Option Tab**: (Same as Grid Widget)

### Line Chart Widget Settings:
- **General Tab**: (Same as Grid Widget)
- **Visual Settings Tab**:
  - Line color picker for each field with RGB/HEX input
  - Line thickness slider (1-10 pixels)
  - Line style selector (solid, dashed, dotted, dash-dot)
  - Point style (none, circle, square, triangle, diamond)
  - Point size configuration (if points enabled)
  - Interpolation method (linear, step, smooth curve)
  - Legend position and visibility
- **Axis Configuration Tab**:
  - X-axis field selection dropdown
  - X-axis range (auto, fixed min/max, sliding window)
  - Y-axis range settings per field
  - Axis labels and titles customization
  - Grid lines visibility and style
  - Scale type (linear, logarithmic)
  - Time window size for sliding window mode
  - If no field is assigned to the X-axis, it will automatically increment as 1, 2, 3... based on the number of incoming packets
- **Trigger Option Tab**: (Same as Grid Widget)
- **"Reset Zoom"**: Right-click option that appears when the chart is zoomed in

### 3D Chart Widget Settings:
- **General Tab**: (Same as Grid Widget)
- **Visual Settings Tab**:
  - Line/point colors for each field
  - Line thickness and point size
  - 3D rendering style (wireframe, solid, points cloud)
  - Camera angle presets and custom positioning
  - Lighting configuration
  - Background color/gradient
- **Axis Configuration Tab**:
  - X, Y, and Z axis field selection
  - Independent axis range settings
  - Axis labels and scaling options
  - Grid plane visibility (XY, XZ, YZ planes)
  - Rotation speed for auto-rotate mode
- **Trigger Option Tab**: (Same as Grid Widget)

### Pie Chart Widget Settings:
- **General Tab**: (Same as Grid Widget)
- **Visual Settings Tab**:
  - Slice colors for each field
  - Slice border width and color
  - Label display options (percentage, value, both, none)
  - Label position (inside, outside, leader lines)
  - Animation settings (rotation, explosion effect)
  - Donut hole size (for donut chart variant)
- **Display Options Tab**:
  - Minimum slice threshold (group small slices as "Other")
  - Sort order (by value, alphabetical, custom)
  - Slice selection behavior (highlight, explode, tooltip only)
- **Trigger Option Tab**: (Same as Grid Widget)

### Bar Chart Widget Settings:
- **General Tab**: (Same as Grid Widget)
- **Visual Settings Tab**:
  - Bar colors for each field
  - Bar width and spacing
  - Bar style (solid, gradient, pattern fill)
  - Value labels (show on top, inside, hide)
  - Bar grouping options (grouped, stacked)
  - Animation transitions on update
- **Display Options Tab**:
  - Sort order (ascending, descending, custom, none)
  - Bar orientation (vertical, horizontal)
  - Baseline value configuration
  - Scale type (linear, logarithmic)
- **Trigger Option Tab**: (Same as Grid Widget)

## PACKET PARSING AND STRUCTURE MANAGEMENT

### Structure Types

Users will be able to save the structs that will be incoming when either the Ethernet mode or Offline mode is selected. The "Add Structure Window" will serve the purpose of parsing and saving the structs that users have copy-pasted. There will be 2 different types of structs:

**Reusable Structs**: These structs indicate that they don't serve any purpose on their own. The Reusable Structs will not be the incoming packets in either of the offline or Ethernet modes. Instead, they will act as building blocks for the Packet structs. Moreover, reusable structs can also be used as building blocks of other reusable structs.

**Packet Structs**: These structs indicate that they are the structs that will be incoming when either of the offline and Ethernet modes are selected. Packet Structs will have a unique ID (integer type) that will be incoming in the selected ID field of the selected header struct. The ID number will be used to determine the incoming packet type and parse it correctly. ID and packet will have a one-to-one relationship.

### C++ Structure Parsing Considerations

The application must handle structures compiled by C++ compilers with all their complexities:

**Bit Fields Support**:
- Parse and handle bit fields correctly, including those spanning byte boundaries
- Maintain bit field ordering as defined by the compiler (compiler-specific)
- Support both signed and unsigned bit fields
- Handle padding bits inserted by compiler
- JSON representation must include bit offset and bit width for each field

**Union Support**:
- Correctly parse union definitions
- Store all union members with same offset
- Track active union member (if determinable)
- JSON structure must indicate union type and all possible interpretations
- Widget must allow user to select which union member to display

**Compiler-Specific Handling**:
- Support different padding/alignment rules (pragma pack, attribute packed)
- Handle compiler-specific type sizes
- Detect and handle endianness
- Support both MSVC and GCC/Clang structure layouts

**JSON Structure Representation**:
```json
{
  "structName": "ExampleStruct",
  "size": 64,
  "alignment": 8,
  "fields": [
    {
      "name": "flags",
      "type": "bitfield",
      "baseType": "uint32_t",
      "bitOffset": 0,
      "bitWidth": 3,
      "byteOffset": 0
    },
    {
      "name": "data",
      "type": "union",
      "byteOffset": 4,
      "size": 16,
      "members": [
        {"name": "intVal", "type": "int32_t"},
        {"name": "floatVal", "type": "float"},
        {"name": "bytes", "type": "uint8_t[4]"}
      ]
    }
  ]
}
```

Example:
```c
typedef struct {
    bool isValid;
    int validCount;
}S_COUNT;

typedef struct {
    int x;
    int y;
    int z;
    S_COUNT count;
}Field3D;

typedef struct { // Incoming packet with ID of 5
    S_HEADER header;
    Field3D velocity;
    Field3D acceleration;
    char name[4];
    float time;
}DUMMY;
```

### The Add Structure Window

The "Add Structure Window" will be opened when the "Add Struct" button is clicked. It will have 3 tabs:

**"Header Define" Tab**: 
- Shows the current header struct present in incoming packets
- Text field displaying the header struct
- Smaller text field showing the name of the field used as the ID
- Both fields are editable
- The header field is actually a "reusable struct"

**"Reusable Struct Define" Tab**:
- Two rectangular fields
- Left field: copy-paste area for one or more structs to be saved as reusable structs
- Right field: shows the parsed result similar to the Struct Window
- Shows relevant struct types in red when required struct types are not present

**"Packet Structs Define" Tab**:
- Similar to "Reusable Struct Define" tab
- Additional text field for user to write the packet ID
- Shows error when the selected Header struct is not present at the top of the packet struct

## PACKET SUBSCRIPTION AND PROCESSING SYSTEM

### Widget Subscription Mechanism

All UI Widgets must subscribe to specific packet types rather than individual field values. This is because most UI widgets will typically display data from a single packet type. The subscription system works as follows:

- Each UI Widget subscribes to one or more packet IDs upon creation or configuration
- When a packet arrives with a matching ID, all subscribed widgets receive the complete packet
- Widgets process the entire packet according to their configuration
- Multiple widgets can subscribe to the same packet type simultaneously
- The subscription manager maintains a registry of widget-to-packet-ID mappings for efficient distribution

### Data Processing Pipeline

When a packet arrives at a UI Widget, it must follow this strict processing order:

1. **Raw Packet Reception**: Widget receives the complete packet data
2. **Field Extraction**: Extract relevant fields based on widget configuration
3. **Data Transformation**: Apply all configured transformations in sequence:
   - Type conversion (e.g., double to integer, value to hex)
   - Mathematical operations (multiplication by constants, unit conversions)
   - Function applications (diff, average, min, max, etc.)
   - Prefix/postfix additions for display
4. **Trigger Evaluation**: After transformation, evaluate trigger conditions:
   - If triggers are enabled and conditions are not met, discard the update
   - If triggers are disabled or conditions are met, proceed to display
5. **Display Update**: Update the widget's visual representation with processed data

### Field Processing Using Offset and Size

The application must NEVER use field names for actual data processing calculations. Instead, all packet parsing and field extraction must be performed using:

**Offset-Based Processing**:
- Each field has a byte offset from the packet start
- Fields are accessed by calculating: `packet_base_address + field_offset`
- Nested structures require cumulative offset calculation
- All offsets must be pre-calculated during struct definition parsing
- Create an offset lookup table for each packet type for O(1) access

**Size-Based Processing**:
- Each field has a defined size in bytes
- Memory copy operations use: `memcpy(destination, packet_base + offset, size)`
- Bit fields require additional bit offset and bit size parameters
- Endianness must be considered for multi-byte fields
- Padding and alignment rules must be strictly followed

**Implementation Requirements**:
- Generate a binary layout map for each packet structure
- Use memory-mapped access for maximum performance
- Implement zero-copy mechanisms where possible
- Cache offset/size calculations to avoid repeated computation
- Use SIMD instructions for bulk data processing when applicable

## WORKSPACE MANAGEMENT SYSTEM

### Workspace File Structure

The workspace must save the complete application state in a hierarchical JSON structure:

```json
{
  "workspace": {
    "version": "1.0.0",
    "name": "MyWorkspace",
    "created": "timestamp",
    "modified": "timestamp",
    "structReferences": {
      "reusableStructsFile": "path/to/reusable_structs.json",
      "packetStructsFile": "path/to/packet_structs.json"
    },
    "testFramework": {
      "enabled": true,
      "testsFile": "path/to/tests_config.json",
      "failureHistory": []
    },
    "tabs": [
      {
        "name": "Tab1",
        "order": 0,
        "widgets": [
          {
            "type": "Grid",
            "id": "unique_widget_id",
            "position": {"x": 100, "y": 100},
            "size": {"width": 400, "height": 300},
            "subscribedPackets": [1, 5, 7],
            "fields": ["velocity.x", "velocity.y", "time"],
            "settings": {
              "general": {...},
              "trigger": {...}
            }
          }
        ]
      }
    ],
    "globalSettings": {...}
  }
}
```

### Struct Storage

**Reusable Structs File (reusable_structs.json)**:
- Contains all user-defined reusable structures
- Includes structure dependencies and hierarchy
- Maintains version control for struct modifications
- Validates circular dependencies on load

**Packet Structs File (packet_structs.json)**:
- Contains all packet structure definitions
- Maps packet IDs to structure definitions
- Includes header structure reference
- Stores offset/size pre-calculations

### Workspace Loading Process
1. Load and validate workspace file
2. Load referenced reusable structs file
3. Load referenced packet structs file
4. Rebuild offset/size lookup tables
5. Recreate tabs in specified order
6. Instantiate all widgets with saved positions
7. Restore widget settings and subscriptions
8. Connect to data sources (Ethernet/Offline)
9. Begin packet processing

## PERFORMANCE REQUIREMENTS AND OPTIMIZATION

### Real-Time Processing Capabilities
- Must handle packet rates exceeding 200 Hz per packet type
- Support multiple packet types arriving at different rates simultaneously
- Total system throughput: minimum 10,000 packets/second across all types
- Maximum latency from packet arrival to display: < 5 milliseconds
- Zero packet loss under normal operating conditions

### Real-Time Performance Classifications

**Hard Real-Time Requirements**:
- Packet reception and timestamping: < 10 microseconds
- Safety-critical test execution: < 50 microseconds
- Emergency stop triggers: < 1 millisecond

**Soft Real-Time Requirements**:
- Widget updates: < 16.67ms (60 FPS)
- Non-critical test execution: < 1 millisecond  
- Chart rendering: < 33ms (30 FPS minimum)

**Best-Effort Processing**:
- Statistics calculations
- Log file writing
- Workspace auto-save
- Export operations

### Thread Safety Architecture

**Thread Separation Strategy**:
- **Network Thread**: Dedicated to packet reception only
- **Parser Thread Pool**: Multiple threads for packet parsing
- **Test Execution Threads**: Separate thread pool for test framework
- **Widget Processing Threads**: One thread per widget type
- **UI Thread**: Qt main thread for GUI only

**Synchronization Mechanisms**:
- Lock-free ring buffers for packet queuing
- Read-write locks for shared packet data
- Atomic operations for statistics counters
- Message passing for inter-thread communication
- Copy-on-write for packet distribution to widgets

**Data Ownership Rules**:
- Packets are immutable after parsing
- Each widget owns its display data copy
- Test framework has read-only access to packet stream
- No shared mutable state between threads

### Event-Driven Architecture
- **NO POLLING**: Implement pure event-driven processing
- Use callback mechanisms for packet arrival notifications
- Implement async/await patterns for non-blocking operations
- Use message queues with back-pressure handling
- Separate UI thread from packet processing threads

### Performance Optimization Strategies

**Memory Management**:
- Pre-allocate memory pools for packet buffers
- Use ring buffers for packet queuing
- Implement custom memory allocators for hot paths
- Minimize heap allocations in critical sections
- Use stack allocation for temporary data

**Multi-Threading Architecture**:
- Dedicated thread for network packet reception
- Worker thread pool for packet parsing
- Separate threads for each widget type processing
- UI updates batched and rate-limited (e.g., 60 FPS max)
- Lock-free data structures for inter-thread communication

**Data Processing Optimization**:
- SIMD vectorization for numerical computations
- Batch processing for similar operations
- Lazy evaluation for non-visible widgets
- Incremental updates instead of full redraws
- GPU acceleration for chart rendering (OpenGL/Vulkan)

**Caching Strategies**:
- LRU cache for transformed data
- Memoization of trigger condition results
- Pre-computed lookup tables for conversions
- Spatial indexing for 3D chart data
- Delta compression for GridLogger storage

### Scalability Requirements
- Support 100+ simultaneous widgets without degradation
- Handle workspace files up to 100 MB
- Maintain performance with 1 million+ data points in charts
- Support packet sizes from 1 byte to 64 KB
- Graceful degradation under system resource constraints

### Monitoring and Diagnostics
- Built-in performance profiler
- Packet loss detection and reporting
- Processing latency histograms
- Memory usage tracking per widget
- CPU usage per processing stage
- Network bandwidth utilization metrics
- Frame rate monitoring for UI updates

## ERROR RECOVERY AND RESILIENCE

### Widget Failure Handling
- **Widget Crash Isolation**: Each widget runs in isolated context
- **Automatic Restart**: Failed widgets automatically restart with last known good configuration
- **State Recovery**: Widget state saved every 5 seconds for recovery
- **Failure Logging**: Detailed crash dumps saved for debugging
- **Graceful Degradation**: Other widgets continue operating if one fails

### Packet Processing Errors
- **Malformed Packet Handling**: 
  - Log and skip malformed packets
  - Maintain statistics on packet errors
  - Continue processing subsequent packets
- **Buffer Overflow Protection**:
  - Pre-validated buffer sizes
  - Boundary checks on all memory operations
  - Automatic packet dropping when buffers full
- **Parsing Error Recovery**:
  - Try multiple parser strategies
  - Fall back to raw hex display
  - Alert user with detailed error info

### Network Failure Recovery
- **Connection Loss Handling**:
  - Automatic reconnection attempts with exponential backoff
  - Queue packets during brief disconnections
  - Alert user after 3 failed attempts
- **Timeout Management**:
  - Configurable timeouts for all network operations
  - Separate timeout for each packet type
  - Watchdog timer for hung connections

### Resource Exhaustion Handling
- **Memory Management**:
  - Automatic data pruning when memory threshold reached (80%)
  - User warning at 70% memory usage
  - Emergency data dump and restart at 95%
- **CPU Overload**:
  - Automatic widget frame rate reduction
  - Test execution throttling
  - Packet sampling mode activation
- **Disk Space**:
  - Automatic log rotation
  - Old data compression
  - User alerts for low disk space

## PERFORMANCE VISUALIZATION

### Real-Time Performance Dashboard

A dedicated performance monitoring dashboard accessible via toolbar button showing:

**System Overview**:
- Overall CPU usage gauge with color coding (green < 50%, yellow 50-80%, red > 80%)
- Memory usage bar graph with trend line
- Network throughput meter (packets/sec and MB/sec)
- Disk I/O indicators

**Per-Widget Metrics**:
- Individual CPU usage per widget
- Memory consumption per widget
- Update rate (FPS) per widget
- Processing latency per widget
- Packet queue depth visualization

**Packet Processing Pipeline**:
- Visual pipeline showing:
  - Network reception rate
  - Parser throughput
  - Widget distribution timing
  - Test execution overhead
- Bottleneck highlighting (slowest stage shown in red)
- Queue depth at each stage

**Performance Alerts**:
- Real-time notifications for:
  - Frame drops
  - Packet loss
  - Memory pressure
  - CPU throttling
  - Test execution delays

**Historical Trends**:
- 5-minute rolling window graphs for all metrics
- Performance event log
- Exportable performance reports

## ETHERNET MODE CONFIGURATION

### Network Settings

When Ethernet mode is selected, the Port Settings button opens a configuration dialog with:

**Connection Parameters**:
- **Remote Device Settings**:
  - Destination IP Address
  - Destination MAC Address  
  - Destination Port (UDP/TCP)
- **Local Device Settings**:
  - Source IP Address (auto-detect or manual)
  - Source Port
  - Network Interface Selection (if multiple NICs)
- **Protocol Settings**:
  - Protocol Type (UDP/TCP)
  - Multicast Group (if applicable)
  - Receive Buffer Size
  - Socket Timeout

**Connection Profiles**:
- Save multiple connection profiles with names
- Quick profile switching via dropdown
- Import/Export profiles
- Default profile on startup
- Profile data saved in workspace for persistence

**Network Diagnostics**:
- Ping test to destination
- Port availability check
- Network statistics display
- Packet capture for debugging

## OFFLINE MODE OPERATION

### File Handling

**Drag and Drop Support**:
- Users can drag any file directly onto the application
- No specific format requirements - the app will attempt to parse based on configured structures
- Multiple file support (play files sequentially)
- Automatic file type detection based on content

**Playback Controls**:
- **Play/Pause**: Start or pause playback at current position
- **Stop**: Reset to beginning of file
- **Step Forward/Backward**: Advance or rewind by single packet
- **Jump to Specific Point**:
  - By packet number
  - By timestamp
  - By percentage of file
  - By bookmark
- **Playback Speed**: 
  - Adjustable from 0.1x to 10x normal speed
  - Frame-by-frame mode for debugging
  - Real-time mode (match original timing)

**File Processing**:
- Load file into memory-mapped buffer for performance
- Build packet index for quick seeking
- Support files up to available system memory
- Background indexing for large files

## SIMULATION MODE (DEVELOPMENT ONLY)

### Built-in Test Framework

For development and testing purposes, the application includes a simulation mode with:

**Default Packet Structures**:
```c
// Pre-defined test structures for immediate use
typedef struct {
    uint32_t packet_id;
    uint32_t sequence;
    uint64_t timestamp;
} TEST_HEADER;

typedef struct {
    TEST_HEADER header;
    float sine_wave;
    float cosine_wave;
    float ramp;
    uint32_t counter;
    bool toggle;
} SIGNAL_TEST_PACKET; // ID: 1001

typedef struct {
    TEST_HEADER header;
    float x, y, z;
    float vx, vy, vz;
    float ax, ay, az;
} MOTION_TEST_PACKET; // ID: 1002

typedef struct {
    TEST_HEADER header;
    uint32_t status_flags : 8;
    uint32_t error_code : 8;
    uint32_t warning_flags : 16;
    float temperatures[4];
    float voltages[4];
} SYSTEM_TEST_PACKET; // ID: 1003
```

**Data Generation Patterns**:
- **Sine/Cosine Waves**: Configurable frequency and amplitude
- **Ramp Functions**: Linear, exponential, sawtooth
- **Random Data**: Uniform, Gaussian distributions
- **Step Functions**: Configurable step size and interval
- **Bit Patterns**: Rotating bits, walking ones/zeros
- **Realistic Motion**: Simulated vehicle/aircraft dynamics

**Stress Test Framework**:
- **Packet Rate Testing**:
  - Generate up to 10,000 packets/second
  - Multiple packet types simultaneously
  - Variable rate patterns (burst, steady, random)
- **Data Volume Testing**:
  - Large packet sizes (up to 64KB)
  - Millions of packets continuous stream
  - Memory stress testing
- **Error Injection**:
  - Malformed packets
  - Out-of-order packets
  - Packet loss simulation
  - Bit errors
  - CRC failures
- **Performance Metrics**:
  - Automatic performance profiling during stress tests
  - Report generation with bottleneck analysis
  - Regression testing against previous versions

**Simulation Controls**:
- Start/Stop simulation buttons
- Packet rate control (1-10000 Hz)
- Packet type selection checkboxes
- Error injection probability sliders
- Scenario presets (Normal, Stress, Error, Performance)
- Record simulation data to file for replay

## OFFLINE OPERATION REQUIREMENTS

### Standalone Operation

The application is designed to operate on computers without internet connectivity:

**No External Dependencies**:
- All libraries bundled with installation
- No cloud services required
- No online activation or licensing checks
- No automatic updates (manual update via USB)

**Local Resource Management**:
- All configuration stored locally
- Help documentation included in installation
- Example files and templates bundled
- No telemetry or analytics

**Security Considerations**:
- No outbound network connections except configured Ethernet mode
- File system access limited to user-specified directories
- No automatic file uploads or sharing
- Workspace encryption option for sensitive data

## DEVELOPMENT COLLABORATION NOTES

### Claude Code Integration

This specification is designed for implementation with Claude Code. Key considerations:

**Project Structure Recommendations**:
```
monitor-app/
├── src/
│   ├── core/           # Packet processing, parsing
│   ├── widgets/        # UI widget implementations
│   ├── network/        # Ethernet mode handling
│   ├── offline/        # File playback logic
│   ├── testing/        # Test framework
│   ├── simulation/     # Simulation mode
│   └── performance/    # Performance monitoring
├── include/            # Header files
├── resources/          # UI resources, icons
├── config/            # Default configurations
├── tests/             # Unit and integration tests
└── docs/              # Documentation
```

**Implementation Priorities**:
1. Core packet parsing and structure management
2. Basic Grid widget with drag-drop
3. Ethernet mode packet reception
4. Offline file playback
5. Other widgets incrementally
6. Test framework
7. Performance optimizations

**Technology Stack**:
- Qt6 for UI (QtWidgets, QtCharts, Qt3D)
- C++17 or later
- CMake build system
- Cross-platform (Windows/macOS)

### Overview

The Monitor application will include a comprehensive Real-Time Test Framework that automatically validates incoming packet data against user-defined rules and conditions. This framework will operate continuously without interrupting the main application flow, providing immediate feedback when test conditions fail.

### Access and Interface

**Test Framework Button**: 
- A dedicated button labeled "Test Framework" or "RT Tests" will be added to the main toolbar alongside other UI buttons
- Clicking this button opens the "Real-Time Test Manager" window
- The button will show a status indicator (green = all tests passing, red = failures present, yellow = tests paused)

### Real-Time Test Manager Window

The Real-Time Test Manager is a standalone window (independent of tabs) that consists of multiple sections:

#### Test Definition Panel

**Test Builder Interface**:
- **Visual Test Builder**: Drag-and-drop interface for creating test conditions
  - Field selector tree showing all available packets and their fields
  - Operator palette (comparison, logical, mathematical)
  - Time-based condition builders
  - State machine designers for complex sequential tests

- **Script Editor Mode**: Advanced text-based test definition
  - Syntax-highlighted editor for writing test expressions
  - Auto-completion for packet names, field names, and functions
  - Real-time syntax validation
  - Test expression language supporting:
    - Field references: `PacketA.structB.fieldC`
    - Previous values: `PacketA.fieldF[-1]` (previous value), `PacketA.fieldF[-n]` (n packets ago)
    - Time references: `PacketA.time@(structB.bitC==1)` (timestamp when condition met)
    - Mathematical operations: `abs()`, `diff()`, `avg()`, `min()`, `max()`
    - Logical operations: `&&`, `||`, `!`, `XOR`

#### Test Categories

Tests will be organized into categories:

**1. Simple Field Validation**:
```
Test: "Velocity Range Check"
Condition: DUMMY.velocity.x >= -100 && DUMMY.velocity.x <= 100
Action: ERROR if false
Message: "Velocity X out of valid range"
```

**2. Cross-Field Validation**:
```
Test: "Acceleration Consistency"
Condition: abs(DUMMY.acceleration.x) < 10 when DUMMY.velocity.x == 0
Action: WARNING if false
Message: "Acceleration detected while velocity is zero"
```

**3. Temporal Validation**:
```
Test: "Bit Transition Timing"
Condition: (DUMMY.time@(structB.bitC==1) - DUMMY.time@(structB.bitE==0)) < 200
Action: ERROR if false
Message: "Bit transition time exceeded 200ms"
```

**4. Sequential State Validation**:
```
Test: "Field Delta Check"
Condition: abs(DUMMY.fieldF - DUMMY.fieldF[-1]) < 0.5
Action: WARNING if false
Message: "Field F changed by more than 0.5 between consecutive packets"
```

**5. Statistical Validation**:
```
Test: "Average Range Check"
Condition: avg(DUMMY.velocity.x, last=10) < 50
Action: INFO if false
Message: "Average velocity over last 10 packets exceeds threshold"
```

### Test Configuration Options

Each test will have the following configurable parameters:

**Test Properties**:
- **Name**: Unique identifier for the test
- **Description**: Detailed explanation of what the test validates
- **Category**: Grouping for organization (Safety, Performance, Data Integrity, etc.)
- **Severity Level**: INFO, WARNING, ERROR, CRITICAL
- **Enabled/Disabled**: Toggle to activate/deactivate individual tests
- **Packet Subscription**: Which packet IDs this test monitors

**Trigger Conditions**:
- **Evaluation Frequency**: Every packet, every N packets, time-based (every X ms)
- **Prerequisite Conditions**: Only run test if certain conditions are met
- **Cooldown Period**: Minimum time between consecutive failure reports
- **Failure Threshold**: Number of failures before triggering notification

**Actions on Failure**:
- **Notification Type**: 
  - Visual indicator in main window
  - Pop-up notification
  - Sound alert
  - Log entry
  - Email notification (future feature)
- **Data Capture**: 
  - Save failing packet data
  - Capture N packets before/after failure
  - Screenshot widget states
- **Application Response**:
  - Continue normally (default)
  - Pause data reception
  - Trigger emergency stop (for critical failures)
  - Execute custom script

### Test Results Display

#### Real-Time Results Panel

The Test Results Panel will show:

**Summary Section**:
- Total tests running
- Tests passed/failed/skipped
- Overall system health indicator
- Performance metrics (tests/second executed)

**Failure List**:
- Chronological list of test failures
- Each entry shows:
  - Timestamp (with microsecond precision)
  - Test name and category
  - Failure message
  - Packet ID and sequence number
  - Actual vs. expected values
  - Quick link to relevant widget

**Detailed View**:
- Double-clicking a failure opens detailed view:
  - Complete packet data at failure time
  - Historical data graph showing the failure point
  - Related test conditions
  - Stack trace of test execution
  - Suggestions for resolution

#### Statistics Dashboard

**Test Performance Metrics**:
- Pass/fail ratio per test
- Failure frequency over time
- Most common failure patterns
- Correlation analysis between failures
- Performance impact of tests on system

**Visualization Options**:
- Timeline view of failures
- Heat map of failure distribution
- Pie chart of failure categories
- Trend analysis graphs

### Test Management Features

#### Test Sets

**Test Set Organization**:
- Group related tests into sets
- Enable/disable entire sets at once
- Import/export test sets
- Version control for test definitions
- Share test sets between workspaces

**Test Templates**:
- Pre-built test templates for common validations
- Customizable template parameters
- Template library management
- Community-shared templates (future feature)

#### Test Execution Control

**Execution Modes**:
- **Continuous Mode**: Tests run on every applicable packet
- **Sampling Mode**: Tests run on every Nth packet
- **Triggered Mode**: Tests run when specific conditions are met
- **Manual Mode**: Tests run only when manually triggered

**Performance Optimization**:
- Automatic test prioritization based on failure likelihood
- Parallel test execution for independent tests
- Lazy evaluation for complex conditions
- Caching of intermediate results
- JIT compilation of test expressions

### Storage and Persistence

#### Test Configuration Storage

Tests will be saved in JSON format as part of the workspace:

```json
{
  "testFramework": {
    "version": "1.0.0",
    "enabled": true,
    "tests": [
      {
        "id": "test_001",
        "name": "Bit Transition Timing",
        "category": "Temporal",
        "enabled": true,
        "severity": "ERROR",
        "condition": {
          "type": "temporal",
          "expression": "(DUMMY.time@(structB.bitC==1) - DUMMY.time@(structB.bitE==0)) < 200",
          "packets": ["DUMMY"]
        },
        "trigger": {
          "frequency": "every_packet",
          "cooldown_ms": 1000
        },
        "actions": {
          "on_failure": {
            "notify": true,
            "capture_data": true,
            "capture_range": {"before": 5, "after": 5}
          }
        },
        "statistics": {
          "total_executions": 10000,
          "failures": 23,
          "last_failure": "timestamp",
          "avg_execution_time_us": 12
        }
      }
    ],
    "testSets": [...],
    "globalSettings": {
      "maxConcurrentTests": 100,
      "testTimeout_ms": 1000,
      "failureHistorySize": 10000
    }
  }
}
```

#### Test Results Logging

**Logging System**:
- Rotating log files with configurable size limits
- Structured logging in JSON format
- Separate logs for different severity levels
- Integration with system logging tools
- Export capabilities (CSV, JSON, XML)

### Advanced Features

#### Intelligent Test Generation

**Auto-Generate Tests**:
- Analyze packet structure to suggest tests
- Learn normal ranges from historical data
- Detect anomalies and create tests automatically
- Machine learning-based test recommendations

#### Test Coverage Analysis

**Coverage Metrics**:
- Field coverage: Which fields are being tested
- Value range coverage: What values have been tested
- Temporal coverage: Time-based condition coverage
- Path coverage: State machine transition coverage

#### Integration Capabilities

**External Integration**:
- REST API for test control and results
- Webhook notifications for failures
- Integration with CI/CD pipelines
- MQTT publishing of test results
- Database logging support

### Performance Considerations

**Test Framework Performance**:
- Tests must execute in < 100 microseconds average
- Support for 1000+ concurrent tests
- Memory usage < 100MB for test framework
- Zero impact on main packet processing when tests are disabled
- Graceful degradation under high load

**Optimization Strategies**:
- Compile test expressions to bytecode
- Use SIMD for numerical comparisons
- Implement test result caching
- Parallel execution on multi-core systems
- GPU acceleration for complex statistical tests

### User Interface Guidelines

**Design Principles**:
- Clear visual distinction between test states
- Intuitive drag-and-drop test creation
- Real-time feedback during test editing
- Contextual help and documentation
- Keyboard shortcuts for common operations

**Accessibility Features**:
- Color-blind friendly status indicators
- Screen reader support for test results
- Configurable font sizes and themes
- Exportable test reports for offline review

## ADDITIONAL FEATURES

### The UI Design
The UI will have a modern look with smooth animations and transitions. All widgets should provide immediate visual feedback for user interactions.

### Error Handling
- Comprehensive error messages for struct parsing failures
- Graceful handling of malformed packets
- Recovery mechanisms for connection losses
- Validation of all user inputs
- Rollback capabilities for failed operations

### Future Extensibility
- Plugin architecture for custom widgets
- Scripting support for advanced transformations
- Export capabilities for analysis data
- Integration with external tools and databases
- Cloud workspace synchronization support