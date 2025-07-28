#include <bms2.h>
#include <SoftwareSerial.h>
#include <U8g2lib.h>
#include <SPI.h>
#include "BleSerialClient.h"
#include "ESP32-smartBMSdisplay.h"
#include "WiFi.h"
// #define DEBUG true

// Conditional debug macros to eliminate debug code in release
#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x, y) Serial.printf(x, y)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x, y)
#endif

void setup() {
  DEBUG_PRINTLN(F("BMS Monitor Init"));
  
  Serial.begin(115200);
  
  if (haveBT) {
    startBluetooth();
  } else {
    MySoftwareSerial.begin(9600);
  }
  
  pinMode(ALARM_PIN, OUTPUT);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  digitalWrite(ALARM_PIN, LOW);
  u8g2.begin(); 
  u8g2.setContrast(1);
  
  DEBUG_PRINTLN(F("Init Complete."));
}

void loop() { 
  bms.begin(&MySerial);
  if (haveBT) {
    while (!SerialBT.connected()) SerialBT.bleLoop();
  }
  
  for(uint8_t i=0; i<3; i++){
    checkProtectionParms();
    bms.main_task(true);
    myNumCells = bms.get_num_cells();
    delay(100);
  }
  
  DEBUG_PRINT(F("Number of Cells:"));
  DEBUG_PRINTLN(myNumCells);
  
  while (SerialBT.connected() || !haveBT){
    bms.main_task(true);
    updateBasic();
    updateCells();
    updateStatus();
    setMyAlarm();
    buzzer();
    updateDisplay();
    if (haveBT) SerialBT.bleLoop();
    
    if (digitalRead(SW2)) depressed = false;
    if (!digitalRead(SW2) && !depressed) {
      depressed = true;
      pagenumber++;
      if (pagenumber == END) pagenumber = 0;
    }
  }
  
  MyAlarm = false;
  digitalWrite(ALARM_PIN, false);
  bms.end();
}

void startBluetooth() {
  String macStr = WiFi.macAddress();
  sscanf(macStr.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X",
         (unsigned int*)&unitMACAddress[0], (unsigned int*)&unitMACAddress[1], 
         (unsigned int*)&unitMACAddress[2], (unsigned int*)&unitMACAddress[3], 
         (unsigned int*)&unitMACAddress[4], (unsigned int*)&unitMACAddress[5]);
  
  unitMACAddress[5] += 2;
  sprintf(deviceName, "BMSMonitor-%02X%02X", unitMACAddress[4], unitMACAddress[5]); 
  SerialBT.begin(deviceName);
  SerialBT.setTimeout(10);
}

void setMyAlarm() {
  MyAlarm = false;
  packOver = false;
  PackUnder = false;
  chargeOvercurrent = false;
  dischargeOvercurrent = false;
  cellOver = false;
  cellUnder = false;
  
  if (PackVoltagef > povt - 0.1) { 
    MyAlarm = packOver = true;
    DEBUG_PRINTLN(F("Pack Overvoltage!"));
  } else if (PackVoltagef < puvt + 0.1) { 
    MyAlarm = PackUnder = true;
    DEBUG_PRINTLN(F("Pack Undervoltage!"));
  } else if (PackCurrentf > coct) { 
    MyAlarm = chargeOvercurrent = true;
    DEBUG_PRINTLN(F("Pack overcurrent!"));
  } else if (PackCurrentf < doct * (-1)) { 
    MyAlarm = dischargeOvercurrent = true;
    DEBUG_PRINTLN(F("Pack Discharge Overcurrent!"));
  } else if (setCellMyAlarm()) { 
    MyAlarm = true;
    DEBUG_PRINTLN(F("Cell Voltage Alarm!"));
  }
}

bool setCellMyAlarm() {
  bool ca = false;
  for (uint8_t c = 0; c < myNumCells; c++){
    if (myCellVoltages[c] > covt) {
      ca = cellOver = true;
    }
    if (myCellVoltages[c] < cuvt) {
      ca = cellUnder = true;
    }
  }
  return ca;
}

