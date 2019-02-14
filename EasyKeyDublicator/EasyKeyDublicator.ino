#include <OneWire.h>
#include "pitches.h"

#define iButtonPin A5      // Линия data ibutton
#define R_Led 2            // RGB Led
#define G_Led 3
#define B_Led 4
#define ACpinGnd 5         // Земля аналогового компаратора Cyfral/Metacom
#define ACpin 6            // Вход аналогового компаратора 3В для Cyfral/Metacom
#define BtnPin 7           // Кнопка переключения режима чтение/запись
#define BtnPinGnd 8        // Земля кнопки переключения режима 
#define speakerPin 9       // Спикер, он же buzzer, он же beeper
#define speakerPinGnd 10    // земля Спикера

OneWire ibutton (iButtonPin); 
byte addr[8];                             // временный буфер
byte keyID[8];                            // ID ключа для записи
bool readflag = false;                    // флаг сигнализируе, что данные с ключа успечно прочианы в ардуино
bool writeflag = false;                   // режим запись/чтение
bool preBtnPinSt = HIGH;
enum emRWType {TM01, RW1990_1, RW1990_2, TM2004};                            // тип болванки
enum emkeyType {keyUnknown, keyDallas, keyTM2004, keyCyfral, keyMetacom};    // тип оригинального ключа  
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
//*************** dallas **************
emRWType getRWtype(){    
   byte answer;
  // TM01 это неизвестный тип болванки, делается попытка записи TM-01 без финализации для dallas или c финализацией под cyfral или metacom
  // RW1990_1 - dallas-совместимые RW-1990, RW-1990.1, ТМ-08, ТМ-08v2 
  // RW1990_2 - dallas-совместимая RW-1990.2
  // TM2004 - dallas-совместимая TM2004 в доп. памятью 1кб
  //if (writeflag){
    // пробуем определить RW-1990.1
    ibutton.reset(); ibutton.write(0xD1); // проуем снять флаг записи для RW-1990.1
    ibutton.write_bit(1);                 // записываем значение флага записи = 1 - отключаем запись
    delay(10); pinMode(iButtonPin, INPUT);
    ibutton.reset(); ibutton.write(0xB5); // send 0xB5 - запрос на чтение флага записи
    answer = ibutton.read();
    //Serial.print("\n Answer RW-1990.1: "); Serial.println(answer, HEX);
    if (answer == 0xFE){
      Serial.println(" Type: dallas RW-1990.1 ");
      return RW1990_1;            // это RW-1990.1
    }
    // пробуем определить RW-1990.2
    ibutton.reset(); ibutton.write(0x1D);  // проуем установить флаг записи для RW-1990.2 
    ibutton.write_bit(1);                  // записываем значение флага записи = 1 - включаем запись
    delay(10); pinMode(iButtonPin, INPUT);
    ibutton.reset(); ibutton.write(0x1E);  // send 0x1E - запрос на чтение флага записи
    answer = ibutton.read();
    //Serial.print("\n Answer RW-1990.2: "); Serial.println(answer, HEX);
    if (answer == 0xFE){
      ibutton.reset(); ibutton.write(0x1D); // возвращаем оратно запрет записи для RW-1990.2
      ibutton.write_bit(0);                 // записываем значение флага записи = 0 - выключаем запись
      delay(10); pinMode(iButtonPin, INPUT);
      Serial.println(" Type: dallas RW-1990.2 ");
      return RW1990_2; // это RW-1990.2
    }
  //}
  // пробуем определить TM-2004
  ibutton.reset(); ibutton.write(0x33);                     // посылаем команду чтения ROM для перевода в расширенный 3-х байтовый режим
  for ( byte i=0; i<8; i++) ibutton.read();                 //читаем данные ключа
  ibutton.write(0xAA);                                      // пробуем прочитать регистр статуса для TM-2004    
  ibutton.write(0x00); ibutton.write(0x00);                 // передаем адрес для считывания
  answer = ibutton.read();                                  // читаем CRC комманды и адреса
  //Serial.print("TM2004 CRC: "); Serial.println(answer, HEX);
  byte m1[3] = {0xAA, 0,0};                                 // вычисляем CRC комманды
  if (OneWire::crc8(m1, 3) == answer) {
    answer = ibutton.read();                                  // читаем регистр статуса
    //Serial.print(" status: "); Serial.println(answer, HEX);
    Serial.println(" Type: dallas TM2004");
    return TM2004; // это Type: TM2004
  }
  Serial.println(" Type: dallas unknown, trying TM-01! ");
  return TM01;                              // это неизвестный тип DS1990, нужно перебирать алгоритмы записи (TM-01)
}

bool write2iBtnTM2004(){                // функция записи на TM2004
  byte answer; bool result = true;
  ibutton.reset(); ibutton.write(0x33);                     // посылаем команду чтения ROM для перевода в расширенный 3-х байтовый режим
  for (byte i=0; i<8; i++) ibutton.read();                 //читаем данные ключа
  ibutton.write(0x3C);                                      // команда записи ROM для TM-2004    
  ibutton.write(0x00); ibutton.write(0x00);                 // передаем адрес с которого начинается запись
  for (byte i = 0; i<8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    ibutton.write(keyID[i]);
    answer = ibutton.read();
    //if (OneWire::crc8(m1, 3) != answer){result = false; break;}     // crc не верный
    delayMicroseconds(600); ibutton.write_bit(1); delay(50);         // испульс записи
    pinMode(iButtonPin, INPUT);
    Serial.print('*');
    Sd_WriteStep();
    if (keyID[i] != ibutton.read()) {result = false; break;}    //читаем записанный байт и сравниваем, с тем что должно записаться
  } 
  if (!result){
    ibutton.reset();
    Serial.println(" The key copy faild");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    return false;    
  }
  ibutton.reset();
  Serial.println(" The key has copied successesfully");
  Sd_ReadOK();
  delay(500);
  digitalWrite(R_Led, HIGH);
  return true;
}

