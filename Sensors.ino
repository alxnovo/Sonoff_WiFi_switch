// -----------------  Аналоговый вход A0
void initA0() {
  static uint16_t t = readArgsInt();
  static uint8_t averageFactor = readArgsInt();
  if (t < 500) t = 1000;
  if (averageFactor == 0) averageFactor = 1023;
  analogRead(A0);
  sendStatus(stateA0S, analogRead(A0));
  sendOptions(alarmA0S, 0);
  String alarmSet="ALARM "+stateA0S+" "+highalarmA0S+" "+lowalarmA0S;
  sCmd.readStr(alarmSet);
  //alarmLoad(stateA0S, highalarmA0S, lowalarmA0S);
  ts.add(3, t, [&](void*) {
    uint32_t a = 0;
    for (uint8_t i = 1; i <= 10; i++) {
      a += analogRead(A0);
    }
    a = a / 10;
    sendStatus(stateA0S, a);
    alarmTest(stateA0S, highalarmA0S, lowalarmA0S, alarmA0S);
  }, nullptr, true);
  HTTP.on("/analog.json", HTTP_GET, []() {
    String data = graf3(getStatusInt(stateA0S), getOptionsInt(highalarmA0S), getOptionsInt(lowalarmA0S), 10, t, "low:0");
    httpOkJson(data);
  });
  modulesReg("analog");
}
// ----------------- OneWire -------------------------------------------------------------------------------
void initOneWire() {
  uint8_t pin = readArgsInt();
  static uint16_t t = readArgsInt();
  static uint8_t averageFactor = readArgsInt();
  if (t < 750) t = 1000;
  //Serial.println(t);
//  Serial.println(pin);
  oneWire =  new OneWire(pin);
  sensors.setOneWire(oneWire);
  sensors.begin();
  byte num = sensors.getDS18Count();
//Serial.println(num);
  for (byte i = 0; i < num; i++) {
    sendStatus(temperatureS + (String)(i + 1), (String)sensors.getTempCByIndex(i));
    modulesReg(temperatureS + (String)(i + 1));
    sendOptions(alarmtempS + (String)(i + 1), 0);
    sendOptions(highalarmtempS + (String)(i + 1), 0);
    sendOptions(lowalarmtempS + (String)(i + 1), 0);
    alarmLoad(temperatureS + (String)(i + 1), highalarmtempS + (String)(i + 1), lowalarmtempS + (String)(i + 1));
  }
  sendOptions(temperatureS + "num", num);
  ts.add(4, t, [&](void*) {
    //    static float oldTemp = 0;
    float temp = 0;
    sensors.requestTemperatures();
    for (byte i = 0; i < sensors.getDS18Count(); i++) {
      temp = sensors.getTempCByIndex(i);
      String num = "";
      num = (String)(i + 1);
      sendStatus(temperatureS + num, (String)temp);
      alarmTest(temperatureS + num, highalarmtempS + num, lowalarmtempS + num, alarmtempS + num);
    }
  }, nullptr, true);

  HTTP.on("/temperature.json", HTTP_GET, []() {
    String num = HTTP.arg("n");
    String data = graf3(getStatusFloat(temperatureS + num), getOptionsFloat(highalarmtempS + num), getOptionsFloat(lowalarmtempS + num), 10, t, "low:0");
    httpOkJson(data);
  });
}
// -----------------  DHT
void initDHT() {
  uint8_t pin = readArgsInt();
  dht.setup(pin);
  delay(1000);
  static uint16_t t = readArgsInt();
  static uint16_t test = dht.getMinimumSamplingPeriod();
  if (t < test) t = test;
  //Serial.println(t);
  String temp = "";
  temp += dht.getTemperature();
  //  Serial.println(temp);
  if (temp != "nan") {
    sendStatus(temperatureS, dht.getTemperature());
    sendOptions(alarmtempS, 0);
    alarmLoad(temperatureS, highalarmtempS, lowalarmtempS);
    sendStatus(humidityS, dht.getHumidity());
    sendOptions(alarmhumS, 0);
    alarmLoad(humidityS, highalarmhumS, lowalarmhumS);
    ts.add(5, test, [&](void*) {
      sendStatus(temperatureS, dht.getTemperature());
      sendStatus(humidityS, dht.getHumidity());
      alarmTest(temperatureS, highalarmtempS, lowalarmtempS, alarmtempS);
      alarmTest(humidityS, highalarmhumS, lowalarmhumS, alarmhumS);
    }, nullptr, true);
    HTTP.on("/temperature.json", HTTP_GET, []() {
      //      String data = graf(getStatusInt(temperatureS), 10, t, "low:0");
      String data = graf3(getStatusFloat(temperatureS), getOptionsFloat(highalarmtempS), getOptionsFloat(lowalarmtempS), 10, t, "low:0");
      httpOkJson(data);
    });
    HTTP.on("/humidity.json", HTTP_GET, []() {
      //      String data = graf(getStatusInt(humidityS), 10, t, "low:0");
      String data = graf3(getStatusFloat(humidityS), getOptionsFloat(highalarmhumS), getOptionsFloat(lowalarmhumS), 10, t, "low:0");
      httpOkJson(data);
    });
    modulesReg(temperatureS);
    modulesReg(humidityS);
  }
}

