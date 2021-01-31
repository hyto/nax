#include <Arduino.h>
#include <avr/wdt.h>
#include <EEPROM.h>

// #define PJON_PACKET_MAX_LENGTH     64

#include <PJON.h>

// Define el nivel de depuracion para la consola serial
uint8_t DEBUG_LEVEL = 0;
#define MAX_NAX_PACKET_SIZE 40
#define MAX_NAX_RESONSE_SIZE 40
#define EE_IO_INFO_BASE 100
#define EE_DATA_BASE 512

#ifndef MAX_IOMAP_SIZE 
#define MAX_IOMAP_SIZE 8
#endif 
uint8_t pinMap[MAX_IOMAP_SIZE];

#define RESET_PIN 13

//typedef void (*rpc_callback)(const JsonArray);

struct NaxCommand {
private:
  char *m_orig_cmd;
  char m_result[MAX_NAX_RESONSE_SIZE]; // respuesta en cadena para ser usado en la devolucion de una llamada remota por ejemplo
  // char* m_result; //[MAX_NAX_RESONSE_SIZE]; // respuesta en cadena para ser usado en la devolucion de una llamada remota por ejemplo
  int m_errno; // nro de error donde 0 es sin error
public:
  NaxCommand();
  int argc;
  char *args[10];
  int parseCmd(char* params); 
  void showArgs();
  int ArgAsInt(int index);
  char *ArgAsChar(int index);
  double ArgAsFloat(int index);

  int AsInt(int index);
  char *AsChar(int index);
  double AsFloat(int index);

  char *command();
  int deviceId();
  int callId();

  int setResult(char* data);
  int setResult(int value);
  void setErrno(int _errno);
  char *result();
  int getErrno();

  char *originalCommand();
  // String naxResult();
  char *naxResult();
};

 NaxCommand::NaxCommand(){
   m_errno = 0;
 }

char *NaxCommand::originalCommand(){
  return m_orig_cmd;
}

int NaxCommand::parseCmd(char* params){
  m_orig_cmd = params;
  const char d[2] = " ";
  char *token;
  int i = 0;
  argc = 0;
  token = strtok(params, d);
  while(token != NULL) {
    args[i] = token;
    if(DEBUG_LEVEL > 0) {
      Serial.print(F("token: "));
      Serial.println(token);
    }
    token = strtok(NULL, d);
    i++;
    argc = i;
  }
  return argc;
}

char *NaxCommand::result(){
  return m_result;
}

int NaxCommand::getErrno(){
  return m_errno;
}

/**
 * header:version:devid:callid:data
 * ej: nax:1:10:0:io=7,temp=34.5
 */
char* NaxCommand::naxResult(){
  return m_result;
}

int NaxCommand::setResult(char* data){  
  snprintf(m_result, MAX_NAX_RESONSE_SIZE, "%s:%s:%s:%s", getErrno() >= 0 ? "r" : "e", 
    args[1], args[2], data);
  return strlen(m_result);
}

int NaxCommand::setResult(int value){  
  char r[10];
  snprintf(r, 10, "%d", value);
  setResult(r);
  return strlen(m_result);
}

void NaxCommand::setErrno(int _errno){
  m_errno = _errno;
}

int NaxCommand::ArgAsInt(int index){
  return atoi(args[index]);
}

char *NaxCommand::ArgAsChar(int index){
  return args[index];
}

double NaxCommand::ArgAsFloat(int index){
  return atof(args[index]);
}

int NaxCommand::AsInt(int index){
  return ArgAsInt(index + 3);
}

char *NaxCommand::AsChar(int index){
  return ArgAsChar(index + 3);
}

double NaxCommand::AsFloat(int index) {
  return ArgAsFloat(index + 3);
}

char *NaxCommand::command(){
  return args[0];
}

int NaxCommand::deviceId(){
  return ArgAsInt(1); 
}
  
int NaxCommand::callId(){
  return ArgAsInt(2);
}

