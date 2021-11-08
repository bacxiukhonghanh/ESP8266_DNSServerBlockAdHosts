// Based on: https://github.com/esp8266/Arduino/blob/master/libraries/DNSServer/src/DNSServer.cpp

#include "MyDNSServer.h"
#include <lwip/def.h>
#include <Arduino.h>
#include <memory>
#include <ESP8266WiFi.h>
#include "DomainsList.h"

#ifdef DEBUG_ESP_PORT
#define DEBUG_OUTPUT DEBUG_ESP_PORT
#else
#define DEBUG_OUTPUT Serial
#endif

#define DNS_HEADER_SIZE sizeof(DNSHeader)
#define led_output 2

DomainsList domainsList;

MyDNSServer::MyDNSServer()
{
  _ttl = lwip_htonl(60);
  _errorReplyCode = DNSReplyCode::ServerFailure;
}

// This will blink the LED indicating that the server received a DNS request,
// but it'll be executed only after I've sent the DNS response to the client
void BlinkLEDOnRequests() {
  digitalWrite(led_output, LOW);  // ON
  delay(50);
  digitalWrite(led_output, HIGH); // OFF
}

bool MyDNSServer::start(const uint16_t &port, const IPAddress &IPForDetectedHosts)
{
  _port = port;
  
  Serial.println("Pre-process domains list...");
  for (int i = 0; i < sizeof(domainsList.domains)/sizeof(domainsList.domains[0]); i++){
    if (domainsList.domains[i] != "") downcaseAndRemoveWwwPrefix(domainsList.domains[i]);
  }
  Serial.println("Done.");
  
  _IPForDetectedHosts[0] = IPForDetectedHosts[0];
  _IPForDetectedHosts[1] = IPForDetectedHosts[1];
  _IPForDetectedHosts[2] = IPForDetectedHosts[2];
  _IPForDetectedHosts[3] = IPForDetectedHosts[3];
  return _udp.begin(_port) == 1;
}

void MyDNSServer::setErrorReplyCode(const DNSReplyCode &replyCode)
{
  _errorReplyCode = replyCode;
}

void MyDNSServer::setTTL(const uint32_t &ttl)
{
  _ttl = lwip_htonl(ttl);
}

void MyDNSServer::stop()
{
  _udp.stop();
}

void MyDNSServer::downcaseAndRemoveWwwPrefix(String &domainName)
{
  domainName.toLowerCase();
  /* for special purposes, I don't think we need to remove the www. part yet */
  //if (domainName.startsWith("www."))
  //    domainName.remove(0, 4);
}