// -----------------------Данных уровней активных модулей --------------------------------------------
void alarmLoadModules(){
  String  modulesN = selectToMarker(modules, "]");
  modulesN = deleteBeforeDelimiter(modulesN, "[");
  modulesN +=",";
  modulesN.replace("\"", "");
  //Serial.println(modulesN);
  //"upgrade","relay1","ntp","ddns","mqtt","analog"
  do{
  String m = selectToMarker(modulesN, ",");
  //Serial.println(m);
  String alarmSet="ALARM ";
  if (m=="analog")  alarmSet +=stateA0S+" "+highalarmA0S+" "+lowalarmA0S;
  if (m==temperatureS)  alarmSet +=temperatureS+" "+highalarmtempS+" "+lowalarmtempS;
  if (m==humidityS)  alarmSet +=humidityS+" "+highalarmhumS+" "+lowalarmhumS;
  if (m=="pow")  alarmSet +=ActivePowerWS+" "+highalarmpowS+" "+lowalarmpowS;

  modulesN = deleteBeforeDelimiter(modulesN, ",");
  } while (modulesN !="");
  }

// ----------------------- Загрузка данных уровней сработки ------------------------------------------
void alarmLoad(String sName, String high, String low) {
  sendOptionsF(high, 0);
  sendOptionsF(low, 0);
  String configSensor = readFile(ScenaryS, 4096);
  configSensor.replace("\r\n", "\n");
  configSensor.replace("\n\n", "\n");
  configSensor += "\n";
  do {
    String test = selectToMarker(configSensor, "\n");
    // if temperature1 > 26.50
    String del = "if " + sName + " >";
    if (test.indexOf(del) != -1) {
      //      Serial.println(test);
      test.replace(del, "");
      sendOptionsF(high, test.toFloat());
    }
    del = "if " + sName + " <";
    if (test.indexOf(del) != -1) {
      //      Serial.println(test);
      test.replace(del, "");
      sendOptionsF(low, test.toFloat());
    }
    configSensor = deleteBeforeDelimiter(configSensor, "\n"); //Откидываем обработанную строку
    configSensor = deleteBeforeDelimiterTo(configSensor, "if"); // откидываем все до следующего if
  } while (configSensor.length() != 0);
}
// ------------------------------- Проверка уровней ------------------------------------------------
// Текущее значение сенсора уровни признак датчика
void alarmTest(String value, String high, String low, String sAlarm ) {
  if (getOptionsFloat(high) != 0 || getOptionsFloat(low) != 0) { // нужно добавить флаг остановки теста
    //Serial.println("test");
    if (getStatusFloat(value) > getOptionsFloat(high) && getOptionsInt(sAlarm) == LOW) {
      sendOptions(sAlarm, HIGH);
      //Serial.println(getStatusFloat(value));
//      Serial.print("up<");
//      Serial.println(getStatusFloat(value));
      flag = sendStatus(value, getStatusFloat(value));
    }
      if (getStatusFloat(value) < getOptionsFloat(low) && getOptionsInt(sAlarm) == HIGH) {
        sendOptions(sAlarm, LOW);
//        Serial.print("down>");
//        Serial.println(getStatusFloat(value));
        flag = sendStatus(value, getStatusFloat(value));
      }
  }
}


