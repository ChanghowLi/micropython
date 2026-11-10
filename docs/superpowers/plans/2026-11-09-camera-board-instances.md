# Camera board instances implementation plan

Goal: Split the board camera singleton into one DVP and one MIPI object, preserving legacy DVP preview.
Architecture: Reuse CameraConfig and existing Camera capture APIs. Explicit configurations borrow their bus and buffers; the legacy DVP wrapper owns its shared IIC0 reference.
Constraints: Keep the change small, follow existing C style, do not compile or run tests per user instruction.
Scope: Instance management only. MIPI VIN/CSI hardware configuration and MIPI dynamic buffers are subsequent work.

- [x] Add independent board state and configuration/teardown entry points.
- [x] Preserve legacy DVP initialization and preview.
- [x] Store each camera's SCCB bus and select it before register control.
- [x] Reject two OV5640 devices on the same controller (no address remapping or mux support).
- [x] Read back changed code and document ownership and remaining integration.

Ruling: Sensor configuration/lifecycle calls remain application-serialized, matching the existing lifecycle contract. DMA capture states are already separate in the driver. No new locking framework is introduced.
Validation: Source inspection only; no claim of build or hardware success.
