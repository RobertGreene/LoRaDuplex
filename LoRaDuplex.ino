
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

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0xE2;     // address of this device
byte destination = 0xE2;      // destination to send to
long lastSendTime = 0;        // last send time
//long lastReceivedTime = 0;    // last received time
int interval = 2000;          // interval between sends
boolean sending = false;
String thisIs = "REPEATER";
const int buzzer = 5;
bool firstRun = true;
bool verb=false;

struct mdata{
  char mdata[65];
};
struct sendMessages{
  mdata messages[4];
};
sendMessages theSender;

struct sentMessages{
  mdata messages[4];
};
sentMessages theSent;

char secret_key[7] = { 28, 253, 144, 71, 77, 177, 217 };
char secret_key2[5] = { 128, 53, 14, 75, 72 };
char secret_key3[7] = { 3, 9, 19, 11, 10, 2, 7 };

bool pinging = false;
int pingInterval = 10000;
byte channels[10] = { 0xE2, 0xF7, 0xA3, 0xB9, 0xC1, 0xC3, 0xD6, 0xD8, 0xA6, 0xE5 };

void setup() {
  //pinMode(buzzer, OUTPUT);
 
  int inc=0;
  while (inc < 4){
    theSent.messages[inc].mdata[0]=char(0);
    theSent.messages[inc].mdata[1]=char(0);
    if (inc<4){
      theSender.messages[inc].mdata[0]=char(0);
      theSender.messages[inc].mdata[1]=char(0);    
    }
    inc++;
  }

  Serial.begin(19200);                   // initialize serial

  if (Serial)
    Serial.println(F("LoRa Duplex"));

  // override the default CS, reset, and IRQ pins (optional)
  LoRa.setPins(SS, RST, DI0); // set CS, reset, IRQ pin

  if (!LoRa.begin(BAND, PABOOST)) {            // initialize ratio at 915 MHz
    if (Serial)
      Serial.println(F("LoRa init failed. Check your connections."));
    while (true);                       // if failed, do nothing
  }
  if (Serial)
    Serial.println(F("LoRa init succeeded."));

}

void loop() {

  // get their handle from serial console
  if (Serial && firstRun) {
    Serial.println(F("What is your Handle/Username?"));
    while (!Serial.available());
    changeName(Serial.readString());
    firstRun = false;
    Serial.println(F(""));
    Serial.print(F("Hi "));
    Serial.print(thisIs);
    Serial.println(F(", "));
    Serial.println(F("You are setup and ready to start coms!")); 
    Serial.println(F("Type '?' for help/commands."));
  }

  // pinging out but not if its repeating first
  if (pinging && !sending) {
    if (millis() - lastSendTime > pingInterval)  {
      lastSendTime = millis();
      String pingStr=F("Ping from ");
      pingStr.concat(thisIs);
      pingStr.concat(F(" </E"));
      addToSend(encrypt(pingStr));
      Serial.println(F("Pinging out..."));
    }
  }

  // sending/repeating messages
  if (sending && ((millis() - lastSendTime) > interval))
    sender();    
  
  // send message from serial console
  if (Serial) {
    if (Serial.available()) {
      String fromSerial = Serial.readString();
      if (command_helper(fromSerial))
        return;

      String message = thisIs;
      message.concat(F(": "));
      message.concat(fromSerial);
      message.trim();
      sendMessage(message);
      
      // Testing encryption
      //Serial.println(encrypt(message));
      //Serial.println(decrypt(encrypt(message)));
    }
  }

  // parse for a packet, and call onReceive with the result:
  onReceive(LoRa.parsePacket());
}