void buzzer() {
  if (MyAlarm) {
    if (!digitalRead(SW1)) mute = true; 
    DEBUG_PRINTLN(F("Alarm."));
    if (millis() - buzzerTimestamp >= 100) {    
      myBeep = mute ? LOW : !myBeep;
      DEBUG_PRINTLN(F("Beep."));
      digitalWrite(ALARM_PIN, myBeep);
      buzzerTimestamp = millis();
    }    
  } else {
    DEBUG_PRINTLN(F("No Alarm."));
    digitalWrite(ALARM_PIN, LOW);
    mute = false;
  }  
}

void checkProtectionParms() {
  povt = bms.get_0x20_batt_over_volt_trig() / 1000.0;
  puvt = bms.get_0x22_batt_under_volt_trig() / 1000.0;
  covt = bms.get_0x24_cell_over_volt_trig() / 1000.0;
  cuvt = bms.get_0x26_cell_under_volt_trig() / 1000.0;
  coct = bms.get_0x28_charge_over_current_trig() / 1000.0;
  doct = bms.get_0x29_discharge_over_current_release() / 1000.0;
  
  DEBUG_PRINT(F("Pack Overvoltage:")); DEBUG_PRINTLN(povt);
  DEBUG_PRINT(F("Pack Undervoltage:")); DEBUG_PRINTLN(puvt);
  DEBUG_PRINT(F("Cell Overvoltage:")); DEBUG_PRINTLN(covt);
  DEBUG_PRINT(F("Cell Undervoltage:")); DEBUG_PRINTLN(cuvt);
  DEBUG_PRINT(F("Charge Overcurrent:")); DEBUG_PRINTLN(coct);
  DEBUG_PRINT(F("Discharge Overcurrent:")); DEBUG_PRINTLN(doct);
}

void updateCells() {
  CellMax = 0.0;
  CellMin = 5.0;
  Cellavg = 0.0;
  
  for (uint8_t c = 0; c < myNumCells; c++){
    if (bms.get_cell_voltage(c) != 0) {
      myCellVoltages[c] = bms.get_cell_voltage(c);
      balancerStates[c] = bms.get_balance_status(c);
      if (myCellVoltages[c] > CellMax) CellMax = myCellVoltages[c];
      if (myCellVoltages[c] < CellMin) CellMin = myCellVoltages[c];
      Cellavg += myCellVoltages[c];
      
      DEBUG_PRINT(F("    Cell:"));
      DEBUG_PRINT(c+1);
      DEBUG_PRINT(F(" "));
      DEBUG_PRINT(myCellVoltages[c]);
      DEBUG_PRINT(balancerStates[c] ? '*' : ' ');
    }
  }
  Cellavg = Cellavg / myNumCells;
  Celldiff = CellMax - CellMin;
  DEBUG_PRINTLN();
}

void updateBasic() {
  PackVoltagef = bms.get_voltage();
  PackCurrentf = bms.get_current();
  RSOC = bms.get_state_of_charge();
  RemainCapacityf = bms.get_balance_capacity();
  Temp_probe_1f = bms.get_ntc_temperature(1);
}

void updateStatus() {
  chargeFet = bms.get_charge_mosfet_status();
  dischargeFet = bms.get_discharge_mosfet_status();
  
  DEBUG_PRINT(F("Mosfet Charge = ")); DEBUG_PRINT(chargeFet);
  DEBUG_PRINT(F("  Mosfet DisCharge = ")); DEBUG_PRINTLN(dischargeFet);
  
  prot_status = bms.get_protection_status();
  cellOver = prot_status.single_cell_overvoltage_protection; 
  cellUnder = prot_status.single_cell_undervoltage_protection; 
  packOver = prot_status.whole_pack_overvoltage_protection;
  PackUnder = prot_status.whole_pack_undervoltage_protection;
  chargeOverTemp = prot_status.charging_over_temperature_protection;
  chargeUnderTemp = prot_status.discharge_low_temperature_protection;
  dischargeOverTemp = prot_status.discharge_over_temperature_protection;
  dischargeUnderTemp = prot_status.discharge_low_temperature_protection;
  chargeOvercurrent = prot_status.charging_overcurrent_protection;
  dischargeOvercurrent = prot_status.discharge_overcurrent_protection;
  shortCircuit = prot_status.short_circuit_protection;
  AFEerror = prot_status.front_end_detection_ic_error;
  
  // Simplified debug output
  #ifdef DEBUG
  if(AFEerror) DEBUG_PRINTLN(F("AFE ERROR"));
  else if(shortCircuit) DEBUG_PRINTLN(F("SHORT CIRCUIT"));
  else if(cellOver) DEBUG_PRINTLN(F("Cell Overvoltage"));
  else if(PackCurrentf>0) DEBUG_PRINTLN(F("Charging"));
  else if(PackCurrentf<0) DEBUG_PRINTLN(F("Discharging"));
  else if(PackCurrentf==0) DEBUG_PRINTLN(F("Idle"));
  #endif
}