bool write2iBtnRW1990_1_2_TM01(emRWType rwType){              // функция записи на RW1990.1, RW1990.2, TM-01C(F)
  byte rwCmd, rwFlag = 1;
  switch (rwType){
    case TM01: rwCmd = 0xC1; break;                   //TM-01C(F)
    case RW1990_1: rwCmd = 0xD1; rwFlag = 0; break;  // RW1990.1  флаг записи инвертирован
    case RW1990_2: rwCmd = 0x1D; break;              // RW1990.2
  }
  ibutton.reset(); ibutton.write(rwCmd);       // send 0xD1 - флаг записи
  ibutton.write_bit(rwFlag);                   // записываем значение флага записи = 1 - разрешить запись
  delay(10); pinMode(iButtonPin, INPUT);
  ibutton.reset(); ibutton.write(0xD5);        // команда на запись
  for (byte i = 0; i<8; i++){
    digitalWrite(R_Led, !digitalRead(R_Led));
    if (rwType == RW1990_1) BurnByte(~keyID[i]);      // запись происходит инверсно для RW1990.1
      else BurnByte(keyID[i]);
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
  if ((keyType == keyMetacom)||(keyType == keyCyfral)){      //переводим ключ из формата dallas
    ibutton.reset();
    if (keyType == keyCyfral) ibutton.write(0xCA);       // send 0xCA - флаг финализации Cyfral
      else ibutton.write(0xCB);                       // send 0xCA - флаг финализации metacom
    ibutton.write_bit(1);                             // записываем значение флага финализации = 1 - перевезти формат
    delay(10); pinMode(iButtonPin, INPUT);
  }
  Sd_ReadOK();
  delay(500);
  digitalWrite(R_Led, HIGH);
  return true;
}

void BurnByte(byte data){
  for(byte n_bit=0; n_bit<8; n_bit++){ 
    ibutton.write_bit(data & 1);  
    delay(5);                        // даем время на прошивку каждого бита до 10 мс
    data = data >> 1;                // переходим к следующему bit
  }
  pinMode(iButtonPin, INPUT);
}

bool dataIsBurningOK(){
  byte buff[8];
  if (!ibutton.reset()) return false;
  ibutton.write(0x33);
  ibutton.read_bytes(buff, 8);
  byte Check = 0;
  for (byte i = 0; i < 8; i++) 
    if (keyID[i] == buff[i]) Check++;      // сравниваем код для записи с тем, что уже записано в ключе.
  if (Check != 8) return false;             // если коды совпадают, ключ успешно скопирован
  return true;
}

bool write2iBtn(){
  int Check = 0, CheckSumNewKey = 0;
  Serial.print("The new key code is: ");
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    CheckSumNewKey += keyID[i];  
    if (keyID[i] == addr[i]) Check++;    // сравниваем код для записи с тем, что уже записано в ключе.
  }
  if (Check == 8) {                     // если коды совпадают, ничего писать не нужно
    digitalWrite(R_Led, LOW); 
    Serial.println(" it is the same key. Writing in not needed.");
    Sd_ErrorBeep();
    digitalWrite(R_Led, HIGH);
    delay(500);
    return false;
  }
  byte rwType = getRWtype(); // определяем тип RW-1990.1 или 1990.2 или TM-01
  Serial.print("\n Burning iButton ID: ");
  if (rwType == TM2004) return write2iBtnTM2004();  //шьем TM2004
    else return write2iBtnRW1990_1_2_TM01(rwType); //пробуем прошить другие форматы
}

bool readiBtn(){
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    keyID[i] = addr[i];                               // копируем прочтенный код в ReadID
  }
  if (addr[0] == 0x01) {                         // это ключ формата dallas
    keyType = keyDallas;
    if (getRWtype() == TM2004) {
      //Serial.println(" Type: dallas TM2004");
      keyType = keyTM2004;
    } //else Serial.println(" Type: dallas RW1990.x");
    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      Sd_ErrorBeep();
      digitalWrite(B_Led, HIGH);
      return false;
    }
    Sd_ReadOK();
    return true;
  }
  keyType = keyUnknown;
  return true;
}

//************ Cyfral ***********************
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
    }             // end if
  } while ((long)(micros() - tStart) < timeOut);
  return 0;
}

void ACsetOn(){
  ADCSRA &= ~(1<<ADEN);      // выключаем ADC
  ADCSRB |= (1<<ACME);        //включаем AC
  ADMUX = 0b00000101;        // подключаем к AC Линию A5
}

bool read_cyfral(byte* buf, byte CyfralPin){
  unsigned long ti; byte j = 0;
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
  keyType = keyCyfral;
  for (byte i = 0; i < 8; i++) {
    Serial.print(addr[i], HEX); Serial.print(":");
    keyID[i] = addr[i];                               // копируем прочтенный код в ReadID
  }
  Serial.println(" Type: Cyfral ");
  return true;  
}
//*************************************

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
  //goto l1;
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


//***************** зуки****************
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
