#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUDP.h>

#include "settings.h"

// ------------------------------------------------------------
// Constants
// ------------------------------------------------------------

constexpr uint16_t HTTP_PORT = 1337;
constexpr uint16_t UDP_SHUTDOWN_PORT = 20388;
constexpr uint16_t WOL_PORT = 9;

constexpr uint32_t DNS_UPDATE_INTERVAL = 3600000UL; // 1 hour

constexpr uint8_t WOL_PREAMBLE_SIZE = 6;
constexpr uint8_t MAC_SIZE = 6;
constexpr uint8_t WOL_REPEAT = 16;

constexpr int CMD_WAKE = 99;

// ------------------------------------------------------------
// Globals
// ------------------------------------------------------------

ESP8266WebServer server(HTTP_PORT);

MDNSResponder mdns;

WiFiUDP udp;
WiFiUDP udpShutdown;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* mac_addr = MAC_ADDRESS;
const char* wolPassword = WOL_PASS;

const char* duckDomain = DUCK_DOMAIN;
const char* duckToken = DUCK_TOKEN;

unsigned long lastDnsUpdate = 0;

// ------------------------------------------------------------
// Function declarations
// ------------------------------------------------------------

void beginWifi();
void reconnectWifi();

void updateDuckDNS();

void handleHome();
void handleCommand();
void handleNotFound();

void sendCommand(const IPAddress& ip, const byte* mac, int command);

bool macStringToBytes(const String& mac, byte* bytes);

byte valFromChar(char c);

inline void ledOn()
{
    digitalWrite(LED_BUILTIN, LOW);
}

inline void ledOff()
{
    digitalWrite(LED_BUILTIN, HIGH);
}

// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    ledOff();

    beginWifi();

    if (!mdns.begin("esp8266", WiFi.localIP()))
    {
        Serial.println(F("Failed to start mDNS"));
    }

    udp.begin(WOL_PORT);
    udpShutdown.begin(UDP_SHUTDOWN_PORT);

    server.on("/", handleHome);
    server.on("/command", handleCommand);
    server.onNotFound(handleNotFound);

    server.begin();

    Serial.println(F("HTTP server started"));
}

// ------------------------------------------------------------
// Loop
// ------------------------------------------------------------

void loop()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        reconnectWifi();
    }

    if (millis() - lastDnsUpdate >= DNS_UPDATE_INTERVAL)
    {
        lastDnsUpdate = millis();
        updateDuckDNS();
    }

    mdns.update();

    server.handleClient();
}

// ------------------------------------------------------------
// WiFi
// ------------------------------------------------------------

void beginWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.println();
    Serial.print(F("Connecting"));

    while (WiFi.status() != WL_CONNECTED)
    {
        ledOn();
        delay(200);
        ledOff();
        delay(200);

        Serial.print('.');
    }

    Serial.println();
    Serial.print(F("Connected to "));
    Serial.println(ssid);

    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());

    updateDuckDNS();
}

void reconnectWifi()
{
    Serial.println(F("WiFi disconnected"));

    WiFi.disconnect();
    WiFi.begin(ssid, password);

    unsigned long start = millis();

    while (WiFi.status() != WL_CONNECTED &&
        millis() - start < 10000)
    {
        delay(250);
        Serial.print('.');
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.println(F("Reconnected"));

        if (!mdns.begin("esp8266", WiFi.localIP()))
        {
            Serial.println(F("Failed to restart mDNS"));
        }

        updateDuckDNS();
    }
    else
    {
        Serial.println();
        Serial.println(F("Reconnect failed. Restarting..."));
        ESP.restart();
    }
}

// ------------------------------------------------------------
// DuckDNS
// ------------------------------------------------------------

void updateDuckDNS()
{
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    char url[256];

    snprintf(
        url,
        sizeof(url),
        "https://www.duckdns.org/update?domains=%s&token=%s&ip=",
        duckDomain,
        duckToken);

    http.begin(client, url);

    int code = http.GET();

    if (code > 0)
    {
        Serial.print(F("DuckDNS: "));
        Serial.println(http.getString());
    }
    else
    {
        Serial.print(F("DuckDNS update failed: "));
        Serial.println(code);
    }

    http.end();
}
// ------------------------------------------------------------
// HTTP Handlers
// ------------------------------------------------------------

