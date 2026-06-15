# InfoDashboard

InfoDashboard is an ESP-IDF and LVGL information dashboard for the Waveshare ESP32-S3-RLCD-4.2 board. It targets the board's 4.2 inch monochrome reflective LCD in landscape `400 x 300` mode and keeps the implementation on ESP-IDF, not Arduino.

This repository is a public mirror exported from a private development repository. Private deployment history, local project memory, Wi-Fi credentials, API tokens, and site-specific endpoint values are intentionally not included.

## Current Shape

- Firmware: ESP-IDF + LVGL 9 under `firmware/`.
- Display: high-contrast 1 bpp dashboard rendered for the RLCD framebuffer.
- Local board data: time, battery state, and SHTC3 indoor temperature/humidity.
- Remote data: optional Codex quota, ServerMonitor status, and Open-Meteo weather endpoints configured locally.
- Offline behavior: the firmware can show local/config/no-data states without using mock business data on hardware.

## Repository Layout

```text
firmware/       ESP-IDF application, board code, UI, preview, screenshot tools
.env.example   Optional local environment variable template
LICENSE        Apache-2.0 license
```

Local files such as `.env`, `firmware/sdkconfig`, `firmware/dependencies.lock`, build directories, logs, and generated previews are intentionally ignored.

## Build

Install ESP-IDF for ESP32-S3, then build from `firmware/`:

```powershell
Push-Location .\firmware
idf.py set-target esp32s3
idf.py build
```

Flash after confirming the correct serial port for your board:

```powershell
idf.py -p COMx flash monitor
```

## Configuration

Configure runtime values with `idf.py menuconfig` or a local `firmware/sdkconfig` that is not committed:

```text
Info Dashboard Transport
  WiFi SSID: your 2.4 GHz network
  WiFi password: set locally
  Dashboard API latest URL: https://example.com/codex-quota/api/v1/latest
  Dashboard API key: set locally when required
  Open-Meteo current weather URL: Open-Meteo forecast URL for your location
  Weather display location: short display name
  ServerMonitor status URL: https://example.com/server-monitor/api/v1/targets/default/status
  ServerMonitor local status URL 1: optional LAN endpoint
  ServerMonitor local status URL 2: optional LAN endpoint
  ServerMonitor API token: set locally when required
  ServerMonitor display target: short target label
  Cloudflare Access client ID: optional
  Cloudflare Access client secret: optional
```

The ESP32 sends `X-API-Key` for the Codex quota endpoint and `Authorization: Bearer <token>` for ServerMonitor when configured. If both Cloudflare Access values are set, public Codex and ServerMonitor requests also include the Cloudflare Access service-token headers.

Do not commit Wi-Fi passwords, API keys, tokens, Cloudflare secrets, local endpoint URLs, or generated `sdkconfig` files.

## UI Preview

The desktop preview reads the same layout and generated bitmap fonts used by the firmware:

```powershell
python .\firmware\tools\preview_dashboard.py
```

Open:

```text
http://localhost:8787
```

Export preview images:

```powershell
python .\firmware\tools\preview_dashboard.py --export-svg .\preview.svg
python .\firmware\tools\preview_dashboard.py --export-png .\preview.png
```

## Device Screenshot

When enabled, the firmware exposes a read-only PBM screenshot endpoint for the final 1 bpp RLCD framebuffer:

```text
GET http://<esp32-ip>:8080/screenshot.pbm
```

Fetch locally:

```powershell
python .\firmware\tools\fetch_screenshot.py <esp32-ip> -o screen.pbm
python .\firmware\tools\fetch_screenshot.py <esp32-ip> -o screen.png
```

PNG output requires Pillow; PBM output works without it.

## Data Boundaries

- Indoor temperature and humidity come from the board-local SHTC3 sensor over I2C.
- Weather comes from Open-Meteo or another configured public weather endpoint.
- ServerMonitor and Codex quota data are optional external services configured by the device owner.
- OAuth sessions, refresh tokens, collector internals, and long-lived service credentials must stay on server-side services, not in this firmware repository.
- Hardware firmware should show real API data, real cached data, board-local sensor data, or explicit empty/config/no-data states. Mock data belongs only in desktop previews or tests.