// ----------------------Приемник ИK
void irReceived() {
  byte pin = readArgsInt();
  if (pin == 1 || pin == 3)  Serial.end();
  irReceiver = new IRrecv(pin);  // Create a new IRrecv object. Change to what ever pin you need etc.
  irReceiver->enableIRIn(); // Start the receiver
  // задача опрашивать RC код
  ts.add(6, 100, [&](void*) {
    handleIrReceiv();
  }, nullptr, true);
  sendStatus(irReceivedS, "ffffffff");
  modulesReg(irReceivedS);
}
void handleIrReceiv() {
  if (irReceiver->decode(&results)) {
    //serialPrintUint64(results.value, HEX);
    //Serial.println("");
    dump(&results);
    flag = sendStatus(irReceivedS, String((uint32_t) results.value, HEX));
    irReceiver->resume();  // Continue looking for IR codes after your finished dealing with the data.
  }
}
void dump(decode_results *results) {
  // Dumps out the decode_results structure.
  // Call this after IRrecv::decode()
  uint16_t count = results->rawlen;
  if (results->decode_type == UNKNOWN) {
    sendOptions(irDecodeTypeS, "UNKNOWN");
  } else if (results->decode_type == NEC) {
    sendOptions(irDecodeTypeS, "NEC");
  } else if (results->decode_type == SONY) {
    sendOptions(irDecodeTypeS, "SONY");
  } else if (results->decode_type == RC5) {
    sendOptions(irDecodeTypeS, "RC5");
  } else if (results->decode_type == RC5X) {
    sendOptions(irDecodeTypeS, "RC5X");
  } else if (results->decode_type == RC6) {
    sendOptions(irDecodeTypeS, "RC6");
  } else if (results->decode_type == RCMM) {
    sendOptions(irDecodeTypeS, "RCMM");
  } else if (results->decode_type == PANASONIC) {
    sendOptions(irDecodeTypeS, "PANASONIC");
  } else if (results->decode_type == LG) {
    sendOptions(irDecodeTypeS, "LG");
  } else if (results->decode_type == JVC) {
    sendOptions(irDecodeTypeS, "JVC");
  } else if (results->decode_type == AIWA_RC_T501) {
    sendOptions(irDecodeTypeS, "AIWA_RC_T501");
  } else if (results->decode_type == WHYNTER) {
    sendOptions(irDecodeTypeS, "WHYNTER");
  } else if (results->decode_type == NIKAI) {
    sendOptions(irDecodeTypeS, "NIKAI");
  }
}
// ----------------------Приемник на 433мГ
void rfReceived() {
  byte pin = readArgsInt();
  if (pin == 1 || pin == 3)  Serial.end();
  mySwitch.enableReceive(pin);
  pinMode(pin, INPUT);
  // задача опрашивать RC код
  ts.add(7, 5, [&](void*) {
    // handleRfReceiv();
  }, nullptr, true);
  sendStatus(rfReceivedS, 0);
  sendOptions(rfBitS, 0);
  sendOptions(rfProtocolS, 0);
  modulesReg(rfReceivedS);
}
void handleRfReceiv() {
  if (mySwitch.available()) {
    int value = mySwitch.getReceivedValue();
    if (value == 0) {
      sendStatus(rfReceivedS, 0);
      sendOptions(rfBitS, 0);
      sendOptions(rfProtocolS, 0);
    } else {
      uint32_t temp = mySwitch.getReceivedValue() ;
      flag = sendStatus(rfReceivedS, temp);
      temp = mySwitch.getReceivedBitlength();
      sendOptions(rfBitS, temp);
      temp = mySwitch.getReceivedProtocol();
      sendOptions(rfProtocolS, temp);
    }
    mySwitch.resetAvailable();
  }
}
// -----------------  Кнопка
void initTach() {
  uint8_t pin = readArgsInt(); // первый аргумент pin
  String num = readArgsString(); // второй аргумент прификс реле 0 1 2
  uint16_t bDelay = readArgsInt(); // третий время нажатия
  sendStatus(stateTachS + num, 0);
  buttons[num.toInt()].attach(pin);
  buttons[num.toInt()].interval(bDelay);
  but[num.toInt()] = true;
  boolean inv = readArgsInt(); // четвертый аргумент инверсия входа
  sendOptions("invTach" + num, inv);
}
void handleButtons() {
  static uint8_t num = 0;
  if (but[num]) {
    buttons[num].update();
    //if (buttons[num].fell() != getStatusInt(stateTachS + String(num, DEC))) {
    if (buttons[num].fell()) {
      flag = sendStatus(stateTachS + String(num, DEC), !getOptionsInt("invTach" + String(num)));
    }
    //if (buttons[num].rose() != getStatusInt(stateTachS + String(num, DEC))) {
    if (buttons[num].rose()) {
      flag = sendStatus(stateTachS + String(num, DEC), getOptionsInt("invTach" + String(num)));
    }
    //buttons[num].rose()
  }
  num++;
  if (num == 8) num = 0;
}

