/*
 * Copyright (c) 2020 by Ashutosh Dhekne <dhekne@gatech.edu>
 * Peer-peer protocol
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file PeerProtocol_test01.ino
 * 
 *  
 */

#include <SPI.h>
#include <math.h>
//#include <DW1000.h>
//#include "ProtocolConsts.h"
#include "genericFunctions.h"
#include "RangingContainer.h"
#include "Adafruit_LSM9DS1.h"
#include <SdFat.h>
#include <time.h>
#include<TimeLib.h>
#include "RTClib.h"
#include<Wire.h>

#define VBATPIN A2
#define LED_PIN 12
#define NOISE_PIN 11
#define GOOD_PIN 6
#define SILENCE_PIN 5
#define DEV_INDICATOR_PIN 13

int AlarmNoise = 0;// = rx_packet[LED_IDX] & 0x01;
int AlarmLed = 0;// = rx_packet[LED_IDX] & 0x02;

bool first = true;

#define INIT_RTC_ALWAYS 0

#define USB_CONNECTION 0
#define INITIATOR 1
#define IGNORE_IMU 1
Adafruit_LSM9DS1 lsm = Adafruit_LSM9DS1();
#define IMU_READINGS_MAX 18*4
byte imu_buffer[IMU_READINGS_MAX];
#define IMU_SINGLE_READING_BUFF_SIZE 6
int disable_imu = 1;
//int chck =2390;

#define DEBUG_PRINT 0
#define DEBUG_CIR 0
// connection pins
#define OUR_UWB_FEATHER 1
#define AUS_UWB_FEATHER 0
#define NCIR_FULL 1016
#define RX_TIME_FP_INDEX_OFFSET 5
#define NCIR_FULL 1016
#define RX_TIME_FP_INDEX_OFFSET 5
int packet_count = 1;
int packet_type = 0;


#if(OUR_UWB_FEATHER==1)
const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 17; // irq pin
const uint8_t PIN_SS = 19; // spi select pin
#endif
/*
#if(AUS_UWB_FEATHER==1)
const uint8_t PIN_RST = 2; // reset pin
const uint8_t PIN_IRQ = 3; // irq pin
const uint8_t PIN_SS = 4; // spi select pin
#endif
*/


// DEBUG packet sent status and count
volatile boolean received = false;
volatile boolean error = false;
volatile int16_t numReceived = 0; // todo check int type
volatile boolean sendComplete = false;
volatile boolean RxTimeout = false;
String message;

