# ESP8266 Wake-on-LAN Server

An ESP8266 HTTP server that sends Wake-on-LAN packets and, with the Wolow Companion, shutdown or restart commands.

## API

`POST /command` on port `1337`.

Every request must include this header:

```
Authorization: Bearer <API_BEARER_TOKEN>
```

The endpoint accepts an `application/json` request body:

| Property | Description |
| --- | --- |
| `mac` | Target MAC address as 12 hex characters, for example `AABBCCDDEEFF` |
| `bcast` | Last octet of the local broadcast address, normally `255` |
| `cmd` | `99` wake, `0` shutdown, or `1` restart |

Example wake request:

```powershell
curl.exe -X POST "http://192.168.1.50:1337/command" `
  -H "Authorization: Bearer my-secret-token" `
  -H "Content-Type: application/json" `
  -d "{\"mac\":\"AABBCCDDEEFF\",\"bcast\":255,\"cmd\":99}"
```

Missing or invalid authorization returns `401 Unauthorized`. Malformed JSON or invalid fields return `400 Bad Request`. A non-JSON request returns `415 Unsupported Media Type`.

## Audit log

The sketch writes every command outcome to Serial in a consistent format and retains the newest configured number of events in a RAM-backed ring buffer (32 by default). This avoids the latency, rate limits, and dependency on an external request for every command. The buffer is cleared when the ESP8266 restarts and does not write to flash.

The optional `GET /logs` endpoint is disabled by default. Set `ENABLE_AUDIT_LOG_ENDPOINT` to `1` in `Sketch1/settings.h` to enable it; it returns the audit entries as JSON and requires the same Bearer token as `/command`. Log entries contain request outcomes, source IP addresses, User-Agent values, target MAC addresses, and commands—but never the Bearer token. Set `AUDIT_LOG_CAPACITY` to change the buffer size.

## Configuration

Set Wi-Fi details, the Bearer token, MAC address, DuckDNS domain, and DuckDNS token in `Sketch1/settings.h`. Set `AUDIT_LOG_CAPACITY` to choose the in-memory audit-log size, and set `ENABLE_AUDIT_LOG_ENDPOINT` to `1` only when you want to expose the authenticated `GET /logs` endpoint.

## Requirements

- ESP8266 board, such as a NodeMCU or Wemos D1 Mini
- Arduino ESP8266 core
- [ArduinoJson](https://arduinojson.org/) installed through the Arduino Library Manager
- Optional: Wolow Companion on the target PC for shutdown and restart

## Remote access

If using port forwarding, forward a chosen external TCP port to port `1337` on the ESP8266. Bearer authentication protects the command endpoint, but the service is plain HTTP; use a VPN or TLS-capable reverse proxy for internet exposure where possible.