boolean command_helper(String cmd) {

  cmd.trim();

  if (cmd.equals("?")) {
    Serial.println(F("**********HELP MENU**********"));
    Serial.println(F("ping - ping out every 10 seconds until stop command"));
    Serial.println(F("stop - use stop after various commands that repeat to stop them"));
    Serial.println(F("channel <1-10> - change the channel ie: channel 2"));
    Serial.println(F("debug - verbose: prints out log information while in use"));
    Serial.println(F("*****************************"));
    return true;
  }

  if (cmd.indexOf("channel ") == 0) {

    cmd = splitAt(cmd, "channel ");
    int str_len = cmd.length() + 1;
    char buf[str_len];
    cmd.toCharArray(buf, str_len);
    int channel = atoi(buf);
    channelSelector(channel);

    return true;
  }

  if (cmd.equals("ping")) {
    Serial.println(F("Starting ping, type 'stop' to stop pinging"));
    pinging = true;
    return true;
  }

  if (cmd.equals("stop")) {
    Serial.println(F("Stopping..."));
    pinging = false;
    return true;
  }

  if (cmd.equals("debug")) {
    Serial.println(F("Debug/Verbose mode toggled"));
    verb = !verb;    
    return true;
  }

  return false;
}

void sender() {

  // pop off the stack
  int inc = 0;
  while (inc < 4) {
    if(theSender.messages[inc].mdata[0]!=char(0) ){
      String checkSend = String((char *)theSender.messages[inc].mdata);        
      if (!checkSend.equals("")) {
        lastSendTime = millis();
        interval = random(2000) + 1500;
        if (Serial && verb){
          Serial.print(F("Sending #"));
          Serial.print(inc);
          Serial.print(F(" "));
          Serial.println((char *)theSender.messages[inc].mdata);
        }
        addToSent(checkSend);
        sendBroadcast(checkSend);
        theSender.messages[inc].mdata[0]=char(0);
        theSender.messages[inc].mdata[1]=char(0);        
        return;
      }
    }
    inc++;
  }
  
  if (Serial && verb)
    Serial.println(F("Stopping Sender"));
  sending = false;
}

void addToSend(String incoming) {

  // add to stack if not already in the repeat list
  int inc = 0;
  boolean found = false;
  while (inc < 4) {    
    if (theSender.messages[inc].mdata[0]!=char(0)){
      String checkSend = String((char *)theSender.messages[inc].mdata);    
      if (checkSend.equals(incoming))
        found = true;
    }
    inc++;
  }
  
  // already in the stack
  if (found)
    return;

  sending = true;

  // add to the stack if there is a spot available
  inc = 0;
  while (inc < 4) {
    if (theSender.messages[inc].mdata[0]==char(0)){
      // add to free spot   
      incoming.toCharArray(theSender.messages[inc].mdata, 64);   
      if (Serial && verb){
        Serial.print(F("Added to Send List #"));
        Serial.print(inc);
        Serial.print(F(" ")); 
        Serial.println((char *)theSender.messages[inc].mdata);
      }      
      return;      
    }
    inc++;
  }
     
}

void addToSent(String outs) {

  // add to the stack if not already in the sent list
  int inc = 0;
  boolean found = sentAlready(outs);

  // already in the stack
  if (found)
    return;

  // add to the stack if there is a spot available
  inc = 0;
  while (inc < 4) {
    if (theSent.messages[inc].mdata[0]==char(0)){
      // add to free spot
      outs.toCharArray(theSent.messages[inc].mdata, 65);
      if (Serial && verb){
        Serial.print(F("Added to sent list #"));
        Serial.print(inc);
        Serial.print(F(" "));
        Serial.println((char *)theSent.messages[inc].mdata);
      }
      found = true;
      return;
    }
    inc++;
  }

  if (!found){
    outs.toCharArray(theSent.messages[0].mdata, 65);
    if (Serial && verb){
      Serial.print(F("Added to sent list #"));
      Serial.print(F("0 "));
      Serial.println((char *)theSent.messages[0].mdata);
    }
   
    theSent.messages[1] = theSent.messages[3];
    if (Serial && verb){
      Serial.print(F("Added to sent list #"));
      Serial.print(F("1 "));
      Serial.println((char *)theSent.messages[1].mdata);
    }
    inc = 2;      
    while (inc <4) {
      theSent.messages[inc].mdata[0]=char(0);
      theSent.messages[inc].mdata[1]=char(0);
      inc++;     
    }
  }
  
}