byte tx_poll_msg[MAX_POLL_LEN] = {POLL_MSG_TYPE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
byte rx_resp_msg[MAX_RESP_LEN] = {RESP_MSG_TYPE, 0x02, 0, 0, 0, 0, 0};
byte tx_final_msg[MAX_FINAL_LEN] = {FINAL_MSG_TYPE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

Ranging thisRange;

byte rx_packet[128];
uint8_t myAcc[1000];

typedef enum states{STATE_IDLE, STATE_POLL, STATE_RESP_EXPECTED, STATE_FINAL_SEND, STATE_TWR_DONE, STATE_RESP_SEND, STATE_FINAL_EXPECTED, STATE_OTHER_POLL_EXPECTED, STATE_RESP_PENDING, STATE_DIST_EST_EXPECTED, STATE_DIST_EST_SEND, STATE_TIGHT_LOOP,
STATE_RECEIVE, STATE_PRESYNC, STATE_SYNC, STATE_ANCHOR, STATE_TAG, STATE_FIRST_START, STATE_OBLIVION, STATE_ACK_EXPECTED} STATES;
volatile uint8_t current_state = STATE_IDLE;
unsigned long silenced_at =0;
#define SILENCE_PERIOD 120
long randNumber;
int currentSlots = 8;
int myDevID = INITIATOR+3;

//Timer for implementing timeouts
#define CPU_HZ 48000000
#define TIMER_PRESCALER_DIV 1024

void startTimer(int frequencyHz);
void setTimerFrequency(int frequencyHz);

void TC3_Handler();

#define MAX_TIMEOUTS 2
volatile boolean timeout_established[MAX_TIMEOUTS];
volatile boolean timeout_triggered[MAX_TIMEOUTS];
volatile boolean timeout_overflow[MAX_TIMEOUTS];
volatile uint64_t timeout_time[MAX_TIMEOUTS];

SdFat sd;
SdFile store_distance; //The root file system useful for deleting all files on the SDCard
char filename[14];
int filenum = 0;
int entries_in_file=0;
int SDChipSelect = 10;

int SDEnabled=0;

#define DIST_ALARM 1500
#define DIST_WARN 2500

//Time
RTC_PCF8523 rtc;

typedef struct DeviceRespTs {
  int deviceID;
  uint64_t respRxTime;
};

#define MAX_DEVICES_TOGETHER 20
DeviceRespTs deviceRespTs[MAX_DEVICES_TOGETHER];
int currentDeviceIndex = 0;


void receiver(uint16_t rxtoval=0 ) {
  received = false;
  DW1000.newReceive();
  DW1000.setDefaults();
  // we cannot don't need to restart the receiver manually
  DW1000.receivePermanently(false);
  if (rxtoval>0) {
    DW1000.setRxTimeout(rxtoval);
  } else {
    //Serial.print("Resetting Timeout to  ");
    //Serial.println(rxtoval);
    DW1000.setRxTimeout(rxtoval);
  }
  DW1000.startReceive();
  //Serial.println("Started Receiver");
}

void print_CIR()
{
    char buff[140];
    char long_buff[1400];
  
    byte firstPath[2];
    DW1000.readBytes(RX_TIME, RX_TIME_FP_INDEX_OFFSET, firstPath, 2);
    uint16_t firstpath = uint16_t(firstPath[1]<<8 | firstPath[0]);
    uint16_t firstPath_integer = (firstpath & 0xFFC0) >> 6;
    uint16_t firstPath_fraction = (firstpath & 0x003F);
    float RX_POWER = DW1000.getReceivePower();
    float FP_POWER = DW1000.getFirstPathPower();
    
    uint8_t myAcc[4 * NCIR_FULL + 6];
    int starttap = 720;
    int endtap = 816;
    int16_t RealData = 0;
    int16_t ImaginaryData = 0;
    
    strcpy(long_buff, "CIR");
    DW1000.getAccMem(myAcc, 0, endtap + 1); //myAcc will contain 16 bit real imaginary pairs
    
    for (int i = starttap ; i < endtap; i++) {
      RealData = myAcc[(i * 4) + 2] << 8 | myAcc[(i * 4) + 1];
      ImaginaryData = myAcc[(i * 4) + 4] << 8 | myAcc[(i * 4) + 3];
   sprintf(buff, "[%d,%d,%d,d%d]", packet_count, RealData, ImaginaryData, i + 1);
    strcat(long_buff, buff);
  }
  Serial.println(long_buff);
}


void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(NOISE_PIN, OUTPUT);
  pinMode(GOOD_PIN, OUTPUT);
  pinMode(DEV_INDICATOR_PIN, OUTPUT);
  pinMode(SILENCE_PIN, INPUT_PULLUP);
  digitalWrite(GOOD_PIN, HIGH);
  analogReadResolution(10);
  // DEBUG monitoring
  Serial.begin(115200);
  while(!Serial)
  {
    delay(10);
    #if(USB_CONNECTION==0)
      break;
    #endif
  }
Serial.print("Waiting...");
delay(5000);
Serial.print("Should see this...");
  //Setting up the RTC Clock
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    //while (1);
  }
  
  if (! rtc.initialized()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("Setting new time");
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    rtc.adjust(DateTime(2020, 10, 17, 19, 40, 0));
  }

//In production, INIT_RTC_ALWAYS should be 0.
//Only turn this to 1 when testing
#if (INIT_RTC_ALWAYS == 1)
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
#endif
  //rtc.adjust(DateTime(2019, 11, 13, 10, 06, 00));
  //SoftRTC.begin(rtc.now());  // Initialize SoftRTC to the current time

//while(1) {
  Serial.println("Current Time");
  DateTime now = rtc.now();
  Serial.print(now.year());
  Serial.print("/");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.print(now.second());
  delay(1000);
//}
  Serial.print("Initializing SD card...");
  //delay(1000);
  if (!sd.begin(SDChipSelect, SPI_FULL_SPEED)) {
    Serial.println("SDCard Initialization failed!");
    SDEnabled = 0;
  } else {
    Serial.println("SDCard Initialization done.");
    SDEnabled = 1;
  }
  

  if (SDEnabled==1) {
    sprintf(filename, "dist%03d.txt", filenum);
    if (!store_distance.open(filename, O_WRITE|O_CREAT)) {
      Serial.println("Could not create file");
      delay(10000);
    }
  }
  randomSeed(analogRead(0));
  Serial.println(F("Peer-peer ranging protocol"));
  Serial.println("Free memory: ");
  Serial.println(freeMemory());
  // initialize the driver
  DW1000.begin(PIN_IRQ, PIN_RST);
  DW1000.select(PIN_SS);
  Serial.println(F("DW1000 initialized ..."));
  // general configuration
  DW1000.newConfiguration();
  DW1000.setDefaults();
  DW1000.setDeviceAddress(6);
  DW1000.setNetworkId(10);
  DW1000.enableMode(DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
  DW1000.commitConfiguration();
  Serial.println(F("Committed configuration ..."));
  // DEBUG chip info and registers pretty printed
  char msg[128];
  DW1000.getPrintableDeviceIdentifier(msg);
  Serial.print("Device ID: "); Serial.println(msg);
  DW1000.getPrintableExtendedUniqueIdentifier(msg);
  Serial.print("Unique ID: "); Serial.println(msg);
  DW1000.getPrintableNetworkIdAndShortAddress(msg);
  Serial.print("Network ID & Device Address: "); Serial.println(msg);
  DW1000.getPrintableDeviceMode(msg);
  Serial.print("Device mode: "); Serial.println(msg);
  // attach callback for (successfully) received messages
  DW1000.attachReceivedHandler(handleReceived);
  DW1000.attachReceiveTimeoutHandler(handleRxTO);
  DW1000.attachReceiveFailedHandler(handleError);
  DW1000.attachErrorHandler(handleError);
  DW1000.attachSentHandler(handleSent);
  // start reception
  
  #if (INITIATOR==0)
    disable_imu = 1;
    receiver(0);
    
  #endif
  current_state = STATE_IDLE;

  for(int i=0;i<MAX_TIMEOUTS;i++) {
    timeout_established[i]=false;
    timeout_triggered[i]=false;
    timeout_overflow[i]=false;
    timeout_time[i]=0;
  }

#if (INITIATOR == 1)
digitalWrite(DEV_INDICATOR_PIN, 1);
delay(500);
digitalWrite(DEV_INDICATOR_PIN, 0);
delay(500);
digitalWrite(DEV_INDICATOR_PIN, 1);
delay(500);
digitalWrite(DEV_INDICATOR_PIN, 0);
delay(500);
digitalWrite(DEV_INDICATOR_PIN, 1);
delay(5000);
digitalWrite(DEV_INDICATOR_PIN, 0);
//delay(500);
#else
digitalWrite(DEV_INDICATOR_PIN, 1);
delay(1000);
digitalWrite(DEV_INDICATOR_PIN, 0);
delay(500);
digitalWrite(DEV_INDICATOR_PIN, 1);
delay(200);
digitalWrite(DEV_INDICATOR_PIN, 0);
delay(500);
digitalWrite(DEV_INDICATOR_PIN, 1);
delay(500);
digitalWrite(DEV_INDICATOR_PIN, 0);
#endif

#if (INITIATOR==1)
  while (!lsm.begin())
  {
    Serial.println("Oops ... unable to initialize the LSM9DS1. Check your wiring!");
    delay(1000);
    #if IGNORE_IMU==1
    disable_imu = 1;
    break;
    #endif
  }
  if (disable_imu==0) {
  Serial.println("Found LSM9DS1 9DOF");
  // 1.) Set the accelerometer range
  lsm.setupAccel(lsm.LSM9DS1_ACCELRANGE_2G);
  //lsm.setupAccel(lsm.LSM9DS1_ACCELRANGE_4G);
  //lsm.setupAccel(lsm.LSM9DS1_ACCELRANGE_8G);
  //lsm.setupAccel(lsm.LSM9DS1_ACCELRANGE_16G);
  
  // 2.) Set the magnetometer sensitivity
  lsm.setupMag(lsm.LSM9DS1_MAGGAIN_4GAUSS);
  //lsm.setupMag(lsm.LSM9DS1_MAGGAIN_8GAUSS);
  //lsm.setupMag(lsm.LSM9DS1_MAGGAIN_12GAUSS);
  //lsm.setupMag(lsm.LSM9DS1_MAGGAIN_16GAUSS);

  // 3.) Setup the gyroscope
  lsm.setupGyro(lsm.LSM9DS1_GYROSCALE_245DPS);
  //lsm.setupGyro(lsm.LSM9DS1_GYROSCALE_500DPS);
  //lsm.setupGyro(lsm.LSM9DS1_GYROSCALE_2000DPS);
  }
#endif

  //startTimer(100);
  
}


void handleSent() {
  // status change on sent success
  sendComplete = true;
  //Serial.println("Send complete");
}


void handleReceived() {
  // status change on reception success
  
  DW1000.getData(rx_packet, DW1000.getDataLength());
  //Serial.println("Received something...");
  received = true;
  //show_packet(rx_packet, DW1000.getDataLength());
}

void handleError() {
  error = true;
}

void handleRxTO() {
  current_state = STATE_IDLE;
  RxTimeout = true;
  #if (DEBUG_PRINT==1)
  Serial.println("Rx Timeout");
  Serial.println("State: ");
  Serial.println(current_state);
  #endif
}


float gravity_f = 0.0f;
uint16_t seq = 0;
uint16_t recvd_poll_seq = 0;
uint16_t recvd_resp_seq = 0;
uint16_t recvd_final_seq = 0;

volatile int unlock_waiting = 0;
long loop_counter = 0;
volatile long waiting = 0;
DW1000Time currentDWTime;
uint64_t currentTime;
uint64_t final_sent_time;
uint64_t init_time;
double elapsed_time;
double TIME_UNIT = 1.0 / (128*499.2e6); // seconds

#define YIFENG_TEST 0
int test_flag = 0;

double start_time_us = 0, current_time_us = 0;

void loop() {
  //digitalWrite(GOOD_PIN, HIGH);
  //Serial.println(analogRead(A2));
  //Serial.print("Pin Read: ");
  //Serial.println(digitalRead(SILENCE_PIN));

  if(SDEnabled) {
  if (digitalRead(SILENCE_PIN)==0)
  {
    store_distance.println("Silenced");
    silenced_at = rtc.now().unixtime();
  }
  }
  
  /*
  Serial.print(analogRead(A1));
  Serial.print(",");
  Serial.print(analogRead(A0));
  Serial.print(",");
  Serial.println(analogRead(A4));
  */
  #if (DEBUG_PRINT==1)
  if((loop_counter++)%500000==0) {
    //Serial.println("Current state: ");
    //Serial.println(current_state);
  }
  #endif

  if (RxTimeout == true) {
    RxTimeout = false;
    /*
    DW1000.getSystemTimestamp(currentDWTime);
    currentTime = currentDWTime.getTimestamp();
    elapsed_time = (currentTime - init_time) * TIME_UNIT * 1e6;
    Serial.print("Timed out. now current_state: ");
    
    Serial.print(current_state);
    Serial.print(" Elasped time: ");
    Serial.println(elapsed_time);
    */
    current_state = STATE_IDLE;
    #if (INITIATOR==0)
      receiver(0);
    #endif
  }


 
  
  switch(current_state) {
    case STATE_IDLE: {
      // Serial.println("IN IDLE STATE");
      if (RxTimeout == true) {
        RxTimeout = false;
        #if (INITIATOR==0)
          receiver(0);
        #endif
      }
      if (received) {
        received = false;
        
        thisRange.initialize();
        current_state = STATE_RESP_SEND;
        #if (DEBUG_PRINT==1)
        Serial.println("******************");
        Serial.println("Going to resp send");
        #endif
        
      }
      #if (INITIATOR==1)
      //Randomly begin the POLL process.
      waiting=0;
      received = false;
      sendComplete = false;
      //randNumber = random(300);
      //if (randNumber<10) {
        //Switch to POLL state
        current_state = STATE_POLL;
        unlock_waiting = 0;
      //}
      #endif
      break;
    }
    case STATE_POLL: {
      if (first) {
        while((millis() / 500) % 2 == 0) {
          delay(100);
        }
        first = false;        
      }
      while((millis() / 500) % 2 == 1) {
        delay(100);
      }
      // Serial.println("IN STATE POLL");
      //Send POLL here
      //Serial.println("State: POLL");
      seq++;
      currentDeviceIndex = 0;
      //currentSlots = 1;
      //tx_poll_msg[POLL_MSG_RAND_BOUND_IDX] = currentSlots;
      tx_poll_msg[SRC_IDX] = myDevID;
      tx_poll_msg[DST_IDX] = BROADCAST_ID;
      tx_poll_msg[SEQ_IDX] = seq & 0xFF;
      tx_poll_msg[SEQ_IDX + 1] = seq >> 8;

      
      //Collect IMU data here and send via UWB
      int imu_buffer_counter = 0;
      if (disable_imu==0)
      {
        collect_imu_data(imu_buffer, &imu_buffer_counter);
      }
      currentTime = get_time_u64();
      
      //memcpy(&tx_poll_msg[POLL_MSG_IMU_IDX], imu_buffer, IMU_SINGLE_READING_BUFF_SIZE*3);
      //any_msg_set_ts(&tx_poll_msg[POLL_MSG_IMU_TS_IDX], currentTime);
      
      generic_send(tx_poll_msg, sizeof(tx_poll_msg), POLL_MSG_POLL_TX_TS_IDX, SEND_DELAY_FIXED);
      //Serial.print("Sending POLL ");
      //Serial.println(seq);
      //set_timeout(1, FIXED_DELAY*currentSlots);
      //
      current_state = STATE_RESP_EXPECTED;
      int i = 0;


      while(!sendComplete){
      };

      current_time_us = get_time_us();
      //Serial.println("Send completed");
      sendComplete = false;
      receiver(TYPICAL_RX_TIMEOUT);
      DW1000.getSystemTimestamp(currentDWTime);
      init_time = currentDWTime.getTimestamp();
      break;
    }
    /* Dont delete, TBC*/
    case STATE_RESP_SEND: {
      
      //uint8_t current_poll_rand = rx_packet[POLL_MSG_RAND_BOUND_IDX];

      //int16_t gravity = (((int16_t)rx_packet[POLL_MSG_POLL_TX_TS_IDX+ANY_MSG_TS_LEN+5])<<8) | rx_packet[POLL_MSG_POLL_TX_TS_IDX+ANY_MSG_TS_LEN+4];
      //gravity_f = ((float)gravity)*(0.061/1000.0)*9.80665f;
      
      seq = rx_packet[SEQ_IDX] +  ((uint16_t)rx_packet[SEQ_IDX+1] << 8);
      #if (DEBUG_PRINT==1)  
      Serial.print("Preparing to send response for ");
      Serial.println(seq);
      
      #endif
      //show_packet(rx_packet, DW1000.getDataLength());
      uint64_t PollTxTime_64=0L;
      any_msg_get_ts(&rx_packet[POLL_MSG_POLL_TX_TS_IDX], &PollTxTime_64);
      thisRange.PollTxTime = DW1000Time((int64_t)PollTxTime_64);
      //Serial.println("Now the time thing");
      DW1000Time rxTS; 
      DW1000.getReceiveTimestamp(rxTS);
      thisRange.PollRxTime = rxTS;
      //Serial.println("About to start generic send: ");
      //show_packet(rx_resp_msg, sizeof(rx_resp_msg));
      //delay(1000);
      rx_resp_msg[DST_IDX] = rx_packet[SRC_IDX];
      rx_resp_msg[SRC_IDX] = myDevID;
      rx_resp_msg[LED_IDX] = AlarmNoise | AlarmLed;
      //generic_send(rx_resp_msg, sizeof(rx_resp_msg), POLL_MSG_POLL_TX_TS_IDX, SEND_DELAY_RANDOM, current_poll_rand);
      generic_send(rx_resp_msg, sizeof(rx_resp_msg), POLL_MSG_POLL_TX_TS_IDX, SEND_DELAY_FIXED);
      //Serial.println("After sending...");
      //show_packet(rx_resp_msg, sizeof(rx_resp_msg));
      while(!sendComplete);
      sendComplete = false;
      DW1000Time txTS; 
      DW1000.getTransmitTimestamp(txTS);
      thisRange.RespTxTime = txTS;
      
      //Serial.println("Response sent");
      receiver(60);
      current_state = STATE_FINAL_EXPECTED;
      break;
    }
    //*/
    case STATE_RESP_EXPECTED: {
      //Serial.println("State: RESP EXPECTED");
      /*if (sendComplete) {
        sendComplete = false;
        receiver();
      }*/
      if (received) {
        received = false;
        //Serial.print("received MSG type: ");
        //Serial.println(rx_packet[0]);
        show_packet(rx_packet, DW1000.getDataLength());
        if ((rx_packet[DST_IDX] == myDevID || rx_packet[DST_IDX] == BROADCAST_ID)) {
          //Serial.println("Recieved response!");
          recvd_resp_seq = rx_packet[SEQ_IDX] +  ((uint16_t)rx_packet[SEQ_IDX+1] << 8);
          deviceRespTs[currentDeviceIndex].deviceID = rx_packet[SRC_IDX];
          DW1000Time rxTS;
          DW1000.getReceiveTimestamp(rxTS);
          deviceRespTs[currentDeviceIndex].respRxTime = rxTS.getTimestamp();
          //currentDeviceIndex++;
          AlarmNoise = rx_packet[LED_IDX] & 0x01;
          AlarmLed = rx_packet[LED_IDX] & 0x02;
          //Serial.print("Noise?");
          //Serial.println(rx_packet[LED_IDX]);
          if (AlarmLed == 2)
          {
            digitalWrite(LED_PIN, HIGH);
            digitalWrite(GOOD_PIN, LOW);
            digitalWrite(NOISE_PIN, LOW);
            if (SDEnabled) {
            store_distance.println("Yellow");
            }
          } else {
            digitalWrite(LED_PIN, LOW);
          }
          if (AlarmNoise == 1)
          {
            if (digitalRead(SILENCE_PIN)==0)
            {
              digitalWrite(NOISE_PIN, LOW);
              digitalWrite(LED_PIN, LOW);
              if (SDEnabled) {
              store_distance.println("RED Silenced");
              store_distance.println(rtc.now().unixtime());
              }
              silenced_at = rtc.now().unixtime();
            } else if (rtc.now().unixtime()-silenced_at < SILENCE_PERIOD)
            {
              digitalWrite(NOISE_PIN, LOW);
              digitalWrite(LED_PIN, LOW);
              if (SDEnabled) {
              store_distance.println("RED Still Silenced");
              store_distance.println(rtc.now().unixtime());
              }
            } else {
              digitalWrite(NOISE_PIN, LOW);
              digitalWrite(LED_PIN, LOW);
              digitalWrite(GOOD_PIN, LOW);
            if (SDEnabled) {
              store_distance.println("RED");
            }
            }
            
          } else {
            digitalWrite(NOISE_PIN, LOW);
            //digitalWrite(GOOD_PIN, HIGH);
          }
          if(AlarmLed!=2 && AlarmNoise != 1)
          {
            digitalWrite(GOOD_PIN, HIGH);
            if (SDEnabled) {
            store_distance.println("GREEN");
            }
          } else {
            //digitalWrite(GOOD_PIN, LOW);
          }
          
          
          float volts = getVoltage();
          if (SDEnabled) {
          store_distance.print("Voltage: ");
          store_distance.println(volts);
          }
          //Serial.print("Voltage: ");
          //Serial.println(volts);
          if (SDEnabled) {
          entries_in_file++;
          if(entries_in_file > 100)
          {
            entries_in_file = 0;
            Serial.println("Changed files");
            SdFile::dateTimeCallback(dateTime);

            store_distance.close();
            filenum++;
            sprintf(filename, "dist%03d.txt", filenum);
            if (!store_distance.open(filename, O_WRITE|O_CREAT)) {
              Serial.println("Could not create file");
              delay(1000);
            }
            
          }
          }
          //Serial.print("currentDeviceIndex: ");
          //Serial.println(currentDeviceIndex);
          //unlock_waiting = 1;
          current_state = STATE_FINAL_SEND;
                
        } else {
          received = false;
          receiver(TYPICAL_RX_TIMEOUT);
        }
      } else {
        /*waiting++;
        if (waiting>200000)
        {
          current_state=STATE_IDLE;
        }*/
      }
      if (unlock_waiting == 1)
      {
        waiting++;
        if (waiting > 50000)
        {
          waiting=0;
          current_state = STATE_FINAL_SEND;
        }
        if (timeout_triggered[1] == true) {
          current_state = STATE_FINAL_SEND;
        }
      }
      break;
    }
    case STATE_FINAL_SEND: {
      //Serial.println("State: FINAL SEND");
      tx_final_msg[SRC_IDX] = myDevID;
      tx_final_msg[DST_IDX] = BROADCAST_ID;
      tx_final_msg[SEQ_IDX] = recvd_resp_seq & 0xFF;
      tx_final_msg[SEQ_IDX + 1] = recvd_resp_seq >> 8;
      //tx_final_msg[FINAL_MSG_NUM_NODES_IDX] = currentDeviceIndex;
      int imu_buffer_counter = 0;
      //collect_imu_data(imu_buffer, &imu_buffer_counter);
      currentTime = get_time_u64();
      //memcpy(&tx_final_msg[FINAL_MSG_IMU_IDX], imu_buffer, IMU_SINGLE_READING_BUFF_SIZE*3);
      //any_msg_set_ts(&tx_final_msg[FINAL_MSG_IMU_TS_IDX], currentTime);
      //Serial.print("Writing resprxtime into final packet: currentDeviceIndex: ");
      //Serial.println(currentDeviceIndex);
      //for (int i=0; i<currentDeviceIndex; i++) {
      int i=0;

      tx_final_msg[FINAL_MSG_RESP_RX_TS_IDX + (i*FINAL_MSG_ONE_RESP_ENTRY)] = deviceRespTs[i].deviceID;
      any_msg_set_ts(&tx_final_msg[FINAL_MSG_RESP_RX_TS_IDX + 1 + (i*FINAL_MSG_ONE_RESP_ENTRY)], deviceRespTs[i].respRxTime);

      generic_send(tx_final_msg, MAX_FINAL_LEN, FINAL_MSG_FINAL_TX_TS_IDX, SEND_DELAY_FIXED);
      while(!sendComplete);
      sendComplete = false;
      
      DW1000.getSystemTimestamp(currentDWTime);
      final_sent_time = currentDWTime.getTimestamp();

      current_state = STATE_ACK_EXPECTED;
      receiver(TYPICAL_RX_TIMEOUT);
      //current_state = STATE_OBLIVION;
      break;
    }
    case STATE_ACK_EXPECTED: {
       //Serial.println("State: ACK EXPECTED");
      if (received) {
        received = false;
        //Serial.println("ACK: receive something!");
        if (rx_packet[0] == TWR_DONE_TYPE) {
           //Serial.println("ACK: receive a TWR DONE packet!");
          recvd_resp_seq = rx_packet[SEQ_IDX] +  ((uint16_t)rx_packet[SEQ_IDX+1] << 8);
          if(recvd_resp_seq == seq){
            //Serial.println("Recieved ACK!");
            current_state = STATE_POLL;
            break;
          }
        }
      }

      DW1000.getSystemTimestamp(currentDWTime);
      currentTime = currentDWTime.getTimestamp();
      elapsed_time = (currentTime - final_sent_time) * TIME_UNIT * 1000;
      if(elapsed_time >= TYPICAL_RX_TIMEOUT){
          current_state = STATE_POLL;
          break;
      }
      start_time_us = get_time_us();
      break;
    }
    /* Dont delete, TBC*/
    case STATE_FINAL_EXPECTED: {
      if(received) {
        received = false;
        //Serial.print("Final Received for: ");
        //Serial.println(seq);
        //show_packet(rx_packet, DW1000.getDataLength());

        if (rx_packet[DST_IDX]==myDevID || rx_packet[DST_IDX] == BROADCAST_ID) {
          DW1000Time rxTS;
          DW1000.getReceiveTimestamp(rxTS);
          thisRange.FinalRxTime = rxTS;
          //int numNodes = rx_packet[FINAL_MSG_NUM_NODES];
          //Serial.print("Total Nodes: ");
          //Serial.println(numNodes);
          //for (int i=0;i<numNodes;i++) {
            //if (rx_packet[FINAL_MSG_RESP_DEV_ID + (i*FINAL_MSG_ONE_RESP_ENTRY)]==myDevID) {
            int i = 0;
              uint64_t RespRxTime_64=0L;
              any_msg_get_ts(&rx_packet[FINAL_MSG_RESP_RX_TS_IDX + (i*FINAL_MSG_ONE_RESP_ENTRY)+1], &RespRxTime_64);
              thisRange.RespRxTime = DW1000Time((int64_t)RespRxTime_64);
              //break;
            //}
          //}
          uint64_t FinalTxTime_64=0L;
          any_msg_get_ts(&rx_packet[FINAL_MSG_FINAL_TX_TS_IDX], &FinalTxTime_64);
          seq = rx_packet[SEQ_IDX];
          receiver(0); //Enable the receiver quickly to allow the next POLL to work
          thisRange.FinalTxTime = DW1000Time((int64_t)FinalTxTime_64);
          //thisRange.printAll();
          //Serial.println("-------");
          Serial.print("Distance is: ");
          Serial.println(thisRange.calculateRange());
          int dist = thisRange.calculateRange();
          //thisRange.printAll();
          //Serial.print(seq);
          //Serial.print(") Dist: ");
          //Serial.println(dist);
          
          //Serial.print(", Gravity: ");
          //Serial.println(gravity_f);
          current_state = STATE_IDLE;
          digitalWrite(GOOD_PIN, HIGH);
          if (SDEnabled) {
          store_distance.print("Dist: ");
          store_distance.println(dist);
          }
          

          if (dist<DIST_ALARM && dist>0) {
            AlarmNoise = 1;
            AlarmLed = 0;
          } else if (dist<DIST_WARN  && dist>0) {
            AlarmLed = 2;
            AlarmNoise = 0;
          } else {
            AlarmLed = 0;
            AlarmNoise = 0;
          }

          
          //Serial.print("Noise?");
          //Serial.println(rx_packet[LED_IDX]);
          if (AlarmLed == 2)
          {
            digitalWrite(LED_PIN, HIGH);
            digitalWrite(GOOD_PIN, LOW);
            digitalWrite(NOISE_PIN, LOW);
            if (SDEnabled) {
            store_distance.println("Yellow");
            }
          } else {
            digitalWrite(LED_PIN, LOW);
          }
          if (AlarmNoise == 1)
          {
            if (digitalRead(SILENCE_PIN)==0)
            {
              digitalWrite(NOISE_PIN, LOW);
              digitalWrite(LED_PIN, LOW);
              if (SDEnabled) {
              store_distance.println("RED Silenced");
              }
              silenced_at = rtc.now().unixtime();
            } else if (rtc.now().unixtime()-silenced_at < SILENCE_PERIOD)
            {
              digitalWrite(NOISE_PIN, LOW);
              digitalWrite(LED_PIN, LOW);
              if (SDEnabled) {
              store_distance.println("RED Still Silenced");
              store_distance.println(rtc.now().unixtime());
              }
            } else 
            {
              digitalWrite(NOISE_PIN, LOW);
              digitalWrite(LED_PIN, LOW);
              digitalWrite(GOOD_PIN, LOW);
            if (SDEnabled) {
            store_distance.println("RED");
            }
            }
            
          } else {
            digitalWrite(NOISE_PIN, LOW);
            //digitalWrite(GOOD_PIN, HIGH);
          }
          if(AlarmLed!=2 && AlarmNoise != 1)
          {
            digitalWrite(GOOD_PIN, HIGH);
            if (SDEnabled) {
            store_distance.println("GREEN");
            }
          } else {
            //digitalWrite(GOOD_PIN, LOW);
          }

          
          float volts = getVoltage();
          if (SDEnabled) {
          store_distance.print("Voltage: ");
          store_distance.println(volts);
          }
          //Serial.print("Voltage: ");
          //Serial.println(volts);
          if (SDEnabled) {
          entries_in_file++;
          if(entries_in_file > 100)
          {
            entries_in_file = 0;
            Serial.println("Changed files");
            SdFile::dateTimeCallback(dateTime);
            store_distance.close();
            filenum++;
            sprintf(filename, "dist%03d.txt", filenum);
            if (!store_distance.open(filename, O_WRITE|O_CREAT)) {
              Serial.println("Could not create file");
              delay(1000);
            }
            
          }
          }
          
        }
      }
      break;
    }
    //*/
    case STATE_OBLIVION: {
      //Do nothing!
    }
    
    
  }
  if(DEBUG_CIR==1)
  {
  print_CIR();
  }
  //if (current_state==  
  // enter on confirmation of ISR status change (successfully received)
  /*if (received) {
    numReceived++;
    // get data as string
    DW1000.getData(message);
    //Serial.print("Received message ... #"); Serial.println(numReceived);
    //Serial.print("Data is ... "); Serial.println(message);
    //Serial.print("FP power is [dBm] ... "); Serial.println(DW1000.getFirstPathPower());
    //Serial.print("RX power is [dBm] ... "); Serial.println(DW1000.getReceivePower());
    //Serial.print("Signal quality is ... "); Serial.println(DW1000.getReceiveQuality());
    received = false;
    DW1000Time timeRx;
    DW1000.getReceiveTimestamp(timeRx);
    //timeRx.print();
    //Serial.println("Accessing accumulator");
    int starttap = 700;
    int endtap = 780;
    DW1000.getAccMem(myAcc, starttap, endtap);
    Serial.println("Returned from acc");
    for (int i=1;i<4*(endtap-starttap);i=i+4)
    {
      int16_t real = (((uint16_t)myAcc[i+1])<<8) + ((uint16_t)myAcc[i]);
      int16_t imag = (((uint16_t)myAcc[i+3])<<8) + ((uint16_t)myAcc[i+2]);
      Serial.println(sqrt(((double)real)*((double)real) + ((double)imag)*((double)imag)));
      //Serial.print(real);
      //Serial.print(", ");
      //Serial.println(imag);
      
    }
    delay(5000);
    receiver();
  }
  if (error) {
    Serial.println("Error receiving a message");
    error = false;
    DW1000.getData(message);
    Serial.print("Error data is ... "); Serial.println(message);
  }*/
}





void show_packet(byte packet[], int num) {
  #if (DEBUG_PRINT==1)
  for (int i=0;i<num;i++) {
    Serial.print(packet[i], HEX);
    Serial.print(" ");
  }
  #endif
  
}

//Timer Functions
//Timer functions.
void setTimerFrequency(int frequencyHz) {
  int compareValue = (CPU_HZ / (TIMER_PRESCALER_DIV * frequencyHz)) - 1;
  
  TcCount16* TC = (TcCount16*) TC3;
  // Make sure the count is in a proportional position to where it was
  // to prevent any jitter or disconnect when changing the compare value.
  TC->COUNT.reg = map(TC->COUNT.reg, 0, TC->CC[0].reg, 0, compareValue);
  TC->CC[0].reg = compareValue;
  //Serial.println(TC->COUNT.reg);
  //Serial.println(TC->CC[0].reg);
  while (TC->STATUS.bit.SYNCBUSY == 1);
}

void startTimer(int frequencyHz) {
  
  REG_GCLK_CLKCTRL = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TCC2_TC3) ;
  
  while ( GCLK->STATUS.bit.SYNCBUSY == 1 ); // wait for sync

  TcCount16* TC = (TcCount16*) TC3;
  
  TC->CTRLA.reg &= ~TC_CTRLA_ENABLE;
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync

  // Use the 16-bit timer
  TC->CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync

  // Use match mode so that the timer counter resets when the count matches the compare register
  TC->CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync

  // Set prescaler to 1024
  TC->CTRLA.reg |= TC_CTRLA_PRESCALER_DIV1024;
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync

  setTimerFrequency(frequencyHz);

  // Enable the compare interrupt
  TC->INTENSET.reg = 0;
  TC->INTENSET.bit.MC0 = 1;

  
    NVIC_SetPriority(TC3_IRQn, 3);
    NVIC_EnableIRQ(TC3_IRQn);  
    

  TC->CTRLA.reg |= TC_CTRLA_ENABLE;
  while (TC->STATUS.bit.SYNCBUSY == 1); // wait for sync
}


