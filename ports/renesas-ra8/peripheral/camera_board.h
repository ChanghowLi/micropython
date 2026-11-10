#ifndef CAMERA_BOARD_H
#define CAMERA_BOARD_H

#include <stddef.h>
#include "camera/camera.h"

/* J300 DVP, RGB565 640x480. Call from a task after SDRAM and camera XCLK
 * are ready. Lifecycle calls must be serialized by the application.
 * The board owns the returned DVP object; release it with Deinitialize. */
uint32_t Camera_BoardInitialize(CameraModelEnum model, Camera **camera);
uint32_t Camera_BoardDeinitialize(void);

/* One independent object per interface. OV7725 is DVP-only.
 * Supply an initialized SCCB bus and the matching CEU/VIN instance.
 * The caller owns that bus and all capture buffers, and prepares power,
 * XCLK and SDRAM. This entry neither starts capture nor initializes HDMI.
 * Reinitializing an occupied interface returns FSP_ERR_IN_USE.
 * Stop all users before deinitializing that interface; release caller-owned
 * buffers/bus only after success. Serialize lifecycle and sensor control
 * calls across both interfaces; DMA capture can run on both simultaneously. */
uint32_t Camera_BoardInitializeWithConfig(const CameraConfig *config, Camera **camera);
uint32_t Camera_BoardDeinitializeInterface(CameraInterfaceEnum interface);

/* OV5640 MIPI, 1024x600, VIN RGB565 output, three dynamic SDRAM frames.
 * Uses board IIC0, module 24 MHz oscillator and existing VIN/CSI configuration.
 * Call CameraStartCapture(camera, NULL), then CameraIsCaptureDone(camera).
 * Frames rotate continuously; copy promptly, or stop capture before reading
 * a stable frame (poll CameraIsCaptureDone again after stopping).
 * Stop all readers before DeinitializeInterface(MIPI_CSI),
 * which releases the board-owned buffers and IIC0 reference.
 * Do not change resolution/format on this fixed-size board configuration.
 * MIPI CSI and the MIPI display share PHY hardware and cannot run together. */
uint32_t Camera_BoardInitializeMIPI(Camera **camera);

/* Two OV5640 modules on board IIC0, without HDMI. Both must initially be idle.
 * Owns DVP XCLK and three dynamic frames per path; returns running objects.
 * DVP: 640x480 RGB565 at SCCB 0x3D. MIPI: 1024x600 RGB565 at 0x3C.
 * Poll each object with CameraIsCaptureDone; frames rotate and are borrowed.
 * CameraStopCapture stops either path independently without freeing its frames;
 * CameraStartCapture(camera, NULL) restarts that path. Serialize all controls.
 * CameraDual_Stop closes both and frees resources; retry it after cleanup errors.
 * Do not reset sensors or change format/resolution during a dual session.
 * Stop all image readers before releasing buffers. This is not frame sync. */
uint32_t CameraDual_Start(Camera **dvp, Camera **mipi);
uint32_t CameraDual_Stop(void);
/* Active includes resources retained after failed cleanup; retry Stop.
 * Running describes a successfully started dual session until Stop is called. */
bool CameraDual_IsActive(void);
bool CameraDual_IsRunning(void);
/* Copy one RGB565 frame into caller-owned memory. Pauses only the selected
 * path and restarts it before returning. Serialize with all camera controls.
 * timeout_ms bounds waiting for a completed frame, not stop/restart time.
 * On stop/restart failure, call CameraDual_Stop to clean up the session. */
uint32_t CameraDual_CopyFrame(CameraInterfaceEnum interface, uint8_t *buffer,
    size_t size, uint32_t timeout_ms);

/* OV5640 or OV7725 DVP -> CEU A/B -> HDMI preview, controlled by a task.
 * Start is non-blocking; Start() keeps OV5640 as the legacy default. */
bool CameraPreview_Start(void);
bool CameraPreview_StartModel(CameraModelEnum model);
/* Single-camera VGA RGB565 snapshot copied by the running preview task.
 * Returns only when its destination is no longer in use; no DMA ring exposed.
 * CameraPreview_CopyFrame cannot run concurrently with another snapshot. */
uint32_t CameraPreview_CopyFrame(uint8_t *buffer, size_t size, uint32_t timeout_ms);
/* Retry cleanup of resources retained after a failed preview task shutdown.
 * The caller must first wait for CameraPreview_IsRunning() to become false. */
uint32_t CameraPreview_Cleanup(void);
void CameraPreview_RequestStop(void);
bool CameraPreview_IsRunning(void);
CameraModelEnum CameraPreview_GetModel(void);
uint32_t CameraPreview_GetLastError(void);

#endif
