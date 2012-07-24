/*
 * WiFlyHQ Example httpclient.ino
 *
 * This sketch implements a simple Web client that connects to a 
 * web server, sends a GET, and then sends the result to the 
 * Serial monitor.
 *
 * This sketch is released to the public domain.
 *
 */

#include <WiFlyHQ.h>

#include <SoftwareSerial.h>
SoftwareSerial wifiSerial(4,5);

#include <Wire.h>
#include <Adafruit_NFCShield_I2C.h>

#define IRQ   (2)
#define RESET (3)  // Not connected by default on the NFC Shield

#define PIN_TEMP 1
#define aref_voltage 3.3

#define PIN_VALVE  8
#define PIN_LED_RED 9
#define PIN_LED_YELLOW 10
#define PIN_LED_GREEN 11
#define PIN_BUTTON 12

#define POUR_DURATION 30000


WiFly wifly;
Adafruit_NFCShield_I2C nfc(IRQ, RESET);


/* Change these to match your WiFi network */
const char mySSID[] = "SSID";
const char myPassword[] = "password";

const char site[] = "beer.machadolab.com";

void terminal();

void setup()
{
    char buf[32];
    
  Serial.begin(115200);
  analogReference(EXTERNAL);
  
  pinMode(PIN_VALVE, OUTPUT);
  digitalWrite(PIN_VALVE, LOW);
  
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_RED, HIGH);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  digitalWrite(PIN_LED_YELLOW, HIGH);
  pinMode(PIN_LED_GREEN, OUTPUT);
  digitalWrite(PIN_LED_GREEN, HIGH);
  
  pinMode(PIN_BUTTON, INPUT);
  
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN53x board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  digitalWrite(PIN_LED_GREEN, LOW);

    Serial.println("Starting");
    Serial.print("Free memory: ");
    Serial.println(wifly.getFreeMemory(),DEC);

    wifiSerial.begin(9600);
    if (!wifly.begin(&wifiSerial, &Serial)) {
        Serial.println("Failed to start wifly");
	terminal();
    }

    /* Join wifi network if not already associated */
    if (!wifly.isAssociated()) {
	/* Setup the WiFly to connect to a wifi network */
	Serial.println("Joining network");
	wifly.setSSID(mySSID);
	wifly.setPassphrase(myPassword);
	wifly.enableDHCP();

	if (wifly.join()) {
	    Serial.println("Joined wifi network");
	} else {
	    Serial.println("Failed to join wifi network");
	    terminal();
	}
    } else {
        Serial.println("Already joined network");
    }

    wifly.setDeviceID("Wifly-WebClient");
    
      // configure board to read RFID tags
    nfc.SAMConfig();
}

void loop()
{

  boolean success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  
  Serial.println("Ready for RFID requests");

  digitalWrite(PIN_LED_RED, HIGH);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);
  
  
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength);
  
  if (success) {
    Serial.println("Found a card!");
    Serial.print("UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
    Serial.print("UID Value: ");
    char cardId[16] = "";
    for (uint8_t i=0; i < uidLength; i++) 
    {
      Serial.print(" 0x");Serial.print(uid[i], HEX);
      sprintf(cardId, "%s%x", cardId, uid[i]);
    }
    Serial.println("");
    Serial.print("Resulting card ID Str: ");
    Serial.println(cardId);
    // Wait 1 second before continuing
    delay(1000);
    
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_LED_YELLOW, HIGH);
    
    if (authorizePour(cardId))
    {
     
      digitalWrite(PIN_LED_YELLOW, LOW);
      digitalWrite(PIN_LED_GREEN, HIGH);
      
      Serial.println("Auth successful - waiting for button");
      boolean pourDone = false;
      while (!pourDone)
      {
        if (digitalRead(PIN_BUTTON) == HIGH)
        {
          unsigned long nowMillis = millis();
          digitalWrite(PIN_LED_RED, HIGH);
          digitalWrite(PIN_LED_YELLOW, HIGH);
          digitalWrite(PIN_LED_GREEN, HIGH);
	  Serial.println("Opening valve");
          digitalWrite(PIN_VALVE, HIGH);
          delay(POUR_DURATION);
	  Serial.println("Closing valve");
          digitalWrite(PIN_VALVE, LOW);
          pourDone = true;
        }
      }
    }
    else
    {
      
      digitalWrite(PIN_LED_YELLOW, LOW);
      digitalWrite(PIN_LED_RED, HIGH);
      
      Serial.println("Auth failed - no beer for you");
    }
    
  }
    
}

/* Connect the WiFly serial to the serial monitor. */
void terminal()
{
   Serial.println("Terminal started:");
    while (1) {
	if (wifly.available() > 0) {
	    Serial.write(wifly.read());
	}


	if (Serial.available() > 0) {
	    wifly.write(Serial.read());
	}
    }
}


boolean authorizePour(char *id)
{
 
  float temperature = readTemperature();
  Serial.print("Temperature is ");
  Serial.println(temperature);
  char buf[48];
  snprintf(buf, 47, "/access.php?a=auth&id=%s&t=%d", id, int(temperature));
  int result = webRequest(site, buf);
  Serial.print("Got web result ");
  Serial.println(result);
  
  if (result >= 200 && result <= 299)
    return true;
  else
    return false;
}


int webRequest(const char *host, char *url)
{
  
    if (wifly.isConnected()) {
        Serial.println("Old connection active. Closing");
	wifly.close();
    }

    if (wifly.open(host, 80)) {
        Serial.print("Connected to ");
	Serial.println(site);

	/* Send the request */
	wifly.print("GET ");
        wifly.print(url);
        wifly.println(" HTTP/1.1");
	wifly.print("Host: ");
        wifly.println(site);
	wifly.println();
         
        char prevChar = 0;
        char curChar = 0;
        char respCode[4] = "99";
        boolean inCode = false;
        boolean haveCode = false;
        int i = 0;
        
        while(!haveCode) 
        {
          if (wifly.available() > 0)
          {
            prevChar = curChar;
            curChar = wifly.read();
            if (curChar == ' ')
            {
              if (inCode)
              {
                inCode = false;
                haveCode = true;
                respCode[i] = '\0';
              }
              else
              {
                inCode = true;
              }
            }
            else if (inCode)
            {
                if (i <= 3)
                  respCode[i++] = curChar; 
            }
            else if (curChar == '\r')
            {
               haveCode = true;
            }
          }
        }
        wifly.close();
        return atoi(respCode);
    } else {
        Serial.println("Failed to connect");
        return 1;
    }
    return 2; 
}


float readTemperature()
{
  
  int tempReading = analogRead(PIN_TEMP);
  Serial.print("tempReading - ");
  Serial.println(tempReading);

  // converting that reading to voltage, which is based off the reference voltage
  float voltage = tempReading * aref_voltage;
  voltage /= 1024.0; 
 
  // now print out the temperature
  float temperatureC = (voltage - 0.5) * 100 ;  //converting from 10 mv per degree wit 500 mV offset
                                               //to degrees ((volatge - 500mV) times 100) 
  // now convert to Fahrenheight
  float temperatureF = (temperatureC * 9.0 / 5.0) + 32.0;

  return temperatureF;  
}
