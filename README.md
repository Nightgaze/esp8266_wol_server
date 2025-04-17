A small ESP8266 server for Wake on Lan, can also send shutdown/restart commands for Wollow.

Get requests:

PC Wake
https://server:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=99

The following commands require Wollow server installation (https://wolow.site/)
PC Shutdown:

https://server:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=0

PC Restart:

https://server:8080/command?mac=[REPLACE_WITH_MAC]&bcast=255&pwd=[REPLACE_WITH_PASSWORD]&cmd=1