void collect_imu_data(byte *imu_buff, int *imu_buffer_counter) {
  if (disable_imu==0)
  {
  lsm.readBuffer(XGTYPE, 0x80 | lsm.LSM9DS1_REGISTER_OUT_X_L_XL, IMU_SINGLE_READING_BUFF_SIZE, &imu_buff[(*imu_buffer_counter)]);
  (*imu_buffer_counter) += IMU_SINGLE_READING_BUFF_SIZE; 
  
  
  
  lsm.readBuffer(MAGTYPE, 0x80 | lsm.LSM9DS1_REGISTER_OUT_X_L_M, IMU_SINGLE_READING_BUFF_SIZE, &imu_buff[(*imu_buffer_counter)]);
  (*imu_buffer_counter) += IMU_SINGLE_READING_BUFF_SIZE; 
  
  
  lsm.readBuffer(XGTYPE, 0x80 | lsm.LSM9DS1_REGISTER_OUT_X_L_G, IMU_SINGLE_READING_BUFF_SIZE, &imu_buff[(*imu_buffer_counter)]);
  (*imu_buffer_counter) += IMU_SINGLE_READING_BUFF_SIZE; 
  } else {
    (*imu_buffer_counter) += IMU_SINGLE_READING_BUFF_SIZE*3;
  }
}

