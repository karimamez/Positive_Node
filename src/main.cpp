#include <Arduino.h>
#include "LoRaWan_APP.h"

#include <OneWire.h>
#include <DallasTemperature.h>


#include <AHT10.h>
#include <Wire.h>

#include "lw_base64.h"

#define timetosleep 1000
#define timetowake  1198000                         ////1000ms  20min 1200S


#define RF_FREQUENCY                                868000000 // Hz

#define TX_OUTPUT_POWER                             20        // dBm

#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
int16_t txnumber;
#define BUFFER_SIZE                                 250 // Define the payload size here
static RadioEvents_t RadioEvents;
char txpacket[BUFFER_SIZE];
uint8_t DevEUI[8] = {0xc2, 0x61, 0x93, 0xcc, 0xe7, 0xde, 0xdb, 0x0e };


static TimerEvent_t sleep;
static TimerEvent_t wakeup;
uint8_t lowpower=1;
String type = "HTCC-AB01";

byte Net_ID=0x06;
byte MSG_ID=0;
String MSG;
String ID="NP";
char endPayload='\n';
char space=' ';

uint8_t readStatus = 0;
AHT10 myAHT10(AHT10_ADDRESS_0X38);

void OnSleep();
void OnWakeup();
void Send ( String Message );
int Sum ( String Message );
int Request_ByteID ( int t );
int Normalize ( int P );



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  boardInitMcu();
  Radio.Sleep( );
  TimerInit( &sleep, OnSleep );
  TimerInit( &wakeup, OnWakeup );
  OnSleep();
}

void loop() {
  if(lowpower == 1){
      lowPowerHandler();
  }
  // put your main code here, to run repeatedly:
}

void OnTxDone(void){
  Radio.Sleep();
}

void OnTxTimeout(void){
  Radio.Sleep();
}

void OnSleep()
{
  Serial.printf("into lowpower mode, %d ms later wake up.\r\n",timetowake);
  pinMode(GPIO6, OUTPUT);
  digitalWrite(GPIO6,LOW);  
  OneWire  oneWire(GPIO5);
  DallasTemperature sensors(&oneWire);
  delay(100);
  sensors.begin();
  sensors.requestTemperatures();
  myAHT10.begin();
  float Temperature_DS18B20 = sensors.getTempCByIndex(0);
  float Battery = getBatteryVoltage();
  float Temperature_AHT10 = myAHT10.readTemperature();
  float Humidity_AHT10 = myAHT10.readHumidity();
  
  Serial.print("Temperature AHT10= ");
  Serial.print(Temperature_AHT10);
  Serial.println("°C");
  Serial.print("Humidity AHT10= ");
  Serial.print(Humidity_AHT10);
  Serial.println(" %");
  Serial.println();
  
  MSG = ID + space + String(Temperature_DS18B20,2) + space + String(Temperature_AHT10,2) + space + String(Battery/1000,2) + space + String(Humidity_AHT10,2) + '\n';
    
    
  Serial.println(MSG);
  digitalWrite(GPIO6, HIGH);
  Send(MSG);
  lowpower=1;
  
  //timetosleep ms later wake up;
  TimerSetValue( &wakeup, timetowake );
  TimerStart( &wakeup );
}
void OnWakeup()
{
  Serial.printf("wake up, %d ms later into lowpower mode.\r\n",timetosleep);
  //lowpower=0;    //1
  
  //timetosleep ms later into lowpower mode;
  TimerSetValue( &sleep, timetosleep );
  TimerStart( &sleep );
}

void Send ( String Message ) { 
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    Radio.Init( &RadioEvents );
    Radio.SetChannel( RF_FREQUENCY );
    Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                   LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                   LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                   true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

    Radio.SetSyncWord(0x34);
    char deveui[9];
    memcpy(deveui, DevEUI, 8);
    deveui[8] = '\0';
    Message = String(deveui) + Message;
    uint32_t length = bin_to_b64((uint8_t *)Message.c_str(), Message.length()-1, &txpacket[2], BUFFER_SIZE);
	txpacket[length+2] = '\0';
    MSG_ID = Request_ByteID(Normalize(Sum(String((char *)&txpacket[2])+endPayload)));
    txpacket[0] = Net_ID;
    txpacket[1] = MSG_ID;
    txpacket[length+2] = endPayload;
	txpacket[length+3] = space;
    Radio.Send((uint8_t *)txpacket, length+4); 
}

int Sum ( String Message ) {
  int SUM=0;
  for (int w=0; w<Message.length(); w++){ 
    SUM += byte(Message[w]);
  }
  return SUM;
}


int Normalize ( int P ) {
  if ( P > 288 ) {
    do {
      P = P - 256;
      } while( P > 288 );
    }
  return P;
}


int Request_ByteID ( int t ) {
  if(t <= 32)
  {
    return (32 - t);
  }
  else
  {
    return (288 - t);
  }
}