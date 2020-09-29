// Управляющая программа для макета пожарной сигнализации
// МК - Arduino Nano
// Примечание: для упрощения монтажа питание на клавиатуру подается с pin'а МК
//
// Дата начала разработки 2019-06-29
// Дата последней модификации 2019-07-20

#define DEBUG 1

// пин кнопки сигнализации
const int buttonPin = A5;
// порог срабатывания кнопки сигнализации
const int buttonThreshold = 900;

// пин датчика дыма
const int sensorPin = A4;

// светодиод индикации состояния
const int statusLED = 13;

// пин питания платы клавиатуры
const int kbdPowerPin = 11;
// пин готовности данных клавиатуры DV
const int kbdDVPin = 2;
// кол-во кнопок клавиатуры
const int kbdKeysCnt = 8;
// пины кнопок клавиатуры
const int kbd8Pin = 3;   // PD3
const int kbd7Pin = 4;   // PD4
const int kbd6Pin = 5;   // PD5
const int kbd5Pin = 6;   // PD6
const int kbd4Pin = 7;   // PD7
const int kbd3Pin = 8;   // PB0
const int kbd2Pin = 9;   // PB1
const int kbd1Pin = 10;  // PB2

// маска пинов кнопок и битов портов D и B
const byte kbdMaskPortD = 0b11111000;
const byte kbdMaskPortB = 0b00000111;

// состояния нажания кнопок клавиатуры и признак нового нажатия
volatile byte kbdPressedKeys;
volatile byte kbdPressedFlag;

// счетчик ввода цифр пин-кода
int pinCodeCnt = 0;
// буфер для накопления вводимого пин-кода
// !!! переделать на работу с массивом, иначе будет утечка памяти
String pinCodeBuff;
// эталонный пин-код
String pinCodeEtal = "1234";

// уровень громкости звука
const int soundLevel = 17;
// зв.фрагмент включения системы (номер и продолжительность в сек)
const int soundOnNum = 1;
const int soundOnDur = 2;
// зв.фрагмент извещения (номер и продолжительность в сек)
const int soundAlarmNum = 2;
const int soundAlarmDur = 7;

// обработчик прерывания INT0
void kbdInterruptHandler() {
  byte portB;
  byte portD;

  // считываем значения пинов портов
  portB = PINB & kbdMaskPortB;
  portD = PIND & kbdMaskPortD;

  // получаем 8-битовый код
  kbdPressedKeys = ((unsigned int)(portB << 8) | portD) >> 3;

  // признак нового нажатия
  kbdPressedFlag = 1;
}

// возвращает код нажатой кнопки
// при множественном нажатии возвращается меньший номер
int getPressedKeyCode()
{
  for (int i = 1; i <= kbdKeysCnt; i++) {
    if (kbdPressedKeys & (1 << kbdKeysCnt - i))
      return(i);
  }
}

// процедура отправки команды в DFPLayer MP3
void DFPlayer_send_cmd(byte cmd, byte prm1, byte prm2) 
{
  // вычисляем контрольную сумму (2 байта)
  int16_t checksum = -(0xFF + 0x06 + cmd + 0x00 + prm1 + prm2);

  // собираем строку команды
  byte cmd_line[10] = {0x7E, 0xFF, 0x06, cmd, 0x00, prm1, prm2, checksum >> 8, (byte) checksum & 0xFF, 0xEF};

  #ifdef DEBUG
  Serial.print("Sending command to sound module: ");
  Serial.print(cmd);
  Serial.print(", ");
  Serial.print(prm1);
  Serial.print(", ");
  Serial.println(prm2);
  #endif

  // отправляем в модуль
  for (byte i=0; i<10; i++)
    Serial.write(cmd_line[i]);
    
  #ifdef DEBUG
  Serial.println();
  #endif  
}

// процедура управления индикатором состояния (LED13)
void setStatusLED(int s) {
  digitalWrite(statusLED, s);
}  

// быстрое тройное мигание светодиодом состояния
void fastBlinkStatusLED() {
  for (int i = 0; i < 3; i++) {
    setStatusLED(HIGH);
    delay(100);
    setStatusLED(LOW);
    delay(100);
  }
}