void NaxCommand::showArgs(){
  for(int i = 0; i < argc; i++) {
    Serial.print(F("arg["));
    Serial.print(i);
    Serial.print(F("] = "));
    Serial.println(args[i]);
  }
}

typedef void(*rpc_callback)(NaxCommand&);
struct NaxCallback {
  char name[7];
  rpc_callback callback;
};

int registerd_callbacks = 0;
#ifndef MAX_NAX_CALLBACKS
#define MAX_NAX_CALLBACKS 32
#endif
rpc_callback nax_callbacks[MAX_NAX_CALLBACKS];
String nax_callbacks_names[MAX_NAX_CALLBACKS];
// NaxCallback _callbacks[6];
// String test_array[32]; ~10 bytes por objeto String

#define NAX_ID_MAIN 1
#define NAX_ID_MAIN_GATE 2
#define NAX_ID_OFFICE_LIGHTS 3

#ifndef PJON_PIN
#define PJON_PIN 12
#endif

// #define STATIC_DOCUMENT_SIZE 128
#ifndef MAX_BUFF_FRAME_SIZE
  #define MAX_BUFF_FRAME_SIZE 40
#endif

void getee(NaxCommand &cmd);
void setee(NaxCommand &cmd);
void dataee(NaxCommand &cmd);
void setdataee(NaxCommand &cmd);
void debugl(NaxCommand &cmd);
void resetdev(NaxCommand &cmd);

void init_callback() {
  for(int i = 0; i < MAX_NAX_CALLBACKS; i++) {
    nax_callbacks[i] = NULL;
    nax_callbacks_names[i] = "";
  }
}

void register_callback(String name, rpc_callback callback) {
  if (registerd_callbacks < MAX_NAX_CALLBACKS) {
    nax_callbacks_names[registerd_callbacks] = name;
    nax_callbacks[registerd_callbacks] = callback;
    registerd_callbacks++;
    if(DEBUG_LEVEL > 0) {
      Serial.print(F("registrado callback: "));
      Serial.print(registerd_callbacks - 1);
      Serial.print(F(":"));
      Serial.println(name);
    }
  }
  else
  {
    Serial.print(F("ERROR register_callback, numero maximo alcanzado: "));
    Serial.println(name);
  }
}

// void(*resetDevice)(void) = 0;

