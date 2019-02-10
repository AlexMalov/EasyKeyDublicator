#include <OneWire.h>
#include "pitches.h"

#define iButtonPin A5      // Линия data ibutton
#define R_Led 2            // RGB Led
#define G_Led 3
#define B_Led 4
#define ACpinGnd 5         // Земля аналогового компаратора Cyfral
#define ACpin 6            // Вход аналогового компаратора 3В для Cyfral
#define BtnPin 7           // Кнопка переключения режима чтение/запись
#define BtnPinGnd 8        // Земля кнопки переключения режима чтение/запись
#define speakerPin 9       // Спикер, он же buzzer, он же beeper
#define speakerPinGnd 10    // Спикер, он же buzzer, он же beeper

OneWire ibutton (iButtonPin); 
byte addr[8];
byte ReadID[8];                           // массив с данными для записи
bool readflag = false;                    // флаг сигнализируе, что данные с ключа успечно прочианы в ардуино
bool writeflag = false;                   // режим запись/чтение
bool preBtnPinSt = HIGH;
enum emkeyType {unknown, dallas, cyfral, metacom};    // тип оригинального ключа  
emkeyType keyType;

void setup() {
  pinMode(BtnPin, INPUT_PULLUP);                            // включаем чтение и подягиваем пин кнопки режима к +5В
  pinMode(BtnPinGnd, OUTPUT); digitalWrite(BtnPinGnd, LOW); // подключаем второй пин кнопки к земле
  pinMode(speakerPin, OUTPUT);
  pinMode(speakerPinGnd, OUTPUT); digitalWrite(speakerPinGnd, LOW); // подключаем второй пин спикера к земле
  pinMode(ACpin, INPUT);                                            // Вход аналогового компаратора 3В для Cyfral
  pinMode(ACpinGnd, OUTPUT); digitalWrite(ACpinGnd, LOW);           // подключаем второй пин аналогового компаратора Cyfral к земле 
  pinMode(R_Led, OUTPUT); pinMode(G_Led, OUTPUT); pinMode(B_Led, OUTPUT);  //RGB-led
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
  // 0 это неизвестный тип болванки, нужно перебирать алгоритмы записи (TM-01)
  // 1 это RW-1990, RW-1990.1, ТМ-08, ТМ-08v2
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
  return 0;                              // это неизвестный тип DS1990, нужно перебирать алгоритмы записи (TM-01)
}

bool write2iBtnRW1990_1_2_TM01(byte rwType){              // функция записи на RW1990.1
  byte rwCmd, rwFlag = 1;
  switch (rwType){
    case 0: rwCmd = 0xC1; break;              //TM-01C(F)
    case 1: rwCmd = 0xD1; rwFlag = 0; break;  // RW1990.1  флаг записи инвертирован
    case 2: rwCmd = 0x1D; break;              //RW1990.2
  }
  ibutton.reset(); ibutton.write(rwCmd);       // send 0xD1 - флаг записи
  ibutton.write_bit(rwFlag);                   // записываем значение флага записи = 1 - разрешить запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xD5);        // команда на запись
  for (byte i = 0; i<8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    if (rwType == 1) BurnByte(~ReadID[i]);      // запись происходит инверсно 
      else BurnByte(ReadID[i]);
    Serial.print('*');
    Sd_WriteStep();
  } 
  ibutton.write(rwCmd);                     // send 0xD1 - флаг записи
  ibutton.write_bit(!rwFlag);               // записываем значение флага записи = 1 - отключаем запись
  delay(10); pinMode(iButtonPin, INPUT);
  digitalWrite(R_Led, LOW);       
  if (!dataIsBurningOK()){          // проверяем корректность записи
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;
  }
  Serial.println(" The key has copied successesfully");
  if ((keyType == metacom)||(keyType == cyfral)){      //переводим ключ из формата dallas
    ibutton.reset();
    if (keyType == cyfral) ibutton.write(0xCA);       // send 0xCA - флаг финализации Cyfral
      else ibutton.write(0xCB);                       // send 0xCA - флаг финализации metacom
    ibutton.write_bit(1);                             // записываем значение флага финализации = 1 - перевезти формат
    delay(10); pinMode(iButtonPin, INPUT);
  }
  Sd_ReadOK();
  delay(500);
  digitalWrite(R_Led, HIGH);
  return true;
}

bool dataIsBurningOK(){
  byte buff[8];
  if (!ibutton.reset()) return false;
  ibutton.write(0x33);
  ibutton.read_bytes(buff, 8);
  byte Check = 0;
  for (byte i = 0; i < 8; i++) 
    if (ReadID[i] == buff[i]) Check++;  // сравниваем код для записи с тем, что уже записано в ключе.
  if (Check != 8) return false;         // если коды совпадают, ключ успешно скопирован
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
  byte rwType = getRWtype(); // определяем тип RW-1990.1 или 1990.2 или TM-01
  Serial.print("\n Burning iButton ID: ");
  return write2iBtnRW1990_1_2_TM01(rwType); //пробуем прошить
}

void BurnByte(byte data){
  for(byte n_bit=0; n_bit<8; n_bit++){ 
    ibutton.write_bit(data & 1);  
    delay(5);                        // даем время на прошивку каждого бита до 10 мс
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
  if (addr[0] == 0x01)  {                         // это ключ формата dallas
    keyType = dallas;
    Serial.println("Type: dallas");
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      Sd_ErrorBeep();
      digitalWrite(B_Led, HIGH);
      return false;
    }
    Sd_ReadOK();
    return true;
  }
  keyType = unknown;
  return true;
}