void set_imu_data(float data[], int type){
      float ax, ay, az;
      float acc_unit = 2 * 9.8 / 32768;
      float mag_unit = 0.14;
      float gyro_unit = 0.00875;
      int unit = 1;
      int imu_idx = 6 * type;
      if(type == 0){          
          data[0] = (int16_t)((imu_buffer[0 + imu_idx] + ((int16_t)imu_buffer[1 + imu_idx] << 8))) * acc_unit;
          data[1] = (int16_t)((imu_buffer[2 + imu_idx] + ((int16_t)imu_buffer[3 + imu_idx] << 8))) * acc_unit;
          data[2] = (int16_t)((imu_buffer[4 + imu_idx] + ((int16_t)imu_buffer[5 + imu_idx] << 8))) * acc_unit;
      }else if(type == 1){
          data[0] = (int16_t)((imu_buffer[0 + imu_idx] + ((int16_t)imu_buffer[1 + imu_idx] << 8))) * mag_unit;
          data[1] = (int16_t)((imu_buffer[2 + imu_idx] + ((int16_t)imu_buffer[3 + imu_idx] << 8))) * mag_unit;
          data[2] = (int16_t)((imu_buffer[4 + imu_idx] + ((int16_t)imu_buffer[5 + imu_idx] << 8))) * mag_unit;
      }else{
          data[0] = (int16_t)((imu_buffer[0 + imu_idx] + ((int16_t)imu_buffer[1 + imu_idx] << 8))) * gyro_unit;
          data[1] = (int16_t)((imu_buffer[2 + imu_idx] + ((int16_t)imu_buffer[3 + imu_idx] << 8))) * gyro_unit;
          data[2] = (int16_t)((imu_buffer[4 + imu_idx] + ((int16_t)imu_buffer[5 + imu_idx] << 8))) * gyro_unit;
      }
}


