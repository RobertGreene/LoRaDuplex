
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

String outgoing;              // outgoing message
byte msgCount = 0;            // count of outgoing messages
byte localAddress = 0xE2;     // address of this device
byte destination = 0xE2;      // destination to send to
long lastSendTime = 0;        // last send time
//long lastReceivedTime = 0;    // last received time
int interval = 2000;          // interval between sends
boolean sending = false;
String thisIs;
bool firstRun = true;
bool verb=false;

struct mdata{
  char mdata[65];
  bool sent = true;
  bool heard = false;
};
struct sendMessages{
  mdata messages[8];  
};
sendMessages theSender;

char secret_key[7] = { 28, 253, 144, 71, 77, 177, 217 };
char secret_key2[5] = { 128, 53, 14, 75, 72 };
char secret_key3[7] = { 3, 9, 19, 11, 10, 2, 7 };

bool pinging = false;
int pingInterval = 10000;
byte channels[10] = { 0xE2, 0xF7, 0xA3, 0xB9, 0xC1, 0xC3, 0xD6, 0xD8, 0xA6, 0xE5 };

void setup() {

  int inc=0;
  while (inc < 8){
    theSender.messages[inc].mdata[0]=F(" ");
    theSender.messages[inc].mdata[1]=char(0);
    theSender.messages[inc].sent = true;    
    inc++;
  }
  thisIs = F("REPEAT");

  Serial.begin(19200);                   // initialize serial

  if (Serial)
    Serial.println(F("LoRa RGDuplex"));

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

  if (cmd.equals(F("?"))) {
    Serial.println(F("**********HELP MENU**********"));
    Serial.println(F("ping - ping out every 10 seconds until stop command"));
    Serial.println(F("stop - stop pinging"));
    Serial.println(F("handle <name> - change your name/handle"));
    Serial.println(F("channel <1-10> - change the channel ie: channel 2"));
    Serial.println(F("heard - prints out messages heard by other devices"));
    Serial.println(F("debug - verbose: prints out log information while in use"));
    Serial.println(F("*****************************"));
    return true;
  }

  if (cmd.indexOf(F("channel ")) == 0) {
    cmd = splitAt(cmd, F("channel "));
    int channel = cmd.toInt();
    channelSelector(channel);
    return true;
  }

  if (cmd.indexOf(F("handle ")) == 0) {
    cmd = splitAt(cmd, F("handle "));
    changeName(cmd);
    Serial.print(F("Changed handle/name to "));
    Serial.println(cmd);
    return true;
  }

  if (cmd.equals(F("ping"))) {
    Serial.println(F("Starting ping, type 'stop' to stop pinging"));
    pinging = true;
    return true;
  }
  
  if (cmd.equals(F("heard"))) {
    Serial.println(F("Messages that were heard by other devices: "));
    int inc = 0;
    while (inc < 8){
      if (theSender.messages[inc].sent && theSender.messages[inc].heard){
        String heard = String((char *)theSender.messages[inc].mdata);
        Serial.print(F("Message #"));
        Serial.print(inc);
        Serial.print(F(" - "));
        Serial.println(decrypt(heard).substring(0, decrypt(heard).indexOf(F("</E")) ));
      }
      inc++;
    }
    Serial.println("End of heard messages");      
    return true;
  }

  if (cmd.equals(F("stop"))) {
    Serial.println(F("Stopping ping..."));
    pinging = false;
    return true;
  }

  if (cmd.equals(F("debug"))) {
    Serial.println(F("Debug/Verbose mode toggled"));
    verb = !verb;    
    return true;
  }

  return false;
}