void handleHome()
{
    ledOn();

    IPAddress ip = WiFi.localIP();

    String page = HOME_PAGE;

    page.replace("{favicon}", FAVICON);
    page.replace("{ip1}", String(ip[0]));
    page.replace("{ip2}", String(ip[1]));
    page.replace("{ip3}", String(ip[2]));

    server.send(200, "text/html", page);

    ledOff();
}

void handleCommand()
{
    ledOn();

    if (!server.hasArg("mac") ||
        !server.hasArg("pwd") ||
        !server.hasArg("bcast") ||
        !server.hasArg("cmd"))
    {
        server.send(400, "text/plain", "Missing parameters");
        ledOff();
        return;
    }

    String mac = server.arg("mac");
    String password = server.arg("pwd");

    if (password != wolPassword)
    {
        server.send(403, "text/plain", "Invalid password");
        ledOff();
        return;
    }

    if (mac.length() != 12)
    {
        server.send(400, "text/plain", "Invalid MAC");
        ledOff();
        return;
    }

    byte targetMac[6];

    if (!macStringToBytes(mac, targetMac))
    {
        server.send(400, "text/plain", "Invalid MAC");
        ledOff();
        return;
    }

    int broadcast = server.arg("bcast").toInt();

    if (broadcast < 0 || broadcast > 255)
    {
        server.send(400, "text/plain", "Invalid broadcast");
        ledOff();
        return;
    }

    int command = server.arg("cmd").toInt();

    IPAddress targetIp = WiFi.localIP();
    targetIp[3] = broadcast;

    Serial.println();
    Serial.println(F("Sending command"));
    Serial.print(F("Target IP: "));
    Serial.println(targetIp);

    sendCommand(targetIp, targetMac, command);

    String response =
        "Command sent to " +
        targetIp.toString() +
        " (" +
        mac +
        ")";

    server.send(200, "text/plain", response);

    ledOff();
}

void handleNotFound()
{
    server.send(404, "text/plain", "");
}

// ------------------------------------------------------------
// MAC Helpers
// ------------------------------------------------------------

byte valFromChar(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    return 255;
}

bool macStringToBytes(const String& mac, byte* bytes)
{
    if (mac.length() != 12)
        return false;

    for (int i = 0; i < 6; i++)
    {
        byte high = valFromChar(mac[i * 2]);
        byte low = valFromChar(mac[i * 2 + 1]);

        if (high == 255 || low == 255)
            return false;

        bytes[i] = (high << 4) | low;
    }

    return true;
}

// ------------------------------------------------------------
// UDP Command Sender
// ------------------------------------------------------------

void sendCommand(const IPAddress& ip, const byte* mac, int command)
{
    ledOn();

    if (command == CMD_WAKE)
    {
        static const byte preamble[6] =
        {
            0xFF, 0xFF, 0xFF,
            0xFF, 0xFF, 0xFF
        };

        udp.beginPacket(ip, WOL_PORT);

        udp.write(preamble, sizeof(preamble));

        for (uint8_t i = 0; i < WOL_REPEAT; i++)
        {
            udp.write(mac, MAC_SIZE);
        }

        udp.endPacket();

        Serial.println(F("Wake-on-LAN packet sent"));
    }
    else
    {
        char json[96];

        snprintf(
            json,
            sizeof(json),
            "{\"MacAddress\":\"%s\",\"Command\":%d}",
            mac_addr,
            command);

        udpShutdown.beginPacket(ip, UDP_SHUTDOWN_PORT);
        udpShutdown.write((const uint8_t*)json, strlen(json));
        udpShutdown.endPacket();

        Serial.print(F("Shutdown packet: "));
        Serial.println(json);
    }

    ledOff();
}