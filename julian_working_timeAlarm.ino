#include <WiFi.h>
#include <esp_now.h>
#include <TM1637.h>
#include <Preferences.h>
#include "SoundData.h"
#include "XT_DAC_Audio.h"


// REPLACE WITH THE MAC Address of your receiver 
uint8_t allAddresses[3][6] = { {0x78, 0x21, 0x84, 0x9D, 0x40, 0xD0}, 
                          {0x78, 0x21, 0x84, 0x9C, 0xFA, 0x84},
                          {0x78, 0x21, 0x84, 0x9D, 0x0D, 0x7C}
                          };

uint8_t addressToSendTo[6];


// Replace with your network credentials
const char* ssid  = "Dingle Berry";

// Set web server port number to 80
WiFiServer server(80);

Preferences preferences;

// Variable to store the HTTP request
String header;

// Alarm Variables
bool alarmSet = false;
int alarmTime = 0;
int morning = 1;
int cT = 0;

int clockMorning = 1;

//Sync Time and Set Alarm then change modes

// Define variables to store incoming readings
bool incoming;
int incomingSecs;
bool incomingMorning;

bool transmitter = true;
bool gameStarted = false;
const int ledPin = 23;
int thisMacIndex = 0;
int buttonPin = 18;
bool buttonPressed;
bool currentButton = false;
int numESPSTest = 3;

int CLK = 22;
int DIO = 4;
TM1637 tm(CLK,DIO);
bool setTime = false;
bool settingTime = false;
int clockTime = 0;
int everySecond = 0;
int secondCounter = 0;

bool firstTime = false;

int wackyDiff = 0;

long diff = 0;

unsigned long buttonPressedAt = 0;
bool buttonReleased = false;
byte buttonPrevState = 0;

String success;

//Structure example to send data
//Must match the receiver structure
typedef struct struct_message {
    bool rc;
    int secondsW;
    bool morningW;
    int numAlarmPressedInfo;
} struct_message;


// Create a struct_message called BME280Readings to hold sensor readings
struct_message BME280Readings;

// Create a struct_message to hold incoming sensor readings
struct_message incomingReadings;

esp_now_peer_info_t peerInfo;

// audio alarm variables 
XT_Wav_Class Force(ForceYes);
XT_DAC_Audio_Class DacAudio(25,0);    
uint32_t DemoCounter=0; 

// stop alarm variables
int numAlarmPressed = 0; // current num times user pressed alarm
int alarmPressThreshold = 4; // max num times user presses alarm to turn off

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  
  if (status ==0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Fail :(";
  }
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  incoming = incomingReadings.rc;
  incomingSecs = incomingReadings.secondsW;
  incomingMorning = incomingReadings.morningW;
  numAlarmPressed = incomingReadings.numAlarmPressedInfo;

  Serial.println(incomingReadings.numAlarmPressedInfo);

  if (incomingSecs != 0)
  {
    displayTime(incomingSecs); 
    settingTime = false;
    setTime = true;
    clockTime = incomingSecs;
    clockMorning = incomingMorning;
  }
  Serial.println("Incoming!!");
  // might put delay here so background processing can happen?
}