#ifdef POW
// Импульс 1 Гц на выводе CF1 означает 15 мА или 0,5 В RMS в зависимости от значения в выводе SEL
void ICACHE_RAM_ATTR hlw8012_cf1_interrupt() {
  hlw8012.cf1_interrupt();
}
// Импульс 1 Гц на выводе CF составляет около 12 Вт RMS
void ICACHE_RAM_ATTR hlw8012_cf_interrupt() {
  hlw8012.cf_interrupt();
}

// ---------------------- Измеритель мощьности POW
void initHLW8012() {
  static uint16_t t = readArgsInt();
  String temp = readArgsString();
  byte pinCF;
  if (temp == "") pinCF = pinTest(14);
  else pinCF = pinTest(temp.toInt());
  temp = readArgsString();
  byte pinCF1;
  if (temp == "") pinCF1 = pinTest(13);
  else pinCF1 = pinTest(temp.toInt());
  temp = readArgsString();
  byte pinSEL;
  if (temp == "") pinSEL = pinTest(5);
  else pinSEL = pinTest(temp.toInt());
  hlw8012.begin(pinCF, pinCF1, pinSEL, HIGH, true);
  hlw8012.setResistors(CURRENT_RESISTOR, VOLTAGE_RESISTOR_UPSTREAM, VOLTAGE_RESISTOR_DOWNSTREAM);
  modulesReg("pow");
  attachInterrupt(pinCF1, hlw8012_cf1_interrupt, CHANGE);
  attachInterrupt(pinCF, hlw8012_cf_interrupt, CHANGE);
  sendStatus(ActivePowerWS, hlw8012.getActivePower());
  sendOptions(alarmpowS, 0);
  alarmLoad(ActivePowerWS, highalarmpowS, lowalarmpowS);
  ts.add(8, t, [&](void*) {
  sendStatus(ActivePowerWS, hlw8012.getActivePower());
  sendOptions(voltagevS, hlw8012.getVoltage());
  sendOptions(currentaS, hlw8012.getCurrent());
  sendOptions(apparentpowervaS, hlw8012.getApparentPower());
  sendOptions(powerfactorS, hlw8012.getPowerFactor());
  sendOptions(aggenergywsS, hlw8012.getEnergy());
  alarmTest(ActivePowerWS, highalarmpowS, lowalarmpowS, alarmpowS);
    }, nullptr, true);
    HTTP.on("/pow.json", HTTP_GET, []() {
      String data = graf3(getStatusFloat(ActivePowerWS), getOptionsFloat(highalarmpowS), getOptionsFloat(lowalarmpowS), 10, t, "low:0");
      httpOkJson(data);
    });
  //calibrate();
}

void calibrate() {

  // Let some time to register values
  unsigned long timeout = millis();
  while ((millis() - timeout) < 10000) {
    delay(1);
  }

  // Calibrate using a 60W bulb (pure resistive) on a 230V line
  hlw8012.expectedActivePower(60.0);
  hlw8012.expectedVoltage(220.0);
  hlw8012.expectedCurrent(60.0 / 220.0);

  sendOptions("current", hlw8012.getCurrentMultiplier());
  sendOptions("voltage", hlw8012.getVoltageMultiplier());
  sendOptions("power", hlw8012.getPowerMultiplier());


}
#endif