void MyDNSServer::respondToRequest(uint8_t *buffer, size_t length)
{
  DNSHeader *dnsHeader;
  uint8_t *query, *start;
  size_t remaining, labelLength, queryLength;
  uint16_t qtype, qclass;

  dnsHeader = (DNSHeader *)buffer;

  // Must be a query for us to do anything with it
  if (dnsHeader->QR != DNS_QR_QUERY)
    return;

  // If operation is anything other than query, we don't do it
  if (dnsHeader->OPCode != DNS_OPCODE_QUERY)
    return replyWithError(dnsHeader, DNSReplyCode::NotImplemented);

  // Only support requests containing single queries - everything else
  // is badly defined
  if (dnsHeader->QDCount != lwip_htons(1))
    return replyWithError(dnsHeader, DNSReplyCode::FormError);

  // We must return a FormError in the case of a non-zero ARCount to
  // be minimally compatible with EDNS resolvers
  if (dnsHeader->ANCount != 0 || dnsHeader->NSCount != 0
      || dnsHeader->ARCount != 0)
    return replyWithError(dnsHeader, DNSReplyCode::FormError);

  // Even if we're not going to use the query, we need to parse it
  // so we can check the address type that's being queried

  query = start = buffer + DNS_HEADER_SIZE;
  remaining = length - DNS_HEADER_SIZE;
  while (remaining != 0 && *start != 0) {
    labelLength = *start;
    if (labelLength + 1 > remaining)
      return replyWithError(dnsHeader, DNSReplyCode::FormError);
    remaining -= (labelLength + 1);
    start += (labelLength + 1);
  }

  // 1 octet labelLength, 2 octet qtype, 2 octet qclass
  if (remaining < 5)
    return replyWithError(dnsHeader, DNSReplyCode::FormError);

  start += 1; // Skip the 0 length label that we found above

  memcpy(&qtype, start, sizeof(qtype));
  start += 2;
  memcpy(&qclass, start, sizeof(qclass));
  start += 2;

  queryLength = start - query;

  if (qclass != lwip_htons(DNS_QCLASS_ANY)
      && qclass != lwip_htons(DNS_QCLASS_IN))
    return replyWithError(dnsHeader, DNSReplyCode::NonExistentDomain,
        query, queryLength);

  if (qtype != lwip_htons(DNS_QTYPE_A)
      && qtype != lwip_htons(DNS_QTYPE_ANY))
    return replyWithError(dnsHeader, DNSReplyCode::NonExistentDomain,
        query, queryLength);

  // If we have no domain name configured, just return an error
  //if (domainsList[0].isEmpty())
  if (sizeof(domainsList.domains) == 0)
    return replyWithError(dnsHeader, _errorReplyCode,
        query, queryLength);

  /*
   * Maybe I've not needed this yet.
  */
  // If we're running with a wildcard we can just return a result now
  //if (domainsList.domains[0] == "*" && (sizeof(domainsList.domains) / sizeof(domainsList.domains[0]) == 1) )
  //  return replyWithIP(dnsHeader, query, queryLength);

  /*
   * This "for" block saved me:
   * for (int i = 0; i < q.length(); i++)
   * {
   *   Serial.print((int)q[i], DEC);
   *   Serial.print(" ");
   * }
   * Once I got the String form of the query data via String((char*)query), calling the
   * above "for" block would help me see the ASCII code of each character in the string.
   * For example, initially the String form I got was bacxiukhonghanhnet
   * Where those squares were from could be seen in the output after calling the above "for" block.
   * 15 98 97 99 120 105 117 107 104 111 110 103 104 97 110 104 3 110 101 116
   * The first one was 15, and "bacxiukhonghanh" consisted of 15 chars.
   * So after that I saw the number 3, and I thought it was the length of "net".
   * Therefore I managed to figure out that each portion of the domain name came after a character
   * whose ASCII code told the length of that portion.
   * For the code, I had to declare an index of the character in the String form while I was looping
   * through each of its chars. First, I would take the length of the first portion of the domain
   * name and set that as the current portion's length. As I looped through the String form, if the
   * index were still lower than or equal the current portion's length, I would concatenate every
   * character I met. But if the index were greater than the current portion's length, which means
   * I was already out of the first portion, then I would have to take the char at that index as the
   * length of the next portion, and make that portion the current one I was working at.
   * Things would go on the same way till the index was equal the length of the whole String form.
   * (just to remind myself, the index of a string can only go from 0 to string.length() - 1).
  */
  String queryUnprocessedString = String((char*)query);
  int queryTextLength = queryUnprocessedString.length();
  String queryProcessedString = "";

  int domainPortionLength = (int)queryUnprocessedString[0];
  int queryCharIndex = 1;
  while (queryCharIndex < queryTextLength) {
    if (queryCharIndex > domainPortionLength) {
      domainPortionLength = queryCharIndex + (int)queryUnprocessedString[queryCharIndex];
      queryProcessedString += ".";
    }
    else {
      queryProcessedString += queryUnprocessedString[queryCharIndex];
    }
    queryCharIndex++;
  }

  /*
   * Things might be simpler for me now, as I just have to do less comparison
   * between the processed query data and the entries in the list of domains.
  */
  
  for (int i = 0; i < sizeof(domainsList.domains)/sizeof(domainsList.domains[0]); i++) {
    String matchString = domainsList.domains[i];
    if (matchString != "") {
      /* 
       *  I'm going to check whether it matches the whole entry and whether it is a subdomain
       *  of the entry. If one of the conditions is met, I will return an invalid IP.
      */
      if (queryProcessedString == matchString || queryProcessedString.endsWith("." + matchString)) {
        Serial.println("DETECTED: " + queryProcessedString + " -> " + domainsList.domains[i]);
        replyWithIP(dnsHeader, query, queryLength, _IPForDetectedHosts);
        return BlinkLEDOnRequests();
      }
    }
  }

  /*
   * The domain was not found in the blacklist. I will return the ServerFailure code so that
   * the client will come to the 2ndary DNS server for the real IP address. 
  */
  Serial.println(queryProcessedString + " was not found. Replying so that the client comes to the 2ndary server.");
  replyWithError(dnsHeader, _errorReplyCode, query, queryLength);
  return BlinkLEDOnRequests();
}