void setup() 
{
  preferences.begin("clockStorage", false);
  Serial.begin(115200);
  tm.set(5);

  // audio alarm setup
  Force.RepeatForever = true;
  DacAudio.Play(&Force);

  if (preferences.getInt("clock", 0) != 0)
  {
    Serial.println("DATA FOUND IN PREFERENCES");
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA); 

    pinMode(ledPin, OUTPUT);

   thisMacIndex = findMacAddress();

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) 
    {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    // Register Peers
    for (int i = 0; i < numESPSTest; i++)
    {
      // skip registering peer if i = thisBoard
      if (i == thisMacIndex)
        continue;

      // register peer
      memcpy(peerInfo.peer_addr, allAddresses[i], 6); 
      peerInfo.channel = 0;  
      peerInfo.encrypt = false;

      // Add peer success      
      if (esp_now_add_peer(&peerInfo) != ESP_OK)
      {
        Serial.println("Failed to add peer");
        return;
      }    
    }

   // Register callback function that will be called when data is to be sent
    esp_now_register_send_cb(OnDataSent);
  
    // Register for a callback function that will be called when data is received
    esp_now_register_recv_cb(OnDataRecv);   
    displayTime(preferences.getInt("clock")); 
    settingTime = false;
    setTime = true;
    clockTime = preferences.getInt("clock");
    clockMorning = preferences.getInt("clockM");
    if (preferences.getInt("alarm", 0) != 0)
    {
      alarmTime = preferences.getInt("alarm");
      morning = preferences.getInt("alarmM");
      alarmSet = true;
    }

    BME280Readings.rc = false;
    BME280Readings.secondsW = clockTime;
    BME280Readings.morningW = clockMorning;
    startGame(); 


  }  
  else 
  {
    Serial.println("DATA NOT FOUND IN EEPROM");
    // Connect to Wi-Fi network with SSID and password
    Serial.print("Setting AP (Access Point)…");
    // Remove the password parameter, if you want the AP (Access Point) to be open
    WiFi.softAP(ssid);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    
    server.begin();
    displayTime(720);
  }
  
}

