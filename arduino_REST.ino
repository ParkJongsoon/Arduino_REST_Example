#include <HttpClient.h>
#include "SoftwareSerial.h"
#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>

SoftwareSerial esp(3, 2); // RX, TX
String data; 
String jsonData = "";

/*서버 관련 변수 */
String ssid = "WIFINAME";
String password = "WIFIPASSWORD";
String server = "SERVER ADDRESS";
String uri = "SERVER_URI"; //server side script to call

/*측정 관련 변수 */
#define ONE_WIRE_BUS 5 /*-2핀에 온도센서 연결-*/
#define SensorPin A0            //pH meter Analog output to Arduino Analog Input 0
#define Offset 0.00            //deviation compensate
#define samplingInterval 20
#define printInterval 800
#define ArrayLenth  40    //times of collection
int pHArray[ArrayLenth];   //Store the average value of the sensor feedback
int pHArrayIndex=0; 

OneWire ourWire(ONE_WIRE_BUS);
DallasTemperature sensors(&ourWire);

//reset the esp8266 module
void reset() {
  esp.println("AT+RST");
  delay(3000);

  if (esp.find("OK"))
  {
    Serial.println("Module Reset");
  }
  else
  {
    Serial.println("Cannot Module Reset");
    reset(); 
  }
}

//connect to your wifi network
void connectWifi() {  
  String cmd = "AT+CWJAP=\"" + ssid + "\",\"" + password + "\"";
  Serial.println(cmd);
  esp.println(cmd);
  delay(4000);

  if (esp.find("OK")) {
    Serial.println("Connected!");
  }
  else {
    Serial.println("Cannot connect to wifi");
    connectWifi();    
  }
}

void setup() {
  esp.begin(115200); //start SoftSerial for ESP comms
  Serial.begin(115200); //start regular serial for PC -> Arduino comms

  Serial.print("Module Status : ");
  reset(); //call the ESP reset function
  Serial.print("Module Status : ");
  connectWifi(); //call the ESP connection function
  sensors.begin();
  
}

void loop() 
{ 
  httppost(); 
  delay(1000);
}

float measure_temp(){
  float temper;
  sensors.requestTemperatures();
  Serial.print("Temperature : ");

  Serial.print(sensors.getTempCByIndex(0));
  Serial.println("C");
  temper =  sensors.getTempCByIndex(0);
  delay(1000);
  return temper;
}

float measure_ph(){
  float ph;
  static unsigned long samplingTime = millis();
  static unsigned long printTime = millis();
  static float pHValue,voltage;
  if(millis()-samplingTime > samplingInterval){
      pHArray[pHArrayIndex++]=analogRead(SensorPin);
      if(pHArrayIndex==ArrayLenth)pHArrayIndex=0;
      voltage = avergearray(pHArray, ArrayLenth)*5.0/1024;
      pHValue = 3.5*voltage+Offset;
      samplingTime=millis();
  }
  if(millis() - printTime > printInterval){ //Every 800 milliseconds, print a numerical, convert the state of the LED indicator
    Serial.print("Voltage:");
        Serial.print(voltage,2);
        Serial.print("    pH value: ");
    Serial.println(pHValue,2);
//        digitalWrite(LED,digitalRead(LED)^1);
        printTime=millis();
  } 
  ph = pHValue; 
  return ph;
}

double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    Serial.println("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }
    }
    avg = (double)amount/(number-2);
  }
  return avg;
}

void httppost() 
{
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();    
  root["temp"] = measure_temp();
  root["ph"] = measure_ph();
  root.printTo(jsonData);
  Serial.print("JsonData is : ");
  Serial.println(jsonData); 

  esp.println("AT+CIPSTART=\"TCP\",\"" + server + "\",3000"); //start a TCP connection.

  if (esp.find("OK")) 
  {
    Serial.println("TCP connection ready");
  }
  else
  {
    Serial.println("TCP connection not ready");
  } 
  delay(1000);

  String postRequest = String("POST ");
  postRequest = postRequest + uri + " HTTP/1.1\r\n" +
  "Host: " + server + "\r\n" +
  "Accept: *" + "/" + "*\r\n" +
  "Content-Length: " + data.length() + "\r\n" +
  "Content-Type: application/x-www-form-urlencoded\r\n" +
  "\r\n" + jsonData;

  Serial.println("Post Request text: "); //see what the fully built POST request looks like
  Serial.println(postRequest);

  String sendCmd = "AT+CIPSEND="; //determine the number of caracters to be sent.
  esp.print(sendCmd);
  esp.println(postRequest.length());

  Serial.println("Sending..");
  esp.print(postRequest);

  Serial.println("Data to send: ");
  Serial.println(data);
  Serial.print("Data length: ");
  Serial.println(data.length());
  Serial.print("Request length: ");
  Serial.println(postRequest.length());
  Serial.println("Request Sent:");
  Serial.println(postRequest);

  if( esp.find("SEND OK")) 
  {
    Serial.println("Packet sent");
    while (esp.available()) 
    {
      String tmpResp = esp.readString();
      Serial.println(tmpResp);     
    }

    // close the connection
    esp.println("AT+CIPCLOSE");
    Serial.println("Close the Connection");
  }
  delay(1000);
}


//  