void sender() {
  // pop off the stack
  int inc = 0;
  while (inc < 8) {
    if(!theSender.messages[inc].sent){
      String checkSend = String((char *)theSender.messages[inc].mdata);
      lastSendTime = millis();
      interval = random(3000) + 2000;
      if (Serial && verb){
        Serial.print(F("Sending #"));
        Serial.print(inc);
        Serial.print(F(" "));
        Serial.println(decrypt((char *)theSender.messages[inc].mdata));
      }
      sendBroadcast(checkSend);
      theSender.messages[inc].sent = true;       
      return;
    }
    inc++;
  }
  
  if (Serial && verb)
    Serial.println(F("Stopping Sender"));
  sending = false;
}

void addToSend(String incoming) {
  int inc = 0;
  // add to the stack if there is a spot available
  inc = 0;
  while (inc < 8) {
    if (theSender.messages[inc].sent){
      // add to free spot   
      incoming.toCharArray(theSender.messages[inc].mdata, 65);   
      if (Serial && verb){
        Serial.print(F("Added to Send List #"));
        Serial.print(inc);
        Serial.print(F(" ")); 
        Serial.println(decrypt((char *)theSender.messages[inc].mdata));
      }
      sending = true;
      theSender.messages[inc].sent = false;
      theSender.messages[inc].heard = false;      
      return;      
    }
    inc++;
  }     
}

boolean sentAlready(String checkStr){
  int inc = 0;
  while (inc < 8) {    
    String checkSend = String((char *)theSender.messages[inc].mdata);     
    if (checkSend.equals(checkStr)){
      theSender.messages[inc].heard = true;
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

void sendMessage(String message) {
  String lastSent = F("");
  if (message.length() <= 47) {
    lastSent=message;
    Serial.println(lastSent);
    lastSent.concat(F("</E"));
    addToSend(encrypt(lastSent));
    addToSend(encrypt(lastSent));
    addToSend(encrypt(lastSent)); 
    return;
  }

  message = splitAt(message, thisIs + F(": "));
  
  int i = 0;
  int inc = 1;
  while (i <= message.length()) {
    String pre = F("Part #");
    pre.concat(inc);
    pre.concat(F(" - "));
    pre.concat(thisIs);
    pre.concat(F(": "));
    lastSent = pre;
    
    if (i + 37 < message.length()) {
      lastSent.concat(message.substring(i, i + 37));      
      Serial.println(lastSent);
      lastSent.concat(F("</E"));      
      addToSend(encrypt(lastSent));
      addToSend(encrypt(lastSent));    
    } else {
      lastSent.concat(message.substring(i, i + message.length()+1));
      lastSent.concat(F(" (*END OF "));
      lastSent.concat(inc);
      lastSent.concat(F(" PARTS*)"));
      Serial.println(lastSent);
      lastSent.concat(F("</E"));      
      addToSend(encrypt(lastSent));
      addToSend(encrypt(lastSent));    
    }
    
    i += 37;
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
  while (LoRa.available()) {
     if (incomingLength>65)
        char c = (char)LoRa.read();
     else
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

  String imsg = decrypt(incoming);
  
  if (imsg.indexOf(F("</E")) > -1) {

    //doBuzzer();
    imsg = imsg.substring(0, imsg.indexOf(F("</E")));

    // if message is for this device, or broadcast, print details:
    //Serial.println("Received from: 0x" + String(sender, HEX));
    //Serial.println("Sent to: 0x" + String(recipient, HEX));
    //Serial.println("Message ID: " + String(incomingMsgId));
    //Serial.println("Message length: " + String(incomingLength));

    // check to see if this message is in the 'to repeat' list
    boolean show = true;
    
    // repeater filter
    if (!sentAlready(incoming)) {      
      if (imsg.indexOf(F("Ping from "))!=0){
        addToSend(incoming);
        //lastReceivedTime = millis();
      }            
    } else {
      // do not show repeated message if it was posted under 14 seconds ago
      //if ((millis() - lastReceivedTime) < 14000)
        show = false;
        if (Serial && verb){
          Serial.println(F("Another device heard and repeated:"));
          Serial.println(imsg);
        }
          
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