void MyDNSServer::processNextRequest()
{
  size_t currentPacketSize;

  currentPacketSize = _udp.parsePacket();
  if (currentPacketSize == 0)
    return;

  // The DNS RFC requires that DNS packets be less than 512 bytes in size,
  // so just discard them if they are larger
  if (currentPacketSize > MAX_DNS_PACKETSIZE)
    return;

  // If the packet size is smaller than the DNS header, then someone is
  // messing with us
  if (currentPacketSize < DNS_HEADER_SIZE)
    return;

  std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[currentPacketSize]);
  if (buffer == nullptr)
    return;

  _udp.read(buffer.get(), currentPacketSize);
  respondToRequest(buffer.get(), currentPacketSize);
}

void MyDNSServer::writeNBOShort(uint16_t value)
{
   _udp.write((unsigned char *)&value, 2);
}

void MyDNSServer::replyWithIP(DNSHeader *dnsHeader,
          unsigned char * query,
          size_t queryLength, unsigned char responseIP[])
{
  uint16_t value;

  dnsHeader->QR = DNS_QR_RESPONSE;
  dnsHeader->QDCount = lwip_htons(1);
  dnsHeader->ANCount = lwip_htons(1);
  dnsHeader->NSCount = 0;
  dnsHeader->ARCount = 0;

  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write((unsigned char *) dnsHeader, sizeof(DNSHeader));
  _udp.write(query, queryLength);

  // Rather than restate the name here, we use a pointer to the name contained
  // in the query section. Pointers have the top two bits set.
  value = 0xC000 | DNS_HEADER_SIZE;
  writeNBOShort(lwip_htons(value));

  // Answer is type A (an IPv4 address)
  writeNBOShort(lwip_htons(DNS_QTYPE_A));

  // Answer is in the Internet Class
  writeNBOShort(lwip_htons(DNS_QCLASS_IN));

  // Output TTL (already NBO)
  _udp.write((unsigned char*)&_ttl, 4);

  // Length of RData is 4 bytes (because, in this case, RData is IPv4)
  writeNBOShort(lwip_htons(sizeof(responseIP)));
  _udp.write(responseIP, sizeof(responseIP));
  _udp.endPacket();
}

void MyDNSServer::replyWithError(DNSHeader *dnsHeader,
             DNSReplyCode rcode,
             unsigned char *query,
             size_t queryLength)
{
  dnsHeader->QR = DNS_QR_RESPONSE;
  dnsHeader->RCode = (unsigned char) rcode;
  if (query)
     dnsHeader->QDCount = lwip_htons(1);
  else
     dnsHeader->QDCount = 0;
  dnsHeader->ANCount = 0;
  dnsHeader->NSCount = 0;
  dnsHeader->ARCount = 0;

  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write((unsigned char *)dnsHeader, sizeof(DNSHeader));
  if (query != NULL)
     _udp.write(query, queryLength);
  _udp.endPacket();
}

void MyDNSServer::replyWithError(DNSHeader *dnsHeader,
             DNSReplyCode rcode)
{
  replyWithError(dnsHeader, rcode, NULL, 0);
}

/*
 * Other examples that saved me when I ran the "for" block above on them
 * googlecom
 * 6 103 111 111 103 108 101 3 99 111 109
 * webfacebookcom
 * 3 119 101 98 8 102 97 99 101 98 111 111 107 3 99 111 109
*/
