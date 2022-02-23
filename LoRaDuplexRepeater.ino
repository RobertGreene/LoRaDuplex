
#include <SPI.h>              // include libraries
#include <LoRa.h>

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

byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0xE2;     // address of this device
long lastSendTime = 0;        // last send time
int interval = 2000;          // interval between sends
boolean sending = false;
bool verb=true;

struct mdata{
  char mdata[65];
  bool sent = true;
  byte destination = 0xE2;
};
struct sendMessages{
  mdata messages[12];  
};
sendMessages theSender;

char secret_key[7] = { 28, 253, 144, 71, 77, 177, 217 };
char secret_key2[5] = { 128, 53, 14, 75, 72 };
char secret_key3[7] = { 3, 9, 19, 11, 10, 2, 7 };

void setup() {

  int inc=0;
  while (inc < 12){
    theSender.messages[inc].mdata[0]=F(" ");
    theSender.messages[inc].mdata[1]=char(0);
    theSender.messages[inc].sent = true;    
    inc++;
  }

  Serial.begin(19200);                   // initialize serial

  if (Serial)
    Serial.println(F("LoRa Duplex Repeater"));

  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(SS, RST, DI0); // set CS, reset, IRQ pin
  LoRa.setTxPower(20); //20dB output

  if (!LoRa.begin(BAND, PABOOST)) {            // initialize ratio at 915 MHz
    if (Serial)
      Serial.println(F("LoRa init failed. Check your connections."));
    while (true);                       // if failed, do nothing
  }
  if (Serial)
    Serial.println(F("LoRa init succeeded."));

}

void loop() {
  
  // sending/repeating messages
  if (sending && ((millis() - lastSendTime) > interval))
    sender();

  // parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());
}

void sender() {

  // pop off the stack
  int inc = 0;
  while (inc < 12) {
    if(!theSender.messages[inc].sent){
      String checkSend = String((char *)theSender.messages[inc].mdata);        

      lastSendTime = millis();
      interval = random(1000) + 2000;
      if (Serial && verb){
        Serial.print(F("Sending #"));
        Serial.print(inc);
        Serial.print(F(" "));
        Serial.println((char *)theSender.messages[inc].mdata);
      }
      sendBroadcast(checkSend, theSender.messages[inc].destination);
      theSender.messages[inc].sent = true;       
      return;

    }
    inc++;
  }
  
  if (Serial && verb)
    Serial.println(F("Stopping Sender"));
  sending = false;
}

void addToSend(String incoming, byte destination) {

  int inc = 0;
  // add to the stack if there is a spot available
  while (inc < 12) {
    if (theSender.messages[inc].sent){
      // add to free spot   
      incoming.toCharArray(theSender.messages[inc].mdata, 65);   
      if (Serial && verb){
        Serial.print(F("Added to Send List #"));
        Serial.print(inc);
        Serial.print(F(" ")); 
        Serial.println((char *)theSender.messages[inc].mdata);
      }
      theSender.messages[inc].sent = false;
      theSender.messages[inc].destination = destination;
      sending = true;      
      return;      
    }
    inc++;
  }
     
}

boolean sentAlready(String checkStr){
  int inc = 0;
  while (inc < 12) {    
    String checkSend = String((char *)theSender.messages[inc].mdata);    
    if (checkSend.equals(checkStr))
      return true;
    inc++;
  }
  return false;
}

void encrypt_helper(char *array, int array_size, char s_key[]) {
  int i;
  int ic = 0;
  for (i = 0; i < array_size; i++) {
    ic++;
    if (ic > sizeof(s_key) - 1)
      ic = 0;
    array[i] ^= s_key[ic];
  }
}

String encrypt(String message) {

  char plainText[message.length() + 1];
  message.toCharArray(plainText, message.length() + 1); // message to encrypt
  encrypt_helper(plainText, strlen(plainText), secret_key);
  encrypt_helper(plainText, strlen(plainText), secret_key2);
  encrypt_helper(plainText, strlen(plainText), secret_key3);

  return String(plainText);
}

String decrypt(String message) {

  char cipherText[message.length() + 1];
  message.toCharArray(cipherText, message.length() + 1); // message to decrypt
  encrypt_helper(cipherText, strlen(cipherText), secret_key3);
  encrypt_helper(cipherText, strlen(cipherText), secret_key2);
  encrypt_helper(cipherText, strlen(cipherText), secret_key);

  return String(cipherText);
}

void sendBroadcast(String outs, byte destination) {
  LoRa.beginPacket();                   // start packet
  LoRa.write(destination);              // add destination address
  LoRa.write(destination);              // add sender address
  LoRa.write(msgCount);                 // add message ID
  LoRa.write(outs.length());            // add payload length
  LoRa.print(outs);                     // add payload
  LoRa.endPacket();                     // finish packet and send it
  msgCount++;                           // increment message ID
}

void onReceive(int packetSize) {
  if (packetSize == 0) 
    return;          // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();          // recipient address
  byte sender = LoRa.read();            // sender address
  byte incomingMsgId = LoRa.read();     // incoming msg ID
  byte incomingLength = LoRa.read();    // incoming msg length
  
  String incoming = "";
  while (LoRa.available()) {
     if (incomingLength>65)
        char c = (char)LoRa.read();
     else
        incoming += (char)LoRa.read();
  }
  String imsg = decrypt(incoming);
  
  if (imsg.indexOf(F("</E")) > -1) {
    // repeater filter
    if (!sentAlready(incoming)) {      
      if (imsg.indexOf(F("Ping from "))!=0){
        addToSend(incoming, sender);
        addToSend(incoming, sender);
      }            
    } else{
      if (Serial && verb)
         Serial.println(F("Another device heard and repeated a message we repeated"));      
    }
  }  
}
