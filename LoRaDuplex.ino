/*
  LoRa Duplex communication

  Sends a message every half second, and polls continually
  for new incoming messages. Implements a one-byte addressing scheme,
  with 0xFF as the broadcast address.

  Uses readString() from Stream class to read payload. The Stream class'
  timeout may affect other functuons, like the radio's callback. For an

  created 28 April 2017
  by Tom Igoe
*/
#include <SPI.h>              // include libraries
#include <LoRa.h>


// uncomment the section corresponding to your board
// BSFrance 2017 contact@bsrance.fr 

//  //LoR32u4 433MHz V1.0 (white board)
//  #define SCK     15
//  #define MISO    14
//  #define MOSI    16
//  #define SS      1
//  #define RST     4
//  #define DI0     7
//  #define BAND    433E6 
//  #define PABOOST true

//  //LoR32u4 433MHz V1.2 (white board)
//  #define SCK     15
//  #define MISO    14
//  #define MOSI    16
//  #define SS      8
//  #define RST     4
//  #define DI0     7
//  #define BAND    433E6 
//  #define PABOOST true 

  //LoR32u4II 868MHz or 915MHz (black board)
  #define SCK     15
  #define MISO    14
  #define MOSI    16
  #define SS      8
  #define RST     4
  #define DI0     7
  #define BAND    915E6  // 915E6
  #define PABOOST true 
  
/* AES Encryption Library Stuff */
/*#include "AES.h"
AES aes ;
static uint8_t bits = 128;
byte iv [N_BLOCK] ;
byte *key = (unsigned char*)"0123456789010123";
unsigned long long int my_iv = 36753562;*/

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0xE2;     // address of this device
byte destination = 0xE2;      // destination to send to
long lastSendTime = 0;        // last send time
int interval = 2000;          /// interval between sends

String thisIs = "COM1";
String ret = "";
const int buzzer = 5;
bool firstRun = true;
char secret_key[12] = { 28, 253, 144, 71, 77, 177, 217, 172, 77, 177, 217, 172 };
boolean pinging = false;
int pingInterval = 10000;
byte channels[10] = { 0xE2, 0xF7, 0xA3, 0xB9, 0xC1, 0xC3, 0xD6, 0xD8, 0xA6, 0xE5 };

void setup() {
  pinMode(buzzer, OUTPUT);
  
  Serial.begin(19200);                   // initialize serial
  while (!Serial);

  Serial.println("LoRa Duplex");

  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(SS,RST,DI0);// set CS, reset, IRQ pin

  if (!LoRa.begin(BAND,PABOOST)) {             // initialize ratio at 915 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true);                       // if failed, do nothing
  }

  Serial.println("LoRa init succeeded.");
}

void loop() {

  // get their handle from serial console
  if (firstRun) {
    Serial.println("What is your Handle/Username?");
    while (!Serial.available());
    changeName(Serial.readString());
    firstRun = false;
    Serial.println("");
    Serial.print("Hi ");
    Serial.print(thisIs);
    Serial.print(", \n");
    Serial.println("You are setup and ready to start coms! Type ? for commands.");
  }

  if (pinging) {
     if (millis() - lastSendTime > pingInterval)  {
      sendMessage("Ping from "+thisIs+"</E");
      lastSendTime = millis();
      Serial.println("Sending ping...");
     }
  }

  // send message from serial console
  if (millis() - lastSendTime > interval) {
    if (Serial.available()){
      String fromSerial = Serial.readString();
      if (command_helper(fromSerial)) 
        return;  
      
      String message = thisIs;
      message.concat(": ");
      message.concat(fromSerial); 
      message.trim();  
      Serial.println(message); 
      message.concat("</E");
      sendMessage(message);
      
      // Testing encryption
      //Serial.println(encrypt(message));
      //Serial.println(decrypt(encrypt(message)).substring(0, message.indexOf("</E")));
      
      lastSendTime = millis();            // timestamp the message
      interval = random(2000) + 1000;    // 2-3 seconds
    }
  }
  
  // parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());
}