void loop()
{
  breakOutOfWebsite();  
  
  if (preferences.getInt("clock", 0) == 0)
  {
    WiFiClient client = server.available();   // Listen for incoming clients
    if (client) {                             // If a new client connects,
      Serial.println("New Client.");          // print a message out in the serial port
      String currentLine = "";                // make a String to hold incoming data from the client
      while (client.connected()) {            // loop while the client's connected
        if (client.available()) {             // if there's bytes to read from the client,
          char c = client.read();             // read a byte, then
          Serial.write(c);                    // print it out the serial monitor
          header += c;
          if (c == '\n') {                    // if the byte is a newline character
            // if the current line is blank, you got two newline characters in a row.
            // that's the end of the client HTTP request, so send a response:
            if (currentLine.length() == 0) {
              // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
              // and a content-type so the client knows what's coming, then a blank line:
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");
              client.println();
              
              for (int q = 0; q < header.length(); q++)
              {
                Serial.println(header[q]);
              }

              int a = header.indexOf(":");
              int alarmIndex = header.indexOf("m=")+2;
              int hourStopIndex = header.indexOf("%3");
              String alarmString = "";
              String s = "";
              if (a >= 0)
              {
                for (int j = a - 2; j < a+3; j++)
                {
                  if (header[j] != ':')
                  {
                    s += header[j];
                  }
                }   
                decypher(s);    
                if (header[a + 9] == 'a')
                {
                  clockMorning = 1;
                }       
                else
                {
                  clockMorning = 0;
                }
              }
        
              if (alarmIndex >= 0)
              {
                for (int u = alarmIndex; u < hourStopIndex; u++)
                {
                  alarmString += header[u];
                }
                alarmString += 'L';
                alarmString += header[hourStopIndex + 3];
                alarmString += header[hourStopIndex + 4];
                alarmString += header[hourStopIndex + 9];
                alarmString += header[hourStopIndex + 10];
                setAlarm(alarmString);
                alarmString = "";
              }

            if (header.indexOf(""))
              
              
              // Display the HTML web page
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              // CSS to style the on/off buttons 
              // Feel free to change the background-color and font-size attributes to fit your preferences
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
              client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #555555;}</style></head>");
              
              // Web Page Heading
              client.println("<body><h1>HTML HELL</h1>");
              client.println("<title>Document</title>");
              // setting time
              client.println("<h1> Time is <span id=\"time\"> </span></h1>");
              client.println("<div><p><a id=\"divMsg\" href=\"/28/on\"><button class=\"button\">SYNC TIME</button></a></p></div>");
              client.println("<style>#timeError {display: none;font-size: 0.8em;}#timeError.visible {display: block;}#buttonError {display: none;font-size: 0.8em;}#buttonError.visible {display: block;}input.invalid {border-color: red;}</style>");
              client.println("<form><label for=\"alarm\">Alarm Time: </label><br><input type=\"text\" id=\"alarm\" name=\"alarm\" placeholder=\"12:00\"><input type=\"radio\" id=\"AM\" name=\"MN\" value=\"AM\"><label for=\"AM\">AM</label><input type=\"radio\" id=\"PM\" name=\"MN\" value=\"PM\"><label for=\"PM\">PM</label><br><span role=\"alert\" id=\"timeError\" aria-hidden=\"true\">Please enter a proper time in this format: 12:00</span><span role=\"alert\" id=\"buttonError\" aria-hidden=\"true\">Please select AM or PM</span><input id = \"submit\" type=\"submit\" value=\"Submit\" onclick=\"return validate()\"></form><script>function validate() {const firstNameField = document.getElementById(\"alarm\");const amButton = document.getElementById(\"AM\");const pmButton = document.getElementById(\"PM\");let valid = true;let buttonValid = true;var findColon = firstNameField.value.indexOf(\":\");if (firstNameField.value.length > 5 || firstNameField.value.length < 4){valid = false;}else if (findColon < 0){valid = false;}else if (parseInt(firstNameField.value.slice(0,findColon)) > 12 || parseInt(firstNameField.value.slice(0,findColon)) < 1){valid = false;}else if (parseInt(firstNameField.value.slice(findColon + 1)) > 59 || parseInt(firstNameField.value.slice(findColon + 1)) < 0){valid = false;} if (!amButton.checked && !pmButton.checked){buttonValid = false;}if (!valid) {const nameError = document.getElementById(\"timeError\");nameError.classList.add(\"visible\");firstNameField.classList.add(\"invalid\");nameError.setAttribute(\"aria-hidden\", false);nameError.setAttribute(\"aria-invalid\", true);}else{const nameErro = document.getElementById(\"timeError\");nameErro.classList.remove(\"visible\");firstNameField.classList.remove(\"invalid\");nameErro.setAttribute(\"aria-hidden\", true);nameErro.style = null;}if(!buttonValid) {const nameErr = document.getElementById(\"buttonError\");nameErr.classList.add(\"visible\");firstNameField.classList.add(\"invalid\");nameErr.setAttribute(\"aria-hidden\", false);nameErr.setAttribute(\"aria-invalid\", true);}else{const E = document.getElementById(\"buttonError\");E.classList.remove(\"visible\");firstNameField.classList.remove(\"invalid\");E.setAttribute(\"aria-hidden\", true);E.style = null;}return valid;}</script>");
              client.println("<script>var apenuts; function refreshTime() { const timeDisplay = document.getElementById(\"time\"); const dateString = new Date().toLocaleString();const formattedString = dateString.replace(\", \", \" - \");timeDisplay.textContent = formattedString; apenuts = formattedString;} const a = document.getElementById('divMsg').href=new Date().toLocaleString(); setInterval(refreshTime, 1000);</script>");
              if (alarmSet)
              {
                client.println("<h1>");
                client.println(alarmTime);
                client.println(" Alarm</h1>");
                if (morning == 1)
                {
                  client.println("<h1>");
                  client.println("Alarm is AM</h1>");
                }
                else
                {
                  client.println("<h1>");
                  client.println("Alarm is PM</h1>");
                }
              }
              client.println("<h1>");
              client.println(cT);
              client.println(" Time</h1>");


    
              client.println("</body></html>");
              
              // The HTTP response ends with another blank line
              client.println();
              // Break out of the while loop
              break;
            } else { // if you got a newline, then clear currentLine
              currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
        breakOutOfWebsite();
      }
      // Clear the header variable
      header = "";
      // Close the connection
      client.stop();
      Serial.println("Client disconnected.");
      Serial.println("");
    }
  }
  else
  {
    BME280Readings.rc = false;
  
    // if (!gameStarted)
    // {
    //   // digitalWrite(ledPin, HIGH);
      
    //   if ( digitalRead(buttonPin) == HIGH)
    //   {
    //     while (digitalRead(buttonPin) == HIGH) {}
    //     // digitalWrite(ledPin, LOW); 

    //     // gameStarted = true; // set local game start
    //     // BME280Readings.rc = true;
    //     // startGame(); // send ping to tell others game started
    //     // delay(1000); // give time for others to process
    //     // sendPingToRandomDevice();
    //   }
      
    // }

    if (incoming)
    {
      // others are telling us game started
      if (!gameStarted)
      {
        Serial.println("I know game has started");
        digitalWrite(ledPin, LOW);
        gameStarted = true;
        incoming = false;
        return;
      }
      
      Serial.println("In game: Received Ping");
      digitalWrite(ledPin, HIGH);

      while (true)
      {
        // play our alarm
        DacAudio.FillBuffer();      

        // prevent overflow of audio when playing continuously
        if (DemoCounter >= 980000) // change variable to match audio
        {
          DemoCounter = 0;
        } 

        // stop alarm and game
        if (numAlarmPressed > alarmPressThreshold)
        { 
          // tell others that the game has stopped
          BME280Readings.rc = true;
          BME280Readings.numAlarmPressedInfo = numAlarmPressed;
          startGame(); // send ping to everyone saying that the game is over

          // locally set game has stopped
          gameStarted = false;
          numAlarmPressed = 0;
          DemoCounter = 0;
          incoming = false;
          digitalWrite(ledPin, LOW);
          return;
        }

        if ( digitalRead(buttonPin) == HIGH )
        {

          while (digitalRead(buttonPin) == HIGH ) {}
          
          numAlarmPressed++;
          
          // data to send
          digitalWrite(ledPin, LOW);
          BME280Readings.rc = true;
          BME280Readings.morningW = false;
          BME280Readings.secondsW = 0; 
          BME280Readings.numAlarmPressedInfo = numAlarmPressed;

          sendPingToRandomDevice();
          incoming = false;
          return;
        }
      }
      // reset audio for next time
      DemoCounter = 0;

    }
  }

  everySecond = millis();

  if (settingTime)
  {
    // Blink through each space on the clock to change the value
    Serial.println("Setting Time");
  }
  else
  {
    if (!setTime)
    {
      // Blink the clock

    }
    else
    {
      if (firstTime)
      {
        wackyDiff = millis();
        firstTime = false;
      }
      // Increment the time
      // might have to be less precise
      if (everySecond - wackyDiff > 1000)
      {
        secondCounter++;
        wackyDiff += 1000;
        if (secondCounter % 60 == 0 && secondCounter != 0)
        {
          secondCounter = 0;
          clockTime++;
          displayTime(clockTime);
          if (alarmSet && (clockTime == alarmTime) && (clockMorning == morning))
          {
            //ALARMMMMMMMMM WEEEE WOOOOO
            Serial.println("ALARM IS GOING OFF");
            digitalWrite(ledPin, LOW); 
            gameStarted = true; // set local game start
            BME280Readings.rc = true;
            BME280Readings.morningW = false;
            BME280Readings.secondsW = 0;   
            startGame(); // send ping to tell others game started
            delay(1000); // give time for others to process
            sendPingToRandomDevice();
          }
          if (clockTime == 720)
          {
            if (clockMorning)
            {
              clockMorning = false;
            }
            else
            {
              clockMorning = true;
            }
          }
          if (clockTime == 780)
          {
            clockTime = 60;
            displayTime(clockTime);
          }
        }
      }
    }  
  }
 
} // end of loop


void breakOutOfWebsite()
{
  if ( digitalRead(buttonPin) == HIGH)
      {
        Serial.println("Button being pressed");
        diff = millis();
        while (digitalRead(buttonPin) == HIGH ) {}
        if (millis() - diff > 5000)
        {
          digitalWrite(ledPin, HIGH);
          diff = 0;
          if (preferences.getInt("clock", 0) == 0)
          {
            //Store the current time and alarm then reset 
            preferences.putInt("clock", clockTime);
            preferences.putInt("clockM", clockMorning);
            if (alarmSet)
            {
              preferences.putInt("alarm", alarmTime);
              preferences.putInt("alarmM", morning);      
            }
            ESP.restart();
          }
          else
          {
            //set the EEPROM to 0 then reset
            Serial.println("RESETTING EEPROM");
            preferences.putInt("alarm", 0);
            preferences.putInt("alarmM", 0);   
            preferences.putInt("clock", 0);
            preferences.putInt("clockM", 0);
            ESP.restart();
          }
        }
      }
}

void sendPingToRandomDevice()
{
  int randomAddress = random(numESPSTest);
  BME280Readings.morningW = false;
  BME280Readings.secondsW = 0;   
  
  while (randomAddress == thisMacIndex){randomAddress = random(numESPSTest);}
  esp_err_t result = esp_now_send(allAddresses[randomAddress], (uint8_t *) &BME280Readings, sizeof(BME280Readings));
  Serial.println("address to send to");
  Serial.println(randomAddress);
}

// ping once to others to tell them the game started
void startGame()
{
  for (int i = 0; i < numESPSTest; i++)
  {
    if (thisMacIndex == i)
    {
      continue;
    }
    Serial.println("Contacted ");
    Serial.println(i);

    esp_err_t result = esp_now_send(allAddresses[i], (uint8_t *) &BME280Readings, sizeof(BME280Readings));
  }
}


int findMacAddress()
{
  String macAddress = WiFi.macAddress();
  int sLength = macAddress.length();

  // first digit
  char lastHexMacAddressFirstDigit = macAddress[sLength-2];
  int lastHexMacAddressFirstDigitInt = convertCharToHEX(lastHexMacAddressFirstDigit);
  
  // last digit
  char lastHexMacAddressSecondDigit = macAddress[sLength-1];
  int lastHexMacAddressSecondDigitInt = convertCharToHEX(lastHexMacAddressSecondDigit);
  
  // convert Hex value to decimal
  int thisEsp_TotalHexValue = (lastHexMacAddressFirstDigitInt * 16) + (lastHexMacAddressSecondDigitInt);
  Serial.println(thisEsp_TotalHexValue);

  // find our address in list of addresses
  for (int i = 0; i < 3; i++)
  {
    // returns decimal value of last hex from current address we're viewing in 2D array
    int currentLoopAddress = allAddresses[i][5]; // decimal value from hex
    
    if (currentLoopAddress == thisEsp_TotalHexValue)
    {
      Serial.print("Home MAC FOUND!!!");
      return i;
    }
  }
 
  Serial.print("Bad Comparison; Home MAC not found");
  return 0;  
}

int convertCharToHEX(char inputChar)
{

    switch (inputChar) 
    {
      case 'A':
        return 10;
        
      case 'B':
        return 11;
        
      case 'C':
        return 12;
        
      case 'D':
        return 13;
        
      case 'E':
        return 14;
        
      case 'F':
        return 15;
        
      default:
        return inputChar - '0'; // will return int value for char
    }
}

void displayTime(int seconds){
    int minutes = seconds / 60;
    int second = seconds % 60;

    tm.point(1);
    tm.display(3, second % 10);
    tm.display(2, second / 10 % 10);
    tm.display(1, minutes % 10);
    tm.display(0, minutes / 10 % 10);
}

void decypher(String z)
{
  int q;
  String pq = "";
  pq += z[0];
  pq += z[1];
  q = pq.toInt();
  int m;
  String pp = "";
  pp += z[2];
  pp += z[3];
  m = pp.toInt();
  settingTime = false;
  setTime = true;
  displayTime(q*60+m);
  cT = q*60+m;
  clockTime = cT;
}

void setAlarm(String badName)
{
  Serial.println(badName + " AAAAAAAAA");
  int q;
  String pq = "";
  for (int i = 0; i < badName.indexOf("L"); i++)
  {
    pq += badName[i];
  }
  q = pq.toInt();
  pq = "";
  int p;
  pq += badName[badName.indexOf("L") + 1];
  pq += badName[badName.indexOf("L") + 2];
  p = pq.toInt();
  if (badName.indexOf("AM") >= 0)
  {
    morning = 1;
  }
  else
  {
    morning = 0;
  }
  alarmTime = q*60 + p;
  alarmSet = true;
}