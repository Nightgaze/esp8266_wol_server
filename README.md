# ESP8266 Wake-on-LAN Server

A small ESP8266-based server that supports **Wake-on-LAN (WoL)**. It can also send **shutdown** and **restart** commands when used in conjunction with the [Wolow](https://wolow.site/) server.

---

## 📡 Features

- Wake up a PC using a GET request
- Optional shutdown and restart commands (requires Wolow)

---

## 🔗 API Endpoints

### 🖥 Wake PC

`GET https://[YOUR_SERVER]:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=99`

### ⚠ Requires [Wolow](https://wolow.site/) for the following:

#### ⏻ Shutdown PC

`GET https://[YOUR_SERVER]:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=0`

#### 🔄 Restart PC

`GET https://[YOUR_SERVER]:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=1`

---

## 🔧 Parameters

| Parameter | Description |
|----------|-------------|
| `mac`    | The MAC address of the target PC |
| `bcast`  | The broadcast IP, usually `255` |
| `pwd`    | Your predefined password |
| `cmd`    | Command to execute:<br>• `99` – Wake PC<br>• `0` – Shutdown PC (requires Wolow)<br>• `1` – Restart PC (requires Wolow) |

---

## 🔌 Requirements

- ESP8266 microcontroller (e.g., NodeMCU, Wemos D1 Mini)
- Compatible firmware uploaded to handle HTTP requests
- (Optional) Wolow Companion running on the target PC for shutdown/restart support

---

## 🛠 Example Usage

To **wake** a PC with MAC address `AA:BB:CC:DD:EE:FF`:

```
https://my-esp8266.local:8080/command?mac=AABBCCDDEEFF&bcast=255&pwd=mysecret&cmd=99
```

To **shutdown** that same PC (with Wolow):

```
https://my-esp8266.local:8080/command?mac=AABBCCDDEEFF&bcast=255&pwd=mysecret&cmd=0
```

To **restart** it:

```
https://my-esp8266.local:8080/command?mac=AABBCCDDEEFF&bcast=255&pwd=mysecret&cmd=1
```

---

## 🧠 Notes

- Ensure the ESP8266 is on the same local network as the target PC.
- Enable Wake-on-LAN in your PC’s BIOS and network adapter settings.
- Wolow must be running for shutdown and restart commands to work.

---

## 📎 Resources

- [ESP8266 Documentation](https://arduino-esp8266.readthedocs.io/)
- [Wolow Official Site](https://wolow.site/#wolow-companion)

---

## 📄 License

This project is open-source and available under the MIT License.
