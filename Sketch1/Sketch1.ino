#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Arduino.h>
#include <SPI.h>
#include <WiFiUDP.h>
#include "settings.h"

MDNSResponder mdns;
WiFiUDP udp;
WiFiUDP udpShutdown;
int udpShutdownPort = 20388;
ESP8266WebServer server(1337);
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
String pwd = WOL_PASS;

IPAddress broadcastIp(192, 168, 0, 255);

void sendCommand(const IPAddress ip, const byte mac[], int command);
void beginWifi();
void macStringToBytes(const String mac, byte* bytes);

void setup(void) {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // turn on led
    beginWifi();
    while (!mdns.begin("esp8266", WiFi.localIP())) {}
    udp.begin(9);
    udpShutdown.begin(udpShutdownPort);

    // will display a form that, once submitted, sends a GET to /command
    server.on("/", []() {
        digitalWrite(LED_BUILTIN, LOW);
        IPAddress target_ip;
        target_ip = WiFi.localIP();
        String html_home_page = HOME_PAGE;
        html_home_page.replace("{favicon}", FAVICON);
        html_home_page.replace("{ip1}", String(target_ip[0]));
        html_home_page.replace("{ip2}", String(target_ip[1]));
        html_home_page.replace("{ip3}", String(target_ip[2]));
        server.send(200, "text/html", html_home_page);
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);
     });

    server.on("/command", []()
        {
            digitalWrite(LED_BUILTIN, LOW);

            if (server.arg("mac").length() <= 12 && server.arg("pwd").length() <= 200 && server.arg("bcast").length() <= 3) {
                String mac = server.arg("mac");
                String p = server.arg("pwd");
                int bcast = server.arg("bcast").toInt();
                int command = server.arg("cmd").toInt();

                if (p == pwd) {
                    IPAddress target_ip;
                    target_ip = WiFi.localIP();
                    target_ip[3] = bcast;
                    Serial.println("Sending Shutdown");
                    Serial.println(target_ip);

                    byte target_mac[6];
                    macStringToBytes(mac, target_mac);

                    sendCommand(target_ip, target_mac, command);
                    server.send(200, "text/plain", "Shutdown sent to " + target_ip.toString() + " " + mac);
                }
                else {
                    server.send(403, "text/plain", "Invalid password");
                }
            }
            else {
                server.send(403, "text/plain", "Invalid data");
            }
            delay(1000);
            digitalWrite(LED_BUILTIN, HIGH);
    });

    server.onNotFound([]() {
        digitalWrite(LED_BUILTIN, LOW);
        server.send(404, "text/plain", "");
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);
    });
    server.begin();
    Serial.println("HTTP server started");
}

void loop(void) {
    if (WiFi.status() != WL_CONNECTED)
    {
        ESP.reset();
    }
    server.handleClient();
}


void beginWifi() {
    WiFi.begin(ssid, password);
    Serial.println("");

    while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(250);
        Serial.print(".");
        digitalWrite(LED_BUILTIN, HIGH);
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}


void sendCommand(const IPAddress ip, const byte mac[], int command) {
    digitalWrite(LED_BUILTIN, LOW);
    udpShutdown.beginPacket(ip, udpShutdownPort);

    /*
    * Send a Wake-On-LAN packet for the given MAC address, to the given IP
    * address. Often the IP address will be the local broadcast.
    */
    if (command == 99)
    {
        byte preamble[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        udp.beginPacket(ip, 9);
        udp.write(preamble, 6);
        for (uint8 i = 0; i < 16; i++) {
            udp.write(mac, 6);
        }
        udp.endPacket();
    }
    else
    {
        String json = String("{\"MacAddress\":\"" + MAC_ADDRESS + "\",\"Command\":") + command + "}";
        udpShutdown.write(json.c_str());
        udpShutdown.endPacket();
    }

    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
}


byte valFromChar(char c) {
    if (c >= 'a' && c <= 'f') return ((byte)(c - 'a') + 10) & 0x0F;
    if (c >= 'A' && c <= 'F') return ((byte)(c - 'A') + 10) & 0x0F;
    if (c >= '0' && c <= '9') return ((byte)(c - '0')) & 0x0F;
    return 0;
}

void macStringToBytes(const String mac, byte* bytes) {
    if (mac.length() >= 12) {
        for (int i = 0; i < 6; i++) {
            bytes[i] = (valFromChar(mac.charAt(i * 2)) << 4) | valFromChar(mac.charAt(i * 2 + 1));
        }
    }
    else {
        Serial.println("Incorrect MAC format.");
    }
}