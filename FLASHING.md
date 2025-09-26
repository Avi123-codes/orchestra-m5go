Flashing guide — Orchestra M5GO

This file explains how to build and flash the project for the different device roles
(Conductor and Performers). You can either set the role at compile-time with
`DEVICE_ROLE` or use the discovery/assignment flow at runtime.

Quick notes
- Conductor role should be ROLE_CONDUCTOR (device id 0).
- Performer roles are ROLE_PART_1 .. ROLE_PART_4 (device ids 1..4).
- The repository includes example PlatformIO envs you can enable/copy in `platformio.ini`.

Build & flash (PlatformIO)
1. Using a named environment (recommended):
   - Edit `platformio.ini` and uncomment the `[env:conductor]` or `[env:partX]` block
     and set the correct `upload_port` for the USB port of the device you want to flash.
   - Run:

     pio run -e conductor -t upload

   - Or for a performer:

     pio run -e part1 -t upload

2. Passing a build flag on the CLI (one-off):

   pio run -t upload -e m5stack-core-esp32 -- -DDEVICE_ROLE=ROLE_PART_2

   This compiles the default environment but injects the `DEVICE_ROLE` macro.

3. Edit-and-build (not recommended for many devices):
   - Edit `src/main.c` and change the default `#define DEVICE_ROLE` macro near the top.
   - Build and flash as normal.

Notes about device ids and `espnow_init()`
- `orchestra_init()` sets the device id from the role and calls `espnow_init(device_id)`.
  That keeps behavior deterministic when you explicitly set the role.
- `espnow_init(0)` has two-mode fallback: if a configured role exists (NVS, GPIO, or
  compile-time `DEVICE_ROLE`), it uses that role as the id. If the role is UNKNOWN it
  derives a performer id from the STA MAC address (simple, deterministic mapping to 1..4).

Recommended workflow
1. Decide which physical device will be the conductor. Build and flash it with
   `ROLE_CONDUCTOR`.
2. For performers, either: a) use the example per-role envs and flash each device with
   the appropriate `ROLE_PART_X` env, or b) let the conductor assign roles during
   discovery (run conductor first, then power on performers and let the conductor assign).

Troubleshooting
- If two performers end up with the same role, either use the discovery role-assignment
  or set `DEVICE_ROLE` in the build so each binary is explicit.
- If audio doesn't start on performers while the conductor broadcasts, check serial logs
  on performers to see clock offset logs (HEARTBEAT / Clock offset updated). Increase
  SYNC_LEAD_US in `espnow_comm.c` if your network has higher jitter.

