# OV5640 MIPI C integration

Goal: Connect the existing MIPI CSI/VIN configuration to the board Camera object with dynamic SDRAM receive buffers, independently of HDMI.
Constraints: Keep existing C APIs and driver style. Do not build or run tests. Preserve generated files.

- [x] Locate existing CSI/VIN configuration, interrupts and local board reference.
- [x] Reuse generated 1024x600 settings; allocate three aligned frames from the existing FreeRTOS heap.
- [x] Add board initialization and integrate owned buffer/bus cleanup.
- [x] Refresh VIN configuration on restart and select each Camera's SCCB bus on start/stop.
- [x] Wait for sensor/CSI/VIN idle before releasing capture resources.
- [x] Protect shared PHY ownership and fix VIN status control-pointer indirection.
- [x] Document frame lifetime, fixed dimensions, same-address dual-camera restriction and retained generated buffers.
- [x] Final source inspection (no compilation or execution).

Correction: MIPI/VIN instances already existed in common_data.c; earlier statements based only on hal_data.c were incomplete.
