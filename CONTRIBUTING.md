# Contributing

This repository is a public mirror of a private hardware project. Direct development happens in the private source repository, then a sanitized tree is exported here.

## How To Contribute

- Open an issue for bugs, build problems, documentation gaps, or portability notes.
- Keep reports focused on public code and reproducible behavior.
- Do not include Wi-Fi passwords, API keys, bearer tokens, Cloudflare Access secrets, private endpoint URLs, local IP maps, or screenshots that expose credentials.
- If you have a code patch, describe the change in an issue first. Patches need to be applied to the private source repository before they can appear in this mirror.

## Development Notes

- Firmware lives under `firmware/`.
- The project targets ESP-IDF and LVGL for the Waveshare ESP32-S3-RLCD-4.2 board.
- Local generated files such as `firmware/sdkconfig`, build outputs, logs, and preview captures should stay untracked.