// Display functions remain the same but with status string optimization
const char* getStatusString() {
  if(AFEerror) return "AFE ERROR";
  if(shortCircuit) return "SHORT CIRCUIT";
  if(cellOver) return "Cell Overvoltage";
  if(cellUnder) return "Cell Undervoltage";
  if(packOver) return "Pack Overvoltage";
  if(PackUnder) return "Pack Undervoltage";
  if(chargeOverTemp) return "Charge Overtemp";
  if(chargeUnderTemp) return "Charge Undertemp";
  if(dischargeOverTemp) return "Discharge Overtemp";
  if(dischargeUnderTemp) return "Discharge Undertemp";
  if(chargeOvercurrent) return "Charge Overcurrent";
  if(dischargeOvercurrent) return "Discharge Overcurrent";
  if(PackCurrentf>0) return "Charging";
  if(PackCurrentf<0) return "Discharging";
  return "Idle";
}

void updateDisplay(){
  switch (pagenumber) {
    case SUMMARY: displaySummary(); break;
    case SOC: displaySOC(); break;
    case VOLTAGE: displayVoltage(); break;
    case AMPS: displayCurrent(); break;
    case CELLS: displayCells(); break;
  }
}

void displaySOC() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_inr24_mf);  
    u8g2.setCursor(2, 32);
    u8g2.print(RSOC);
    u8g2.print(F("%"));
    u8g2.setFont(u8g2_font_7x13_mf); 
    u8g2.setCursor(2, 52);
    u8g2.print(RemainCapacityf,0);
    u8g2.print(F("Ah"));
    u8g2.setCursor(65, 52);
    u8g2.print(RemainCapacityf*12.9,0);
    u8g2.print(F("Wh"));
    u8g2.setFont(u8g2_font_5x8_mf);
    u8g2.setCursor(1, 63);
    u8g2.print(getStatusString());
  } while (u8g2.nextPage());
}

void displayVoltage() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_inr24_mf);
    u8g2.setCursor(1, 32);
    u8g2.print(PackVoltagef,2);
    u8g2.print(F("V"));
    u8g2.setFont(u8g2_font_7x13_mf); 
    u8g2.setCursor(2, 52);
    u8g2.print(PackCurrentf,0);
    u8g2.print(F("A"));
    u8g2.setCursor(62, 52);
    u8g2.print(PackCurrentf*PackVoltagef,0);
    u8g2.print(F("W"));
    u8g2.setFont(u8g2_font_5x8_mf);
    u8g2.setCursor(1, 63);
    u8g2.print(getStatusString());
  } while (u8g2.nextPage());
}

void displayCurrent() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_inr24_mf);  
    u8g2.setCursor(2, 32);
    u8g2.print(PackCurrentf,1);
    u8g2.print(F("A"));
    u8g2.setFont(u8g2_font_7x13_mf); 
    u8g2.setCursor(2, 52);
    u8g2.print(PackVoltagef,2);
    u8g2.print(F("V"));
    u8g2.setCursor(62, 52);
    u8g2.print(PackCurrentf*PackVoltagef,0);
    u8g2.print(F("W"));
    u8g2.setFont(u8g2_font_5x8_mf);
    u8g2.setCursor(1, 63);
    u8g2.print(getStatusString());
  } while (u8g2.nextPage());
}

