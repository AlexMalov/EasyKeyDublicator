#include <OneWire.h>
#include "musical_notes.h"

#define iButtonPin A5      // Линия data ibutton
#define BtnPin 6           // Кнопка переключения режима чтение/запись
#define BtnPinGnd 7        // Земля кнопки переключения режима чтение/запись
#define speakerPin 8       // Спикер, он же buzzer, он же beeper
#define speakerPinGnd 9    // Спикер, он же buzzer, он же beeper
#define R_Led 2
#define G_Led 3
#define B_Led 4

OneWire ibutton (iButtonPin); 

byte addr[8];
byte ReadID[8];                           // массив с данными для записи
bool readflag = false;                    // флаг сигнализируе, что данные с ключа успечно прочианы в ардуино
bool writeflag = false;                   // режим запись/чтение
bool preBtnPinSt = HIGH;

void setup() {
  pinMode(BtnPin, INPUT_PULLUP);                            // включаем чтение и подягиваем пин кнопки режима к +5В
  pinMode(BtnPinGnd, OUTPUT); digitalWrite(BtnPinGnd, LOW); // подключаем второй пин кнопки к земле
  pinMode(speakerPin, OUTPUT);
  pinMode(speakerPinGnd, OUTPUT); digitalWrite(speakerPinGnd, LOW); // подключаем второй пин спикера к земле
  pinMode(R_Led, OUTPUT);
  pinMode(G_Led, OUTPUT);
  pinMode(B_Led, OUTPUT);
  clearLed();
  digitalWrite(B_Led, HIGH);                                //awaiting of origin key data
  Serial.begin(115200);
  Sd_StartOK();
}

void clearLed(){
  digitalWrite(R_Led, LOW);
  digitalWrite(G_Led, LOW);
  digitalWrite(B_Led, LOW);  
}

byte getRWtype(){
  // 0 это неизвестный тип RW1990
  // 1 это RW-1990.1
  // 2 это RW-1990.2
  ibutton.reset(); ibutton.write(0xD1); // проуем снять флаг записи для RW-1990.1
  ibutton.write_bit(1);                 // записываем значение флага записи = 1 - отключаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xB5); // send 0xB5 - запрос на чтение флага записи
  byte answer = ibutton.read();
  Serial.print("\n Answer: "); Serial.println(answer, HEX);
  if (answer == 0xFE){
    Serial.println("Type: RW-1990.1 ");
    return 1; // это RW-1990.1
  }
  ibutton.reset(); ibutton.write(0x1D);  // проуем установить флаг записи для RW-1990.2 
  ibutton.write_bit(1);                  // записываем значение флага записи = 1 - включаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0x1E);  // send 0x1E - запрос на чтение флага записи
  answer = ibutton.read();
  Serial.print("\n Answer2: "); Serial.println(answer, HEX);
  if (answer == 0xFE){
    ibutton.reset(); ibutton.write(0x1D); // возвращаем оратно запрет записи для RW-1990.2
    ibutton.write_bit(0);                 // записываем значение флага записи = 0 - выключаем запись
    delay(10); pinMode(iButtonPin, INPUT);
    Serial.println("Type: RW-1990.2 ");
    return 2; // это RW-1990.2
  }
  Serial.println("Type: Unknown! ");
  return 0;                              // это неизвестный тип DS1990
}

bool write2iBtnRW1990.1(){
  if (!ibutton.reset()) return false;
  ibutton.write(0xD1);                    // send 0xD1 - флаг записи
  ibutton.write_bit(0);                   // записываем значение флага записи = 0 - разрешить запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xD5);   // команда на запись
  for (byte i = 0; i<8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    BurnByte(ReadID[i]);
    Serial.print('*');
    Sd_WriteStep();
  } 
  ibutton.write(0xD1);    // send 0xD1 - флаг записи
  ibutton.write_bit(1);   // записываем значение флага записи = 1 - отключаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  return true;
}

bool write2iBtnRW1990.2(){
  if (!ibutton.reset()) return false;
  ibutton.write(0x1D);                    // send 0x1D - флаг записи
  ibutton.write_bit(1);                   // записываем значение флага записи = 1
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xD5);   // команда на запись
  for (byte i = 0; i<8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    BurnByte(~ReadID[i]);
    Serial.print('*');
    Sd_WriteStep();
  } 
  ibutton.write(0x1D);                    // send 0xD1 - флаг записи
  ibutton.write_bit(0);                   // записываем значение флага записи = 0 - отключаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  return true;
}

bool dataIsBurningOK(){
  byte buff[8];
  if (!ibutton.reset()) return false;
  ibutton.write(0x33);
  ibutton.read_bytes(buff, 8);
  Check = 0;
  for (byte i = 0; i < 8; i++) 
    if (ReadID[i] == buff[i]) Check++; // сравниваем код для записи с тем, что уже записано в ключе.
  if (Check != 8) return false;  // если коды совпадают, ключ успешно скопирован
  return true;
}

