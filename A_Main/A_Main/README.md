# BLE Working - Split Arduino Project

This project has been split into multiple `.ino` files for better organization and maintainability.

## File Structure

The files are prefixed with letters (A-I) to ensure proper compilation order:

### Core Files (Compiled First)
1. **A_Main.ino** - Main sketch with setup() and loop()
   - Contains all configuration defines
   - Struct definitions (LinkMetric, NeighbourEntry)
   - Main program flow
   - Forward declarations for all globals

2. **B_DataStructures.ino** - Global variable definitions
   - WiFiUDP, BLE pointers
   - Neighbour array
   - State variables

### Functional Modules (Compiled in Order)
3. **C_EnergyModel.ino** - Energy measurement and estimation
   - Energy tracking variables
   - RSSI-based energy estimation functions
   - Real-time energy measurement

4. **D_LinkMetrics.ino** - Link quality estimation
   - BLE latency and reliability estimation
   - WiFi latency and reliability estimation
   - RSSI-based metric calculation

5. **E_HelperUtils.ino** - Utility functions
   - Node name validation
   - Node number extraction
   - Scan slot timing
   - Link-state packet building

6. **F_NeighbourManagement.ino** - Neighbour discovery and management
   - Neighbour table operations
   - BLE/WiFi neighbour upsert functions
   - Best neighbour selection algorithms
   - Protocol selection logic

7. **G_BLE_Functions.ino** - BLE-specific functionality
   - BLE advertising setup
   - BLE scanning callbacks
   - Scan round management

8. **H_WiFi_Functions.ino** - WiFi-specific functionality
   - WiFi setup and connection
   - UDP message handling
   - Hello packet broadcasting
   - Gateway communication

9. **I_Display.ino** - Display and debugging
   - M5StickC display updates
   - Neighbour table printing
   - Status visualization

## Why the Letter Prefixes?

Arduino IDE compiles `.ino` files **in alphabetical order**. The letter prefixes ensure:
- `A_Main.ino` compiles first → defines structs and macros
- `B_DataStructures.ino` second → defines global variables
- Other files compile after → can use structs and variables

Without this ordering, you get "not declared" errors because files would reference things defined in later files.

## How It Works

Arduino IDE automatically combines all `.ino` files in the same folder during compilation. The files are concatenated in alphabetical order, but because of how C++ compilation works:

- Function declarations can appear after their usage
- All files share the same global scope
- Variables and functions defined in one file are accessible in others
- **Structs and #defines must be declared before use**

## Usage Instructions

1. Create a new folder named `BLE_Working` (or any name you want)
2. Place ALL `.ino` files in this folder
3. Open `A_Main.ino` in Arduino IDE
4. The IDE will show all files as tabs
5. Compile and upload as normal

## Configuration

To configure for different nodes, edit these defines in `A_Main.ino`:

```cpp
#define NODE_ID             "NODE_2"
#define NODE_NUM            2
#define BLE_DEVICE_NAME     "NODE_2"

#define WIFI_SSID           "YourSSID"
#define WIFI_PASSWORD       "YourPassword"
```

## Dependencies

- M5StickCPlus library
- ESP32 BLE libraries (built-in)
- WiFi library (built-in)

## Benefits of This Split

✅ **Better Organization** - Each file has a clear, single responsibility
✅ **Easier Maintenance** - Find and fix code more easily
✅ **Better Readability** - Smaller, focused files
✅ **Reusability** - Easy to copy specific modules to other projects
✅ **Team Collaboration** - Different people can work on different modules

## Troubleshooting

**"Not declared" errors during compilation?**
- Make sure all files are in the same directory
- Verify the letter prefixes are correct (A through I)
- Check that `A_Main.ino` is the main file being opened

**Need to rename files?**
- Keep the alphabetical order: A, B, C, D, E, F, G, H, I
- Or remove the prefixes and put everything back in one file

## Note

All files must be in the same directory for Arduino IDE to recognize them as part of the same sketch. The main file (A_Main.ino) must be opened for the sketch to load properly.
