#include <Arduino.h>
#include <ArduinoJson.h>
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

struct AuditLogEntry
{
    uint32_t timestampMs;
    String message;
};

static_assert(AUDIT_LOG_CAPACITY > 0 && AUDIT_LOG_CAPACITY <= 255,
    "AUDIT_LOG_CAPACITY must be between 1 and 255");

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
const char* bearerToken = API_BEARER_TOKEN;

const char* wolDomain = WOL_DOMAIN;
const char* duckToken = DUCK_TOKEN;

const char* requestHeaders[] =
{
    "Authorization",
    "Content-Type",
    "User-Agent"
};

unsigned long lastDnsUpdate = 0;

AuditLogEntry auditLog[AUDIT_LOG_CAPACITY];
uint8_t auditLogNext = 0;
uint8_t auditLogCount = 0;

// ------------------------------------------------------------
// Function declarations
// ------------------------------------------------------------

void beginWifi();
void reconnectWifi();

void updateDuckDNS();

void handleHome();
void handleCommand();
void handleLogs();
void handleNotFound();

void sendCommand(const IPAddress& ip, const byte* mac, int command);

bool isAuthorized();
void appendLog(const String& message);
String jsonEscape(const String& value);
String requestContext();

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

    server.collectHeaders(requestHeaders,
        sizeof(requestHeaders) / sizeof(requestHeaders[0]));

    server.on("/", HTTP_GET, handleHome);
    server.on("/command", HTTP_POST, handleCommand);
#if ENABLE_AUDIT_LOG_ENDPOINT
    server.on("/logs", HTTP_GET, handleLogs);
#endif
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
        wolDomain,
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

    if (!isAuthorized())
    {
        appendLog("command denied: invalid authorization" + requestContext());
        server.sendHeader("WWW-Authenticate", "Bearer");
        server.send(401, "text/plain", "Unauthorized");
        ledOff();
        return;
    }

    String contentType = server.header("Content-Type");
    contentType.toLowerCase();

    if (!contentType.startsWith("application/json"))
    {
        appendLog("command rejected: content type is not application/json" + requestContext());
        server.send(415, "text/plain", "Content-Type must be application/json");
        ledOff();
        return;
    }

    StaticJsonDocument<256> request;
    DeserializationError error = deserializeJson(request, server.arg("plain"));

    if (error)
    {
        appendLog("command rejected: invalid JSON" + requestContext());
        server.send(400, "text/plain", "Invalid JSON");
        ledOff();
        return;
    }

    if (!request["mac"].is<const char*>() ||
        !request["bcast"].is<int>() ||
        !request["cmd"].is<int>())
    {
        appendLog("command rejected: missing or invalid parameters" + requestContext());
        server.send(400, "text/plain", "Missing or invalid parameters");
        ledOff();
        return;
    }

    String mac = request["mac"].as<String>();
    if (mac.length() != 12)
    {
        appendLog("command rejected: invalid MAC length" + requestContext());
        server.send(400, "text/plain", "Invalid MAC");
        ledOff();
        return;
    }

    byte targetMac[6];

    if (!macStringToBytes(mac, targetMac))
    {
        appendLog("command rejected: invalid MAC format" + requestContext());
        server.send(400, "text/plain", "Invalid MAC");
        ledOff();
        return;
    }

    int broadcast = request["bcast"].as<int>();

    if (broadcast < 0 || broadcast > 255)
    {
        appendLog("command rejected: invalid broadcast" + requestContext());
        server.send(400, "text/plain", "Invalid broadcast");
        ledOff();
        return;
    }

    int command = request["cmd"].as<int>();

    IPAddress targetIp = WiFi.localIP();
    targetIp[3] = broadcast;

    Serial.println();
    Serial.println(F("Sending command"));
    Serial.print(F("Target IP: "));
    Serial.println(targetIp);

    sendCommand(targetIp, targetMac, command);

    appendLog(
        "command sent:" + requestContext() +
        " target=" + targetIp.toString() +
        " mac=" + mac +
        " cmd=" + String(command));

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

void handleLogs()
{
    if (!isAuthorized())
    {
        server.sendHeader("WWW-Authenticate", "Bearer");
        server.send(401, "text/plain", "Unauthorized");
        return;
    }

    String response;
    response.reserve(16 + auditLogCount * 200);
    response = "{\"entries\":[";
    const uint8_t first = auditLogCount == AUDIT_LOG_CAPACITY ? auditLogNext : 0;

    for (uint8_t i = 0; i < auditLogCount; ++i)
    {
        const AuditLogEntry& entry = auditLog[(first + i) % AUDIT_LOG_CAPACITY];

        if (i > 0)
            response += ',';

        response += "{\"timestamp_ms\":" + String(entry.timestampMs) +
            ",\"message\":\"" + jsonEscape(entry.message) + "\"}";
    }

    response += "]}";
    server.send(200, "application/json", response);
}

bool isAuthorized()
{
    if (!server.hasHeader("Authorization"))
        return false;

    return server.header("Authorization") == String("Bearer ") + bearerToken;
}

void appendLog(const String& message)
{
    const uint32_t timestamp = millis();

    auditLog[auditLogNext] = { timestamp, message };
    auditLogNext = (auditLogNext + 1) % AUDIT_LOG_CAPACITY;
    if (auditLogCount < AUDIT_LOG_CAPACITY)
        ++auditLogCount;

    Serial.printf_P(PSTR("[%lu ms] AUDIT %s\n"), timestamp, message.c_str());
}

String jsonEscape(const String& value)
{
    String escaped;
    escaped.reserve(value.length() + 8);

    for (size_t i = 0; i < value.length(); ++i)
    {
        switch (value[i])
        {
        case '\\': escaped += F("\\\\"); break;
        case '\"': escaped += F("\\\""); break;
        case '\n': escaped += F("\\n"); break;
        case '\r': escaped += F("\\r"); break;
        case '\t': escaped += F("\\t"); break;
        default:
            if (static_cast<uint8_t>(value[i]) < 0x20)
            {
                char controlCharacter[7];
                snprintf(controlCharacter, sizeof(controlCharacter),
                    "\\u%04X", static_cast<uint8_t>(value[i]));
                escaped += controlCharacter;
            }
            else
            {
                escaped += value[i];
            }
            break;
        }
    }

    return escaped;
}

String requestContext()
{
    String userAgent = server.header("User-Agent");
    userAgent.replace('\r', ' ');
    userAgent.replace('\n', ' ');

    constexpr size_t MAX_USER_AGENT_LENGTH = 120;
    if (userAgent.length() > MAX_USER_AGENT_LENGTH)
        userAgent.remove(MAX_USER_AGENT_LENGTH);

    if (userAgent.length() == 0)
        userAgent = "unknown";

    return
        " remote=" + server.client().remoteIP().toString() +
        " user-agent=\"" + userAgent + "\"";
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