void reboot() {
  wdt_disable();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

bool nax_basic_callback(NaxCommand &cmd) {
  if(strcmp("getee", cmd.command()) == 0) {
    getee(cmd);
    return true;
  }
  if(strcmp("setee", cmd.command()) == 0) {
    setee(cmd);
    return true;
  }
  if(strcmp("dataee", cmd.command()) == 0) {
    dataee(cmd);
    return true;
  }
  if(strcmp("setdataee", cmd.command()) == 0) {
    setdataee(cmd);
    return true;
  }
  if(strcmp("debugl", cmd.command()) == 0) {
    debugl(cmd);
    return true;
  }  
  if(strcmp("resetdev", cmd.command()) == 0) {
    // resetDevice();
    resetdev(cmd);
    delay(1000);
    // digitalWrite(RESET_PIN, LOW);
    reboot();
    return true;
  }


  return false;
}

void do_callback(NaxCommand &command) {
  // callbacks basicas de nax
  if(nax_basic_callback(command))
    return;

  for(int i = 0; i < registerd_callbacks; i++) {
    if(DEBUG_LEVEL > 0) {
      Serial.print(F("do_callback: "));
      Serial.print(nax_callbacks_names[i]);
      Serial.print(F(" vs "));
      Serial.println(command.command());
    }    
    if(nax_callbacks_names[i] == command.command()){
      nax_callbacks[i](command);
      return;
    }
    char s[15];
    snprintf(s, 14, "%s not found", command.command());
    command.setErrno(-1);
    command.setResult(s);
  } 
}


class NaxLib {
  private:
    int m_busid;
    int m_pin;
    char frameBuff[MAX_BUFF_FRAME_SIZE];
    uint16_t frameBuffLength; 
    bool readingFrame = false;
  public: 
    PJON<SoftwareBitBang> m_bus;
    NaxLib(int pin, int busid);
    void init();
    void connect();
    int deviceId();
    // void receiver(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info);  
    void set_receiver(PJON_Receiver r);
    void update();
    void reportIO(String s);
    void testPJON(int device_id);
    void readIoMapFromStore();
    void writeIoMapToStore(); 

    // manejo de entrada de datos seriales
    void serialUpdate();
    void beginFrame(); 
    void endFrame();
    void addCharToFrame(char ch);
    void processFrame();
    void manageNaxRpc(NaxCommand &cmd);
    void forwardRpcCall(int device_id, char *buff);
};

NaxLib::NaxLib(int pin, int busid) {
  m_pin = pin;
  m_busid = busid;

  m_bus.strategy.set_pin(pin);
  m_bus.set_id(busid);
}

int NaxLib::deviceId(){
  return m_busid;
}

void NaxLib::set_receiver(PJON_Receiver r) {
  m_bus.set_receiver(r);
}

bool startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

// hack auxiliar mientras encuentro como responder desde receive desde un objeto
NaxLib *_nax;

void nax_default_receiver(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info) {
  char buff[length + 1];
  for(uint16_t i = 0; i < length; i++) {
    buff[i] = (char)payload[i];
  }
  buff[length] = '\0';

  Serial.print(F("msg: "));
  Serial.println(buff);

  if(strncmp("c:", buff, 2) == 0) {
    NaxCommand cmd;
    cmd.parseCmd(buff + 2);
    do_callback(cmd);
    String response = cmd.naxResult();
    // #ifndef ESP_H
    _nax->m_bus.send(packet_info.sender_id, response.c_str(), response.length());
    // #endif
  }
}


void NaxLib::init() {
  digitalWrite(RESET_PIN, HIGH);
  delay(200);
  pinMode(RESET_PIN, OUTPUT);

  Serial.begin(115200);
  set_receiver(nax_default_receiver);
  connect();
  _nax = this;
}


void NaxLib::connect(){
  Serial.println(F("Iniciando PJON"));
 // m_bus.set_receiver(receiver);
  m_bus.begin();
}

void NaxLib::update() {
  m_bus.receive(1000);
  m_bus.update();
}


// void NaxLib::receiver(uint8_t *payload, uint16_t length, const PJON_Packet_Info &packet_info) {

// }

void NaxLib::reportIO(String s) {
  m_bus.send(1, s.c_str(), s.length());
}

void NaxLib::writeIoMapToStore() {
  for(int i = 0; i < MAX_IOMAP_SIZE; i++) {
    #ifndef ESP_H
    EEPROM.update(EE_IO_INFO_BASE + i, pinMap[i]);
    #else
    EEPROM.write(IO_INFO_BASE + i, pinMap[i]);
    #endif

  }
}

void NaxLib::readIoMapFromStore() {
  for(int i = 0; i < MAX_IOMAP_SIZE; i++){
    pinMap[i] = EEPROM.read(EE_IO_INFO_BASE + i);
  }
}


/// serial shit
void NaxLib::serialUpdate(){
  while(Serial.available() > 0) {
    char ch = Serial.read();

    if (readingFrame)
      addCharToFrame(ch);
    else if(ch == '>') {
      beginFrame();
    }   
  }
}

void NaxLib::beginFrame() {
  frameBuff[0] = '\0';
  frameBuffLength = 0;
  readingFrame = true;
}

void NaxLib::endFrame() {
  readingFrame = false;
  Serial.println(F("Frame terminado"));
  frameBuff[frameBuffLength] = '\0';
  // Serial.print(frameBuff);
  processFrame();
  frameBuffLength = 0;
}

void NaxLib::addCharToFrame(char ch) {
  if(ch == '<') {
    endFrame();
  } 
  else {
    frameBuff[frameBuffLength] = ch; 
    frameBuffLength += 1; // frameBuffLength lleva la posicion y longitud, se utiliza como indice para asignar el caracter antes de incrementar el largo del buffer
  }
}

void NaxLib::processFrame() {
  NaxCommand cmd;
  char _buff[MAX_BUFF_FRAME_SIZE];

  // copio la cadena porque strtok en cmd.parse modifica la cadena
  strncpy(_buff, frameBuff, MAX_BUFF_FRAME_SIZE);
  _buff[MAX_BUFF_FRAME_SIZE - 1] = '\0';

  if(strncmp(_buff, "c:", 2) == 0)
    cmd.parseCmd(_buff + 2);
  else
    cmd.parseCmd(_buff);
    
  if(cmd.deviceId() == m_busid) {
    do_callback(cmd);
    Serial.println(cmd.naxResult());
  }
  else {
    Serial.println("Reenviando llamada a dispositivo");
    Serial.println(cmd.deviceId());
    forwardRpcCall(cmd.deviceId(), frameBuff);
  }
}


void NaxLib::manageNaxRpc(NaxCommand &cmd){
  do_callback(cmd);
}

void NaxLib::forwardRpcCall(int device_id, char *buff) {
  if(device_id != 0) {
    Serial.print("Reenviando: ");
    Serial.println(buff);
    m_bus.send(device_id, buff, strlen(buff));
  }
}

void NaxLib::testPJON(int device_id) {
  String s = "{hola}";
  m_bus.send(10, s.c_str(), s.length());  
}

uint8_t getee(int addr) {
  return EEPROM.read(addr);
}

void setee(int addr, uint8_t value) {
  uint8_t v = getee(addr);
  if(value == v) 
    return;
  #ifndef ESP_H
  EEPROM.update(addr, value);
  #else
  EEPROM.write(IO_INFO_BASE + addr, value);
  #endif
}

void setee(NaxCommand &cmd) {
  uint8_t value = cmd.AsInt(1);
  int addr = cmd.AsInt(0);

  setee(addr, value);
  char r[10];
  snprintf(r, 10, "%d=%d", addr, value);
  // cmd.setErrno(0);
  cmd.setResult(r);
}


void resetdev(NaxCommand &cmd) {
  cmd.setResult(1000);  
}

void debugl(NaxCommand &cmd) {
  uint8_t l = cmd.AsInt(0);
  DEBUG_LEVEL = l;
  cmd.setResult(DEBUG_LEVEL);  
}

void getee(NaxCommand &cmd) {
  char r[10];
  uint8_t b = getee(cmd.AsInt(0));
  snprintf(r, 10, "%d", b);  
  cmd.setResult(r);
}

uint8_t dataee(int _addr) {
  int addr = EE_DATA_BASE + _addr;
  uint8_t b = getee(addr);
  if(DEBUG_LEVEL > 0) {
    Serial.print(F("dataee @"));
    Serial.print(addr);
    Serial.print(F(" = "));
    Serial.println(b);
  }
  return b;
}

void dataee(NaxCommand &cmd){
  uint8_t b = getee(cmd.AsInt(0)); 

  char r[10];
  snprintf(r, 10, "%d", b);
  cmd.setResult(r);
}

void setdataee(NaxCommand &cmd) {
  // escribo el valor (setee escribe solo si el valor es distinto)
  int addr = EE_DATA_BASE + cmd.AsInt(0);
  uint8_t value = cmd.AsInt(1);
  setee(addr, value);

  // leo el valor escrito y es el que devuelvo
  uint8_t b = getee(addr);
  if(DEBUG_LEVEL > 0) {
    Serial.print(F("dataee @"));
    Serial.print(addr);
    Serial.print(F(" = "));
    Serial.println(b);
  }

  char r[10];
  snprintf(r, 10, "%d", b);
  cmd.setResult(r);
}



void setPinMap(int index, uint8_t value, uint8_t save = 0) {
  pinMap[index] = value;
  if(save > 0) {
    setee(EE_IO_INFO_BASE + index, value);    
  }
}

void setpm(NaxCommand &cmd) {

}
