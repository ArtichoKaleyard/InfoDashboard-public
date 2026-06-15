# Security Policy

## Reporting

Please report security issues by opening a minimal GitHub issue that avoids private data. Do not paste credentials, tokens, Wi-Fi passwords, Cloudflare Access secrets, private endpoint URLs, or full device logs containing secrets.

If a report needs sensitive details, first open an issue asking for a private contact path.

## Supported Scope

This public mirror covers the firmware and public documentation in this repository. Private deployment infrastructure, private collector services, local network topology, and credentials are outside the public support scope.

## Credential Handling

The firmware is designed to receive Wi-Fi credentials and service tokens through local ESP-IDF configuration. These values must remain in local `sdkconfig`, environment files, or deployment tooling and must not be committed.