boolean sentAlready(String checkStr){
  int inc = 0;
  while (inc < 4) {    
    if (theSent.messages[inc].mdata[0]!=char(0)){
      String checkSend = String((char *)theSent.messages[inc].mdata);    
      if (checkSend.equals(checkStr))
        return true;
    }
    inc++;
  }
  return false;
}

void channelSelector(int channel) {
  if (channel > 10)
    channel = 10;
  if (channel <= 0)
    channel = 1;

  channel = channel - 1;

  localAddress = channels[channel];
  destination = channels[channel];
  Serial.print(F("Channel changed to "));
  Serial.println((channel + 1));
}

String splitAt(String d1, String delim) {
  int pos = d1.indexOf(delim) + delim.length();
  if (pos < -1)
    return "";
  return d1.substring(pos);
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
  String lastSent = "";
  if (message.length() <= 61) {
    lastSent=message;
    Serial.println(lastSent);
    lastSent.concat("</E");
    addToSend(encrypt(lastSent));
    return;
  }

  message = splitAt(message, thisIs+": ");
  
  int i = 0;
  int inc = 1;
  while (i <= message.length()) {
    String pre = "Part #";
    pre.concat(inc);
    pre.concat(" - ");
    pre.concat(thisIs);
    pre.concat(": ");
    lastSent = pre;
    
    if (i + 64 <= message.length()) {
      lastSent.concat(message.substring(i, i + 64));      
      Serial.println(lastSent);
      lastSent.concat(F("</E"));      
      addToSend(encrypt(lastSent));    
    } else {
      lastSent.concat(message.substring(i, i + message.length()+1));
      lastSent.concat(F(" (*END OF "));
      lastSent.concat(inc);
      lastSent.concat(F(" PARTS*)"));
      Serial.println(lastSent);
      lastSent.concat(F("</E"));      
      addToSend(encrypt(lastSent));   
    }
    
    i += 64;
    inc++;
  }


}

void sendBroadcast(String outs) {
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

  while (LoRa.available()) 
    incoming += (char)LoRa.read();

  /*if (incomingLength != incoming.length()) {   // check length for error
    Serial.println("error: message length does not match length");
    return;                             // skip rest of function
    }*/

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != destination) {
    //Serial.println("This message is not for me.");
    return;
  }

  String imsg = decrypt(incoming);
  
  if (imsg.indexOf("</E") > -1) {

    //doBuzzer();
    imsg = imsg.substring(0, imsg.indexOf("</E"));

    // if message is for this device, or broadcast, print details:
    //Serial.println("Received from: 0x" + String(sender, HEX));
    //Serial.println("Sent to: 0x" + String(recipient, HEX));
    //Serial.println("Message ID: " + String(incomingMsgId));
    //Serial.println("Message length: " + String(incomingLength));

    // check to see if this message is in the 'to repeat' list
    boolean show = true;
    
    // repeater filter
    if (!sentAlready(incoming)) {      
      if (imsg.indexOf("Ping from ")!=0){
        addToSend(incoming);
        //lastReceivedTime = millis();
      }            
    } else {
      // do not show repeated message if it was posted under 14 seconds ago
      //if ((millis() - lastReceivedTime) < 14000)
        show = false;
    }   

    if (Serial && show) {
      Serial.print(F("[RSSI: "));
      Serial.print(String(LoRa.packetRssi()));
      Serial.print(F("]"));
      Serial.print(F(" [SNR: ")); 
      Serial.print(String(LoRa.packetSnr()));
      Serial.println(F("]"));
      Serial.println(imsg);
    }

  }
}
