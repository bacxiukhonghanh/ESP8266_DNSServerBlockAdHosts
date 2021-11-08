// Based on: https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi/examples/WiFiClientStaticIP/WiFiClientStaticIP.ino
// and: https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/examples/DNSServer/DNSServer.ino

// Load libraries
#include "MyDNSServer.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define led_output 2

// Replace with your network credentials
const char* ssid     = "YOUR_SSID_HERE";
const char* passphrase = "YOUR_PASSPHRASE_HERE";

const byte DNS_PORT = 53;
MyDNSServer dnsServer;
IPAddress IPAddressForConnectionPrevention(127, 0, 0, 1);
ESP8266WebServer webServer(80);

// Set your Static IP address, i.e. 192.168.85.133
IPAddress local_IP(192, 168, 85, 133);
// Set your Gateway IP address, i.e. 192.168.85.1
IPAddress gateway(192, 168, 85, 1);
// Set your Subnet Mask, i.e. 255.255.255.0
IPAddress subnet(255, 255, 255, 0);

/* 
 * As this one acts as a DNS proxy, it will have to rely on the 
 * external DNS servers in the Internet so that it can look up
 * the domains that aren't in its blacklist and return to the
 * client. At least one primary DNS server address configuration
 * has to be set.
*/
IPAddress primaryDNS(192, 168, 85, 1);  // you can also take the gateway as the DNS server, I'm using the example above.
IPAddress secondaryDNS(8, 8, 8, 8);     // specify a backup one

/*
 * This function will test the external DNS connection at the
 * beginning.
*/
void printIPAddressOfHost(const char* host) {
  IPAddress resolvedIP;
  if (!WiFi.hostByName(host, resolvedIP)) {
    Serial.println("DNS lookup failed.");
    Serial.println(host);
  }
  else {
    Serial.println(host);
    Serial.println("IP: ");
    Serial.println(resolvedIP);
  }
}

void setup() {
  Serial.begin(115200);
  // Initialize the output variables as outputs
  pinMode(led_output, OUTPUT);
  // Initially set outputs to HIGH (LED off)
  digitalWrite(led_output, HIGH);

  // Configures static IP address
  if (WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA configured successfully");
  }
  else {
    Serial.println("STA Failed to configure");
  }
  
  // Connect to Wi-Fi network with SSID and passphrase
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, passphrase);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // modify TTL associated  with the domain name (in seconds)
  // default is 60 seconds
  dnsServer.setTTL(300);
  // set which return code will be used for all other domains (e.g. sending
  // ServerFailure instead of NonExistentDomain will reduce number of queries
  // sent by clients)
  // default is DNSReplyCode::NonExistentDomain
  /* 
   * Actually I'll send ServerFailure so that the client can come to its
   * 2ndary DNS server, otherwise the client will be unable to access
   * the rest of the web because its browser will stop at NXDOMAIN error.
   */
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);

  // start DNS server
  dnsServer.start(DNS_PORT, IPAddressForConnectionPrevention);

  // simple HTTP server to see that DNS server is working
  webServer.onNotFound([]() {
    String message = "Yes, speaking.\n\n";
    webServer.send(200, "text/plain", message);
  });
  webServer.begin();

  /*
   * An example that initially tests the external DNS connection.
   * Change it to what you want if necessary, just make sure it does exist.
  */
  Serial.println("Testing external DNS connection...");
  printIPAddressOfHost("ptnk.edu.vn");
}

void loop(){
  dnsServer.processNextRequest();
  webServer.handleClient();
}
