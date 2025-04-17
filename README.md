ESP8266 Wake-on-LAN Server
A small ESP8266-based server that supports Wake-on-LAN (WoL). It can also send shutdown and restart commands when used in conjunction with the Wolow server.

📡 Features
Wake up a PC using a GET request

Optional shutdown and restart commands (requires Wolow)

🔗 API Endpoints
🖥 Wake PC
bash
Copy
Edit
GET https://[YOUR_SERVER]:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=99
⚠ Requires Wolow for the following:
⏻ Shutdown PC
bash
Copy
Edit
GET https://[YOUR_SERVER]:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=0
🔄 Restart PC
bash
Copy
Edit
GET https://[YOUR_SERVER]:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=1
🔧 Parameters
mac – The MAC address of the target PC

bcast – The broadcast IP, usually 255

pwd – Your predefined password

cmd – Command to execute:

99 – Wake PC

0 – Shutdown PC (requires Wolow)

1 – Restart PC (requires Wolow)