boolean command_helper(String cmd) {
    
    cmd.trim();
    
    if (cmd.equals("?")) {
      Serial.println("**********HELP MENU**********");
      Serial.println("ping - ping out every 10 seconds until stop command");
      Serial.println("stop - use stop after various commands that repeat to stop them");
      Serial.println("channel <1-10> - change the channel ie: channel 2");
      Serial.println("*****************************");
      return true;
    }
    
    if (cmd.indexOf("channel ")==0) {

      cmd = splitAt(cmd, "channel ");
      int str_len = cmd.length() + 1;
      char buf[str_len];
      cmd.toCharArray(buf, str_len);
      int channel = atoi(buf);
      channelSelector(channel);
      
      return true;
    }
    
    if (cmd.equals("ping")) {
      Serial.println("Starting ping, type 'stop' to stop pinging");
      pinging = true;
      return true;
    }
    
    if (cmd.equals("stop")) {
      Serial.println("Stopping...");
      pinging = false;
      return true;
    }
    
    return false;
}

void channelSelector(int channel) {
  if (channel > 10)
    channel = 10;
   channel = channel-1;
   localAddress = channels[channel];
   destination = channels[channel]; 
   Serial.print("Channel changed to ");
   Serial.println((channel+1));
}

String splitAt(String d1, String delim) {
  int pos = d1.indexOf(delim) + delim.length();
  if (pos < -1)
    return "";
  return d1.substring(pos);
}

void encrypt_helper(char *array, int array_size) {
    int i;
    int ic = 0;    
    for(i = 0; i < array_size; i++){
      ic++;
      if (ic>sizeof(secret_key)-1)
          ic=0;
      array[i] ^= secret_key[ic];    
    }
}

String encrypt(String message){
  
  char plainText[message.length()+1];
  message.toCharArray(plainText, message.length()+1);// message to encrypt
  encrypt_helper(plainText, strlen(plainText));
  
  /*int plainLength = strlen(plainText);  // don't count the trailing /0 of the string!
  int paddedLength = plainLength + N_BLOCK - plainLength % N_BLOCK;      
  
  aes.iv_inc();
  byte cipherText [paddedLength];
  aes.set_IV(my_iv);
  aes.get_IV(iv);
  aes.do_aes_encrypt( (byte *) plainText,plainLength,cipherText,key,bits,iv);*/
  
  return String(plainText);
}

String decrypt(String message){
  
  char cipherText[message.length()+1];
  message.toCharArray(cipherText, message.length()+1);// message to encrypt
  encrypt_helper(cipherText, strlen(cipherText));
  
  /*int plainLength = strlen(plainText);  // don't count the trailing /0 of the string !
  int paddedLength = plainLength + N_BLOCK - plainLength % N_BLOCK;      
 
  aes.iv_inc();
  byte decrypted [paddedLength];
  aes.set_IV(my_iv);
  aes.get_IV(iv);
  aes.do_aes_decrypt(plainText,paddedLength,decrypted,key,bits,iv);*/
  
  return String(cipherText);
}

void changeName(String name) {
  name.trim();
  thisIs = name;
}

void doBuzzer() {
  analogWrite(buzzer, 128);
  delay(100);
  analogWrite(buzzer, 0);
  delay(100);
  analogWrite(buzzer, 128);
  delay(100);
  analogWrite(buzzer, 0);
}

void sendMessage(String message) {

  //if (message.length() <= 128) {
    sendBroadcast(message);
 /*   return;
  }

  int i = 0;
  while (i <= message.length()) {
    if (i + 128 < message.length()) {
      sendBroadcast(message.substring(i, i + 128));
    } else {
      delay((random(2000) + 1000));
      sendBroadcast((message.substring(i, i + message.length() + 1)));
    }
    i += 128;
  }*/

  
}

void sendBroadcast(String outgoing) {
  
  String outs=encrypt(outgoing);
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(localAddress);             // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outs.length());            // add payload length
  LoRa.print(outs);                     // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length

  String incoming = "";

  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  /*if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             // skip rest of function
  }*/

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != destination) {
    //Serial.println("This message is not for me.");
    return;                            
  }

  String imsg=decrypt(incoming); 
  
  if (imsg.indexOf("</E")>-1){
    
    doBuzzer();
    imsg=imsg.substring(0, imsg.indexOf("</E"));
    
    // if message is for this device, or broadcast, print details:
    //Serial.println("Received from: 0x" + String(sender, HEX));
    //Serial.println("Sent to: 0x" + String(recipient, HEX));
    Serial.println("Message ID: " + String(incomingMsgId));
    //Serial.println("Message length: " + String(incomingLength));
    Serial.println(imsg);
    Serial.println("RSSI: " + String(LoRa.packetRssi()));
    Serial.println("Snr: " + String(LoRa.packetSnr()));
    Serial.println();

  }
}