void TC3_Handler() 
{
  DW1000Time currectTS;
  uint64_t currentUWBTime;
  for(int i=0;i<MAX_TIMEOUTS;i++) {
    if (timeout_established[i]) {
      DW1000.getSystemTimestamp(currectTS);
      currentUWBTime = currectTS.getTimestamp();
      break; //Any timeout if established will populate the currentUWBTime
    }
  }
  for(int i=0;i<MAX_TIMEOUTS;i++) {
    if (timeout_established[i]) {
      if(currentUWBTime > timeout_time[i]) {
        timeout_established[i] = false;
        timeout_time[i] = INFINITE_TIME;
        timeout_overflow[i] = false;
        timeout_triggered[i] = true;
      } else if (timeout_overflow[i] == true && currentUWBTime > (timeout_time[i] - 2^40)) {
        timeout_established[i] = false;
        timeout_time[i] = INFINITE_TIME;
        timeout_overflow[i] = false;
        timeout_triggered[i] = true;
      }
    }
  }
}

void set_timeout(int whichTO, uint32_t delayTime) {
  DW1000Time currectTS;
  uint64_t currentUWBTime;
  DW1000.getSystemTimestamp(currectTS);
  currentUWBTime = currectTS.getTimestamp();
  DW1000Time deltaTime = DW1000Time(delayTime, DW1000Time::MILLISECONDS);
  timeout_time[whichTO] = (currectTS + deltaTime).getTimestamp();
  if (timeout_time[whichTO] > 2^40) {
    timeout_overflow[whichTO] = true;
  } else {
    timeout_overflow[whichTO] = false;
  }
}

double get_time_us(){
    DW1000.getSystemTimestamp(currentDWTime);
    currentTime = currentDWTime.getTimestamp();
    return currentTime * TIME_UNIT * 1e6;
}

uint64_t get_time_u64(){
    DW1000.getSystemTimestamp(currentDWTime);
    return currentDWTime.getTimestamp();
}


//Utility functions
void dateTime(uint16_t* date, uint16_t* time_) {
  DateTime now = rtc.now();
  // return date using FAT_DATE macro to format fields
  *date = FAT_DATE(now.year(), now.month(), now.day());

  // return time using FAT_TIME macro to format fields
  *time_ = FAT_TIME(now.hour(), now.minute(), now.second());
  printDateTime();
}

float getVoltage()
{
  
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}

void printDateTime()
{
  DateTime now = rtc.now();
  Serial.print(now.year());
  Serial.print("/");
  Serial.print(now.month());
  Serial.print("/");
  Serial.print(now.day());
  Serial.print(" ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());
}

#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__



int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}