unsigned long pulseAComp(bool pulse, unsigned long timeOut = 20000){  // pulse HIGH or LOW
  bool AcompState;
  unsigned long tStart = micros();
  do {
      AcompState = ACSR & _BV(ACO);  // читаем флаг компаратора
      if (AcompState == pulse) {
        tStart = micros();
        do {
          AcompState = ACSR & _BV(ACO);  // читаем флаг компаратора
          if (AcompState != pulse) return (long)(micros() - tStart);  
        } while ((long)(micros() - tStart) < timeOut);
        return 0;                                                 //таймаут, импульс не вернуся оратно
      } // end if
    } while ((long)(micros() - tStart) < timeOut);
  return 0;
}

void ACsetOff(){
  ACSR |= (1<<ACD); //switch off the AC
}

void ACsetOn(){
  ADCSRA &= ~(1<<ADEN);      // выключаем ADC
  ADCSRB |= (1<<ACME);        //включаем AC
  ADMUX = 0b00000101;        // подключаем к AC Линию A5
}

bool read_cyfral(byte* buf, byte CyfralPin){
  unsigned long ti;
  byte j;
  digitalWrite(CyfralPin, LOW); pinMode(CyfralPin, OUTPUT);  //отклчаем питание от ключа
  delay(200);
  ACsetOn();    //analogComparator.setOn(0, CyfralPin); 
  pinMode(CyfralPin, INPUT_PULLUP);  // включаем пиание Cyfral
  for (byte i = 0; i<36; i++){    // чиаем 36 bit
    ti = pulseAComp(HIGH);
    if ((ti == 0) || (ti > 200)) break;                      // not Cyfral
    //if ((ti > 20)&&(ti < 50)) bitClear(buf[i >> 3], 7-j);
    if ((ti > 50) && (ti < 200)) bitSet(buf[i >> 3], 7-j);
    j++; if (j>7) j=0; 
  }
  ACsetOff();
  if (ti == 0) return false;
  if ((buf[0] >> 4) != 0b1110) return false;   /// not Cyfral
  byte test;
  for (byte i = 1; i<4; i++){
    test = buf[i] >> 4;
    if ((test != 1)&&(test != 2)&&(test != 4)&&(test != 8)) return false;
    test = buf[i] & 0x0F;
    if ((test != 1)&&(test != 2)&&(test != 4)&&(test != 8)) return false;
  }
  return true;
}

bool searchCyfral(){
  for (byte i = 0; i < 8; i++) addr[i] = 0;
  bool rez = read_cyfral(addr, iButtonPin);
  if (!rez) return false; 
  keyType = cyfral;
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    ReadID[i] = addr[i];                               // копируем прочтенный код в ReadID
  }
  Serial.println("Type: Cyfral ");
  return true;  
}

void loop() {
  bool BtnPinSt = digitalRead(BtnPin);
  bool BtnClick;
  if ((BtnPinSt == LOW) &&(preBtnPinSt!= LOW)) BtnClick = true;
    else BtnClick = false;
  preBtnPinSt = BtnPinSt;
  if ((Serial.read() == 't') || BtnClick) {  // переключаель режима чтение/запись
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
  if ((!writeflag) && (searchCyfral())) {  // запускаем поиск cyfral
    digitalWrite(G_Led, LOW);
    Sd_ReadOK();
    readflag = true;
    clearLed(); digitalWrite(G_Led, HIGH);
  }
 // goto l1;
  if (!ibutton.search(addr)) {  // запускаем поиск dallas
    ibutton.reset_search();
    delay(200);
    return;
  }
  if (!writeflag){ 
    digitalWrite(G_Led, LOW);
    readflag = readiBtn();       // чиаем ключ dallas
    if (readflag) {
      clearLed(); digitalWrite(G_Led, HIGH);
    }
  }else{
    if (readflag == true) write2iBtn();
      else {          // сюда испонение не должно попасть
        clearLed();
        Sd_ErrorBeep();
        digitalWrite(B_Led, HIGH);
    }
  }
  l1:
  delay(300);
}

void Sd_ReadOK() {  // звук ОК
  for (int i=400; i<6000; i=i*1.5) { tone(speakerPin, i); delay(20); }
  noTone(speakerPin);
}

void Sd_WriteStep(){  // звук "очередной шаг"
  for (int i=2500; i<6000; i=i*1.5) { tone(speakerPin, i); delay(10); }
  noTone(speakerPin);
}

void Sd_ErrorBeep() {  // звук "ERROR"
  for (int j=0; j <3; j++){
    for (int i=1000; i<2000; i=i*1.1) { tone(speakerPin, i); delay(10); }
    delay(50);
    for (int i=1000; i>500; i=i*1.9) { tone(speakerPin, i); delay(10); }
    delay(50);
  }
  noTone(speakerPin);
}

void Sd_StartOK(){   // звук "Успешное включение"
  tone(speakerPin, NOTE_A7); delay(100);
  tone(speakerPin, NOTE_G7); delay(100);
  tone(speakerPin, NOTE_E7); delay(100); 
  tone(speakerPin, NOTE_C7); delay(100);  
  tone(speakerPin, NOTE_D7); delay(100); 
  tone(speakerPin, NOTE_B7); delay(100); 
  tone(speakerPin, NOTE_F7); delay(100); 
  tone(speakerPin, NOTE_C7); delay(100);
  noTone(speakerPin); 
}
