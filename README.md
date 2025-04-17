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
- (Optional) Wolow server running on the target PC for shutdown/restart support

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
- Enable Wake-on-LAN in your PC’s [BIOS Settings](https://youtu.be/7rnpV8onpjM?si=Z1_Jr_2Q9qIaoAOS) and [Network Adapter Settings](https://youtu.be/4-zlIAyy10k?si=i68x27b43tMmLcFk)
- Wolow must be running for shutdown and restart commands to work.

---

## 🌍 Accessing from Outside Your Network (Port Forwarding)

To control your ESP8266 remotely over the internet, you'll need to set up **port forwarding** on your router:

### 1. Assign a Static IP to the ESP8266
- Log into your router.
- Find **DHCP settings** or **connected devices**.
- Reserve a static IP for your ESP8266's MAC address (e.g., `192.168.1.100`).

### 2. Set Up Port Forwarding
- Go to your router’s **Port Forwarding** section.
- Forward **external port 8080** to your ESP8266’s local IP on **internal port 8080**.
  - Protocol: **TCP**
  - External Port: `8080`
  - Internal IP: `192.168.1.100` (replace with your ESP8266 IP)
  - Internal Port: `8080`

### 3. Get Your Public IP Address
- Visit [https://whatismyipaddress.com](https://whatismyipaddress.com)
- Note your public IP (e.g., `123.45.67.89`)

### 4. Make a Remote Request
You can now access your ESP8266 from anywhere using:

```
http://123.45.67.89:8080/command?mac=AABBCCDDEEFF&bcast=255&pwd=mysecret&cmd=99

---

## 📎 Resources

- [ESP8266 Documentation](https://arduino-esp8266.readthedocs.io/)
- [Wolow Official Site](https://wolow.site/#wolow-companion)

---

## 📄 License

This project is open-source and available under the MIT License.