void displayCells() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x13_mf);
    u8g2.setCursor(1, 12);
    u8g2.print(myCellVoltages[0],3); u8g2.print(F("v"));
    if(balancerStates[0]) u8g2.print(F("*"));
    u8g2.setCursor(1, 24);
    u8g2.print(myCellVoltages[1],3); u8g2.print(F("v"));
    if(balancerStates[1]) u8g2.print(F("*"));
    u8g2.setCursor(1, 36);
    u8g2.print(myCellVoltages[2],3); u8g2.print(F("v"));
    if(balancerStates[2]) u8g2.print(F("*"));
    u8g2.setCursor(1, 48);
    u8g2.print(myCellVoltages[3],3); u8g2.print(F("v"));
    if(balancerStates[3]) u8g2.print(F("*"));
    
    u8g2.setCursor(62, 12);
    u8g2.print(CellMin,3); u8g2.print(F("min"));
    u8g2.setCursor(62, 24);
    u8g2.print(CellMax,3); u8g2.print(F("max"));
    u8g2.setCursor(62, 36);
    u8g2.print(Celldiff,3); u8g2.print(F("diff"));
    u8g2.drawFrame(58,0,67,39);
    
    u8g2.setFont(u8g2_font_5x8_mf);
    u8g2.setCursor(1, 63);
    u8g2.print(getStatusString());
    u8g2.setCursor(97, 55);
    u8g2.print(F("C:")); u8g2.print(chargeFet ? F("ON") : F("OFF"));
    u8g2.setCursor(97, 63); 
    u8g2.print(F("D:")); u8g2.print(dischargeFet ? F("ON") : F("OFF"));
  } while (u8g2.nextPage());
}

void displaySummary() {
  u8g2.firstPage();
  do {
    u8g2.drawFrame(0,0,58,39);
    u8g2.setFont(u8g2_font_7x13_mf);
    u8g2.setCursor(3, 12); u8g2.print(PackVoltagef,2); u8g2.setCursor(49, 12); u8g2.print(F("V"));
    u8g2.setCursor(3, 24); u8g2.print(PackCurrentf,1); u8g2.setCursor(49, 24); u8g2.print(F("A"));
    u8g2.setCursor(3, 36); u8g2.print(PackVoltagef *PackCurrentf,1); u8g2.setCursor(49, 36); u8g2.print(F("W"));
    u8g2.setCursor(67, 12); u8g2.print(RSOC); u8g2.print(F("%"));
    u8g2.setCursor(67, 24); u8g2.print(RemainCapacityf,0); u8g2.print(F("Ah"));
    u8g2.setCursor(67, 36); u8g2.print(Temp_probe_1f,1); u8g2.print(F("c"));
    
    u8g2.setFont(u8g2_font_5x8_mf);
    u8g2.setCursor(2, 47);
    u8g2.print(getStatusString());
    
    u8g2.setCursor(2, 55);
    u8g2.print(myCellVoltages[0],3); u8g2.print(F("v"));
    if(balancerStates[0]) u8g2.print(F("*"));
    u8g2.setCursor(40, 55);
    u8g2.print(myCellVoltages[1],3); u8g2.print(F("v"));
    if(balancerStates[1]) u8g2.print(F("*"));
    u8g2.setCursor(97, 55);
    u8g2.print(F("C:")); u8g2.print(chargeFet ? F("ON") : F("OFF"));
    
    u8g2.setCursor(2, 63);
    u8g2.print(myCellVoltages[2],3); u8g2.print(F("v"));
    if(balancerStates[2]) u8g2.print(F("*"));
    u8g2.setCursor(40, 63);
    u8g2.print(myCellVoltages[3],3); u8g2.print(F("v"));
    if(balancerStates[3]) u8g2.print(F("*"));
    u8g2.setCursor(97, 63);
    u8g2.print(F("D:")); u8g2.print(dischargeFet ? F("ON") : F("OFF"));
  } while (u8g2.nextPage());
}