void setup() {

  Serial.begin(9600);
  #ifdef DEBUG
  Serial.println("System initialization");
  #endif

  // инициализация портов
  // кнопка и датчик
  pinMode(buttonPin, INPUT);
  pinMode(sensorPin, INPUT);
  
  // индикатор состояния
  pinMode(statusLED, OUTPUT);
  
  // пин питания платы клавиатуры
  pinMode(kbdPowerPin, OUTPUT);
  // пины клавиатуры
  pinMode(kbd8Pin, INPUT);
  pinMode(kbd7Pin, INPUT);
  pinMode(kbd6Pin, INPUT);
  pinMode(kbd5Pin, INPUT);
  pinMode(kbd4Pin, INPUT);
  pinMode(kbd3Pin, INPUT);
  pinMode(kbd2Pin, INPUT);
  pinMode(kbd1Pin, INPUT);
      
  // устанавливаем уровень звука
  DFPlayer_send_cmd(0x06, 0, soundLevel);
  delay(100);
  // звуковой сигнал включения
  DFPlayer_send_cmd(0x03, 0, soundOnNum);
  delay(soundOnDur * 1000);

  // подаём питание на клавиатуру
  digitalWrite(kbdPowerPin, HIGH);
  delay(100);
  
  // подключаем обработчик прерываний от клавиатуры
  attachInterrupt(0, kbdInterruptHandler, RISING);

  fastBlinkStatusLED();
}

void loop() {
  int buttonStatus, sensorStatus;
  static int alarmStatus = 0;
  static long alarmSoundStartTime = 0;

  // проверка на включение режима тревоги
  // проверяем состояние кнопки и датчика 
  buttonStatus = analogRead(buttonPin) < buttonThreshold ? 1:0;
  sensorStatus = digitalRead(sensorPin) > 0 ? 0:1;
  if ((buttonStatus == 1 || sensorStatus == 1) && alarmStatus == 0) {
    // включаем режим тревоги
    #ifdef DEBUG
    Serial.println("Alarm mode is ON");
    #endif
    alarmStatus = 1;
  }
  
  // проверка на выключение режима тревоги через ввод пин-кода
  if (kbdPressedFlag == 1) {
    // была нажата очередная кнопка
    String codeReceived = String(getPressedKeyCode());
    #ifdef DEBUG
    Serial.print("Key pressed: ");
    Serial.println(codeReceived);
    #endif
    // сбрасываем флаг нажатия
    kbdPressedFlag = 0;

    if (alarmStatus == 1) {
      // если в режиме тревоги, то накапливаем вводимый пин-код в буфере
      
      // помещаем введенный код в буфер ввода пин-кода
      pinCodeBuff = pinCodeBuff + codeReceived;
      pinCodeCnt++;
      if (pinCodeCnt == 4) {
        // введены все цифры пин-кода, выполняем проверку
        if (pinCodeBuff == pinCodeEtal) {
          // код совпал, выключаем режим тревоги
          #ifdef DEBUG
          Serial.println("Correct PINCODE entered, alarm mode is OFF");
          #endif
          alarmStatus = 0;
        } 
        else {
          // код не совпал, мигаем светиком
          #ifdef DEBUG
          Serial.print("PINCODE entered ");
          Serial.print(pinCodeBuff);
          Serial.println(" is INCORRECT");
          #endif
          fastBlinkStatusLED();
        }    

        // cбрасываем буфер и счетчик пин-кода 
        pinCodeBuff = ""; 
        pinCodeCnt = 0;
      }
    }  
  }  
    
  if (alarmStatus == 1 && (alarmSoundStartTime == 0 || (millis() - alarmSoundStartTime > soundAlarmDur * 1000))) {
    // находимся в режим тревоги, но звуковой сигнал еще не включен или уже завершился
    #ifdef DEBUG
    Serial.println("Starting siren sound");
    #endif 
    // засекаем новое время включения
    alarmSoundStartTime = millis();
    // включаем звуковой сигнал
    DFPlayer_send_cmd(0x03, 0, soundAlarmNum);
  }
  
  delay(500);
}
