/*
  Web Server

 A simple web server that shows the value of the analog input pins.
 using a SweetPea UnoNet+. This example also demonstrates how to use
 the EspM24Sr library to obtain network information and how to re-
 configure the ethernet controller.

 Circuit:
 * The SweetPea UnoNet uses pins 10, 11, 12, 13 for communication with
   the Ethernet controller.
 * Analog inputs attached to pins A0 through A3 (optional) A4 and A5 are
   used for I2C on the UnoNet boards.

 created 18 Dec 2009 by David A. Mellis
 modified 9 Apr 2012 by Tom Igoe
 modified 10 Feb 2015 by Pontus Oldberg

 NOTE !
   This sketch will only run using the SweetPea modified Ethernet library.
 */

#include <inttypes.h>
#include <SPI.h>
#include <Wire.h>
#include <Ethernet.h>
#include <EspM24sr.h>
#include <avr/wdt.h>

// This length is dependent on the URI as defined in the android app
// If you change this in your version of the app this number also need
// to be changed. The number here works with the SweetPea demo app.
#define ANDROID_URI_LENGTH    50

// Array for holding the mac address
byte mac[6];

// IP addresses used by the system
IPAddress ip;
IPAddress netmask;
IPAddress gateway;
uint16_t port;

// This is our Ethernet Server instance.
EthernetServer server;

// This is the data structure in the NFC Tag EEPROM
struct target_board_data {
  uint8_t board_type;
  uint8_t dhcp_enabled;
  uint8_t ip[4];
  uint8_t netmask[4];
  uint8_t gateway[4];
  uint16_t port;
};

// The UnoNet+ has pin 3 assigned as its GPO pin
// Make sure the JP4 bridge has been shorted on the PCB.
#define GPO_PIN    3

// Instances.
EspM24SR m24sr;
struct cc_file_layout *ccfl;
struct system_file *sf;
struct target_board_data *board_data;

void printMacAddress(byte *mac)
{
  byte cnt = 0;
  while (cnt < 6) {
    if (mac[cnt] < 0x10) Serial.write('0');
    Serial.print(mac[cnt], HEX);
    if (cnt < 5) Serial.write(':');
    cnt++;
  }
}

/*
 * This interrupt will be triggered each time a phone or tablet
 * writes new data to the NFC EEPROM.
 */
void nfcWrite(void)
{
  // The NFC Tag has issued an interrupt. We will simply reset 
  // using the watchdog to restart the system. After restart the
  // system will be start with the new values.
  wdt_enable(WDTO_1S);
  while(1);
}

void setup() {
  size_t length;
  
  // First disable watchdog, in case it was enabled above.
  wdt_disable();
  
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
#if defined(__AVR_ATmega32U4__)
while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
#endif
  // Initialize the ms24sr library and notify what pin should be used
  // for signaling.
  m24sr.begin(GPO_PIN);
  // Set GPO mode 0x20, this mode will trigger the GPO pin each time new
  // data has been written to the memory by the NFC controller
  m24sr.writeGPO(0x20);
  
  // Interrupt 1 is connected to the GPO pin from the NFC Tag device.
  attachInterrupt(1, nfcWrite, RISING);

  // Get the CC and system file from the device.
  // It us mandatory to get these before retrieving the ndef record
  ccfl = m24sr.getCCFile();
  sf = m24sr.getSystemFile();
  
  // Use the UID from the NFC memory to create a mac address
  // The UID is 7 bytes long and we are using the 6 lowest bytes to
  // create the mac address. This will ensure that every device in a
  // local network has a unique mac address.
  memcpy(mac, sf->uid+1, 6);
  Serial.print(F("MAC Address: "));
  printMacAddress(mac);
  Serial.println();
  
  // Now get the NDEF record with ndef data
  char *m24sr_ndef = (char *)m24sr.getNdefMessage(&length);
  // Our board data is located in the ndef record offsetted by the URI length
  board_data = (struct target_board_data *)(m24sr_ndef + ANDROID_URI_LENGTH);

  // Copy NFC Tag EEPROM content to our local variables
  ip = board_data->ip;
  netmask = board_data->netmask;
  gateway = board_data->gateway;
  server.setPort(board_data->port);
  
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip, netmask, gateway);
  // Start server
  server.begin();
  
  Serial.print("WEB server is at ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 1");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // output the value of each analog input pin (Only 4 channels on UnoNet+)
          for (int analogChannel = 0; analogChannel < 4; analogChannel++) {
            int sensorReading = analogRead(analogChannel);
            client.print("analog input ");
            client.print(analogChannel);
            client.print(" is ");
            client.print(sensorReading);
            client.println("<br />");
          }
          client.println("</html>");
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}