bool write2iBtn(){
  int Check = 0, CheckSumNewKey = 0;
  Serial.print("The new key code is: ");
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    CheckSumNewKey += ReadID[i];  
    if (ReadID[i] == addr[i]) Check++; // сравниваем код для записи с тем, что уже записано в ключе.
  }
  if (Check == 8) {  // если коды совпадают, ничего писать не нужно
    digitalWrite(R_Led, LOW); 
    Serial.println(" it is the same key. Writing in not needed.");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    delay(500);
    return false;
  }
  byte rwType = getRWtype(); // определяем тип RW-1990.1 или 1990.2
  Serial.print("\n Burning iButton ID: ");
  switch (rwType) {
    case 0: 
      Sd_ErrorBeep();
      return false;
      break;
    case 1:
      if (!write2iBtnRW1990.1()) return false;
      break;
    case 2:
      if (!write2iBtnRW1990.2()) return  false;
      break;
  }
  digitalWrite(R_Led, HIGH);
  if (!dataIsBurningOK()){
    digitalWrite(R_Led, LOW); 
    Serial.println(" The copy is faild");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;    
  }
  Serial.println(" The key has copied successesfully");
  Sd_ReadOK();
  delay(500);
  return true;
}

void BurnByte(byte data){
  digitalWrite(iButtonPin, HIGH); pinMode(iButtonPin, OUTPUT);
  for(byte n_bit=0; n_bit<8; n_bit++){ 
    ibutton.write_bit(!(data & 1));  // при прошивке 1-ца пишеся нулем, а ноль - единицей
    delay(5);                        // даем время на прошивку каждого ита до 10 мс
    data = data >> 1;                // переходим к следующему bit
  }
  pinMode(iButtonPin, INPUT);
}

bool readiBtn(){
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    ReadID[i] = addr[i];                               // копируем прочтенный код в ReadID
  }
  Serial.println();
  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC is not valid!");
    Sd_ErrorBeep();
    digitalWrite(B_Led, HIGH);
    return false;
  }
  Sd_ReadOK();
  return true;
}

void loop() {
  bool BtnPinSt = digitalRead(BtnPin);
  bool BtnClick;
  if ((BtnPinSt == LOW) &&(preBtnPinSt!= LOW)) BtnClick = true;
    else BtnClick = false;
  preBtnPinSt = BtnPinSt;
  if ((Serial.read() == 't') || BtnClick) {  // переключаель режима чение/запись
    if (readflag == true) {
      writeflag = !writeflag;
      clearLed(); 
      if (writeflag) digitalWrite(R_Led, HIGH);
        else digitalWrite(G_Led, HIGH);
      Serial.print("Writeflag = "); Serial.println(writeflag);  
    } else {
      clearLed();   
      Sd_ErrorBeep();
      digitalWrite(B_Led, HIGH);
    }
  }
  if (!ibutton.search(addr)) {
    ibutton.reset_search();
    delay(200);
    return;
  }
  if (writeflag == false){
    digitalWrite(G_Led, LOW);
    readflag = readiBtn();
    if (readflag) {
      clearLed(); digitalWrite(G_Led, HIGH);
    }
  }else{
    if (readflag == true) {
      write2iBtn();
      }
    else {          // сюда испонение не должно попасть
      clearLed();
      Sd_ErrorBeep();
      digitalWrite(B_Led, HIGH);
    }
  }
  delay(500);
}

void Sd_ReadOK() {  // звук ОК
  for (int i=400; i<6000; i=i*1.5) beep(i,20);
}

void Sd_WriteStep(){  // звук "очередной шаг"
  for (int i=2500; i<6000; i=i*1.5) beep(i,10);
}

void Sd_ErrorBeep() {  // звук "ERROR"
  for (int j=0; j <3; j++){
    for (int i=1000; i<2000; i=i*1.10) beep(i,10);
    delay(50);
    for (int i=1000; i>500; i=i*.90) beep(i,10);
    delay(50);
  }
}

void Sd_StartOK(){   // звук "Успешное включение"
  beep(note_A7,100); //A 
  beep(note_G7,100); //G 
  beep(note_E7,100); //E 
  beep(note_C7,100); //C
  beep(note_D7,100); //D 
  beep(note_B7,100); //B 
  beep(note_F7,100); //F 
  beep(note_C8,100); //C
}

void beep (unsigned long noteFrequency, unsigned long noteDuration)
{      
  unsigned int microsecondsPerWave = 1000000/noteFrequency;   // Calculate how many HIGH/LOW cycles there are per millisecond
  unsigned int loopTime = (unsigned long)(noteDuration * noteFrequency) / 2000; // Play the note for the calculated loopTime.
  for (int i = 0; i < loopTime; i++)   
    {   
      digitalWrite(speakerPin,HIGH); delayMicroseconds(microsecondsPerWave); 
      digitalWrite(speakerPin,LOW); delayMicroseconds(microsecondsPerWave); 
    } 
}     
