#include <bms2.h>
#include <SoftwareSerial.h> // Second serial port for connecting to the BMS
#include <U8g2lib.h> // Needed for the display
#include <SPI.h> // Needed for the display
#include "BleSerialClient.h"
#include "ESP32-smartBMSdisplay.h"
// define DEBUG true

void setup() {
#ifdef DEBUG
Serial.println(F("BMS Monitor Init"));
#endif

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
u8g2.setContrast(1); // 0-255 display brightness
#ifdef DEBUG
Serial.println(F("Init Complete."));
#endif

}

void loop() { 
bms.begin(&MySerial);
if (haveBT) {
    while (!SerialBT.connected()) SerialBT.bleLoop();
}
  for(int i=0;i<3;i++){
    checkProtectionParms();
    bms.main_task(true);
    myNumCells = bms.get_num_cells();
    delay(100);
   }
#ifdef DEBUG    
    Serial.print(F("Number of Cells:"));
    Serial.println(myNumCells);
#endif

    while (SerialBT.connected() || !haveBT){
            bms.main_task(true);
            updateBasic();      // Pack Voltage, Current, SOC, etc.
            updateCells();      // Cell volages and Balancer status
            updateStatus();     // Protection information
            setMyAlarm ();        // Do we need to sound MyAlarm of pending protection?
            buzzer();            
            updateDisplay();
            if (haveBT==true) SerialBT.bleLoop();
              if (digitalRead(SW2)) depressed = false;
              if (!digitalRead(SW2) & !depressed) {
                    depressed = true;
                    pagenumber++;
                    if (pagenumber==END) pagenumber = 1;
                   // timestamp_100ms = millis();
                      
                
              }
    }

    MyAlarm = false;
    digitalWrite (ALARM_PIN, false);
    bms.end();
            
}



void startBluetooth() {
  // Get unit MAC address
  esp_read_mac(unitMACAddress, ESP_MAC_WIFI_STA);
  
  // Convert MAC address to Bluetooth MAC (add 2): https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html#mac-address
  unitMACAddress[5] += 2;                                                          
  
  //Create device name
  sprintf(deviceName, "BMSMonitor-%02X%02X", unitMACAddress[4], unitMACAddress[5]); 

  //Init BLE Serial
  SerialBT.begin(deviceName);
  SerialBT.setTimeout(10);
}

void setMyAlarm (void) {
  if (PackVoltagef > povt) { 
      MyAlarm = true; 
      packOver = true;
      #ifdef DEBUG
      Serial.println ("Pack Overvoltage!");
      #endif
      }
  else if (PackVoltagef < puvt) { 
      MyAlarm = true; 
      PackUnder = true;
      #ifdef DEBUG
      Serial.println ("Pack Undervoltage!");
      #endif
      }
  else if (PackCurrentf > coct) { 
      MyAlarm = true; 
      chargeOvercurrent = true;
      #ifdef DEBUG
      Serial.println ("Pack overcurrent!");
      #endif
      }
  else if (PackCurrentf < doct*(-1)) { 
      MyAlarm = true; 
      dischargeOvercurrent = true;
      #ifdef DEBUG
      Serial.println ("Pack Discharge Overcurrent!");
      #endif
      }
  else if (setCellMyAlarm()) { 
      MyAlarm = true; 
      #ifdef DEBUG
      Serial.println ("Cell Voltage Alarm!");
      #endif
      }
  else MyAlarm = false;
  
}

bool setCellMyAlarm (void) {
  bool ca = false;
  for (int c = 0;c < myNumCells;c++){
            if (myCellVoltages[c]>covt) {
              ca = true;
              cellOver = true;
            }
            if (myCellVoltages[c]<cuvt) {
              ca = true;
              cellUnder = true;
            }
  }
  return ca;
}

void buzzer (void) {
  
  if (MyAlarm == true) {
    if (!digitalRead (SW1)) mute = true; 
    Serial.println("Alarm.");
    if (millis() - buzzerTimestamp >= 100) {    
      if (mute) { 
        myBeep = LOW; 
      } else { 
        myBeep = !myBeep;
      }
#ifdef DEBUG
      if (myBeep)  Serial.println("Beep.");
#endif
      digitalWrite (ALARM_PIN, myBeep);
      buzzerTimestamp = millis();
    }    
  } else {
#ifdef DEBUG
    Serial.println("No Alarm.");
#endif
    digitalWrite (ALARM_PIN, LOW);
    mute = false;
  }  
}



void checkProtectionParms (void) {
povt = bms.get_0x20_batt_over_volt_trig() / 1000.0;
puvt = bms.get_0x22_batt_under_volt_trig() / 1000.0;
covt = bms.get_0x24_cell_over_volt_trig() / 1000.0;
cuvt = bms.get_0x26_cell_under_volt_trig() / 1000.0;
coct = bms.get_0x28_charge_over_current_trig() / 1000.0;
doct = bms.get_0x29_discharge_over_current_release() / 1000.0;
#ifdef DEBUG  
  Serial.print("Pack Overvoltage:");
  Serial.println(povt);

  Serial.print("Pack Undervoltage:");
  Serial.println(puvt);

  Serial.print("Cell Overvoltage:");
  Serial.println(covt);

  Serial.print("Cell Undervoltage:");
  Serial.println(cuvt);

  Serial.print("Charge Overcurrent:");
  Serial.println(coct);

  Serial.print("Discharge Overcurrent:");
  Serial.println(doct);
#endif
}


void updateCells (void) {
    CellMax = 0.0;
    CellMin = 5.0;
    Cellavg = 0.0;
    for (int c = 0;c < myNumCells;c++){
      if (bms.get_cell_voltage(c) != 0) {
        myCellVoltages[c]=bms.get_cell_voltage(c);
        balancerStates[c]=bms.get_balance_status(c);
        if (myCellVoltages[c] > CellMax ) CellMax = myCellVoltages[c];
        if (myCellVoltages[c] < CellMin ) CellMin = myCellVoltages[c];
        Cellavg += myCellVoltages[c];
    }
    Cellavg = Cellavg / myNumCells;
    Celldiff = CellMax - CellMin;
#ifdef DEBUG
    Serial.print("    Cell:");
    Serial.print(c+1);
    Serial.print(" ");
    Serial.print(myCellVoltages[c]);
    Serial.print(balancerStates[c]?'*':' ');
#endif
  }
#ifdef DEBUG
  Serial.println();
#endif

}



void updateBasic (void) {
            // bms.query_0x03_basic_info();
            PackVoltagef = bms.get_voltage();
            PackCurrentf = bms.get_current();
            RSOC = bms.get_state_of_charge();
            RemainCapacityf = bms.get_balance_capacity();
            Temp_probe_1f = bms.get_ntc_temperature(1);

}


void updateStatus (void) {
  // Mosfets
  chargeFet = bms.get_charge_mosfet_status();
  dischargeFet = bms.get_discharge_mosfet_status();

#ifdef DEBUG
  Serial.print(F("Mosfet Charge = "));
  Serial.print(chargeFet);
  Serial.print(F("  Mosfet DisCharge = "));
  Serial.println(dischargeFet);
#endif

  // Pack Protection states
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

#ifdef DEBUG
  if(AFEerror) Serial.println(F("AFE ERROR"));
  else if(shortCircuit) Serial.println(F("SHORT CIRCUIT"));
  else if(cellOver) Serial.println(F("Cell Overvoltage"));
  else if(cellUnder) Serial.println(F("Cell Undervoltage"));
  else if(packOver) Serial.println(F("Pack Overvoltage"));
  else if(PackUnder) Serial.println(F("Pack Undervoltage"));
  else if(chargeOverTemp) Serial.println(F("Charge Overtemp"));
  else if(chargeUnderTemp) Serial.println(F("Charge Undertemp"));
  else if(dischargeOverTemp) Serial.println(F("Discharge Overtemp"));
  else if(dischargeUnderTemp) Serial.println(F("Discharge Undertemp"));
  else if(chargeOvercurrent) Serial.println(F("Charge Overcurrent"));
  else if(dischargeOvercurrent) Serial.println(F("Discharge Overcurrent"));
  else if(PackCurrentf>0) Serial.println(F("Charging"));
  else if(PackCurrentf<0) Serial.println(F("Discharging"));
  else if(PackCurrentf==0) Serial.println(F("Idle"));
#endif  


}

void updateDisplay(void){

  switch (pagenumber) {
    case SUMMARY:
      displaySummary();
      break;

    case SOC:
      displaySOC();
      break;
    
    case VOLTAGE:
      displayVoltage();
      break;

    case AMPS:
      displayCurrent();
      break;

    case CELLS:
      displayCells();
      break;
  }

}

void displaySOC (void) {
u8g2.firstPage();
  do {
 
    //SOC Meter
  u8g2.setFont(u8g2_font_inr24_mf);  
  u8g2.setCursor(5, 32);
  float stateOfCharge = RSOC;
  u8g2.print(stateOfCharge,0);
  u8g2.print("%");
  u8g2.setFont(u8g2_font_7x13_mf); 
  u8g2.setCursor(5, 52);
  u8g2.print(RemainCapacityf,0);
  u8g2.print("Ah");
  u8g2.setCursor(65, 52);
  u8g2.print(RemainCapacityf*12.9,0);
  u8g2.print("Wh");

    //Pack status
  u8g2.setFont(u8g2_font_5x8_mf); // choose font 
  u8g2.setCursor(4, 63);
  if(AFEerror) u8g2.print(F("AFE ERROR"));
  else if(shortCircuit) u8g2.print(F("SHORT CIRCUIT"));
  else if(cellOver) u8g2.print(F("Cell Overvoltage"));
  else if(cellUnder) u8g2.print(F("Cell Undervoltage"));
  else if(packOver) u8g2.print(F("Pack Overvoltage"));
  else if(PackUnder) u8g2.print(F("Pack Undervoltage"));
  else if(chargeOverTemp) u8g2.print(F("Charge Overtemp"));
  else if(chargeUnderTemp) u8g2.print(F("Charge Undertemp"));
  else if(dischargeOverTemp) u8g2.print(F("Discharge Overtemp"));
  else if(dischargeUnderTemp) u8g2.print(F("Discharge Undertemp"));
  else if(chargeOvercurrent) u8g2.print(F("Charge Overcurrent"));
  else if(dischargeOvercurrent) u8g2.print(F("Discharge Overcurrent"));
  else if(PackCurrentf>0) u8g2.print(F("Charging"));
  else if(PackCurrentf<0) u8g2.print(F("Discharging"));
  else if(PackCurrentf==0) u8g2.print(F("Idle"));
} while (u8g2.nextPage());

}

void displayVoltage (void) {
u8g2.firstPage();
  do {
 
    //Volt Meter
  u8g2.setFont(u8g2_font_inr24_mf);   //  u8g2_font_fub30_tr
  u8g2.setCursor(4, 32);
  u8g2.print(PackVoltagef,2);
  u8g2.print("V");
  u8g2.setFont(u8g2_font_7x13_mf); 
  u8g2.setCursor(5, 52);
  u8g2.print(PackCurrentf,0);
  u8g2.print("A");
  u8g2.setCursor(65, 52);
  u8g2.print(PackCurrentf*PackVoltagef,0);
  u8g2.print("W");

    //Pack status
  u8g2.setFont(u8g2_font_5x8_mf); // choose font 
  u8g2.setCursor(4, 63);
  if(AFEerror) u8g2.print(F("AFE ERROR"));
  else if(shortCircuit) u8g2.print(F("SHORT CIRCUIT"));
  else if(cellOver) u8g2.print(F("Cell Overvoltage"));
  else if(cellUnder) u8g2.print(F("Cell Undervoltage"));
  else if(packOver) u8g2.print(F("Pack Overvoltage"));
  else if(PackUnder) u8g2.print(F("Pack Undervoltage"));
  else if(chargeOverTemp) u8g2.print(F("Charge Overtemp"));
  else if(chargeUnderTemp) u8g2.print(F("Charge Undertemp"));
  else if(dischargeOverTemp) u8g2.print(F("Discharge Overtemp"));
  else if(dischargeUnderTemp) u8g2.print(F("Discharge Undertemp"));
  else if(chargeOvercurrent) u8g2.print(F("Charge Overcurrent"));
  else if(dischargeOvercurrent) u8g2.print(F("Discharge Overcurrent"));
  else if(PackCurrentf>0) u8g2.print(F("Charging"));
  else if(PackCurrentf<0) u8g2.print(F("Discharging"));
  else if(PackCurrentf==0) u8g2.print(F("Idle"));
} while (u8g2.nextPage());

}


void displayCurrent (void) {
u8g2.firstPage();
  do {
 
    //Amp Meter
  u8g2.setFont(u8g2_font_inr24_mf);  
  u8g2.setCursor(5, 32);
  u8g2.print(PackCurrentf,1);
  u8g2.print("A");
  u8g2.setFont(u8g2_font_7x13_mf); 
  u8g2.setCursor(5, 52);
  u8g2.print(PackVoltagef,2);
  u8g2.print("V");
  u8g2.setCursor(65, 52);
  u8g2.print(PackCurrentf*PackVoltagef,0);
  u8g2.print("W");

    //Pack status
  u8g2.setFont(u8g2_font_5x8_mf); // choose font 
  u8g2.setCursor(4, 63);
  if(AFEerror) u8g2.print(F("AFE ERROR"));
  else if(shortCircuit) u8g2.print(F("SHORT CIRCUIT"));
  else if(cellOver) u8g2.print(F("Cell Overvoltage"));
  else if(cellUnder) u8g2.print(F("Cell Undervoltage"));
  else if(packOver) u8g2.print(F("Pack Overvoltage"));
  else if(PackUnder) u8g2.print(F("Pack Undervoltage"));
  else if(chargeOverTemp) u8g2.print(F("Charge Overtemp"));
  else if(chargeUnderTemp) u8g2.print(F("Charge Undertemp"));
  else if(dischargeOverTemp) u8g2.print(F("Discharge Overtemp"));
  else if(dischargeUnderTemp) u8g2.print(F("Discharge Undertemp"));
  else if(chargeOvercurrent) u8g2.print(F("Charge Overcurrent"));
  else if(dischargeOvercurrent) u8g2.print(F("Discharge Overcurrent"));
  else if(PackCurrentf>0) u8g2.print(F("Charging"));
  else if(PackCurrentf<0) u8g2.print(F("Discharging"));
  else if(PackCurrentf==0) u8g2.print(F("Idle"));
} while (u8g2.nextPage());

}

void displayCells (void) {

u8g2.firstPage();
  do {

  //Cell States
  u8g2.setFont(u8g2_font_7x13_mf); // u8g2.setFont(u8g2_font_9x15_mf);
  u8g2.setCursor(4, 12);
  u8g2.print(myCellVoltages[0],3);
  u8g2.print("v");
  if(balancerStates[0]==1) u8g2.print("*");
  u8g2.setCursor(4, 24);
  u8g2.print(myCellVoltages[1],3);
  u8g2.print("v");
  if(balancerStates[1]==1) u8g2.print("*");

  
  u8g2.setCursor(4, 36);
  u8g2.print(myCellVoltages[2],3);
  u8g2.print("v");
  if(balancerStates[2]==1) u8g2.print("*");
  u8g2.setCursor(4, 48);
  u8g2.print(myCellVoltages[3],3);
  u8g2.print("v");
  if(balancerStates[3]==1) u8g2.print("*");



// Min,Max,Delta

  
  u8g2.setFont(u8g2_font_7x13_mf);
  u8g2.setCursor(65, 12);
  u8g2.print(CellMin,3); u8g2.print("min");
  u8g2.setCursor(65, 24);
  u8g2.print(CellMax,3); u8g2.print("max");
  u8g2.setCursor(65, 36);
  u8g2.print(Celldiff,3); u8g2.print("diff");
  u8g2.drawFrame(61,0,67,39); // u8g2.drawLine(61, 0, 61, 39);

  //Pack status
  u8g2.setFont(u8g2_font_5x8_mf); // choose font 
  u8g2.setCursor(4, 63);
  if(AFEerror) u8g2.print(F("AFE ERROR"));
  else if(shortCircuit) u8g2.print(F("SHORT CIRCUIT"));
  else if(cellOver) u8g2.print(F("Cell Overvoltage"));
  else if(cellUnder) u8g2.print(F("Cell Undervoltage"));
  else if(packOver) u8g2.print(F("Pack Overvoltage"));
  else if(PackUnder) u8g2.print(F("Pack Undervoltage"));
  else if(chargeOverTemp) u8g2.print(F("Charge Overtemp"));
  else if(chargeUnderTemp) u8g2.print(F("Charge Undertemp"));
  else if(dischargeOverTemp) u8g2.print(F("Discharge Overtemp"));
  else if(dischargeUnderTemp) u8g2.print(F("Discharge Undertemp"));
  else if(chargeOvercurrent) u8g2.print(F("Charge Overcurrent"));
  else if(dischargeOvercurrent) u8g2.print(F("Discharge Overcurrent"));
  else if(PackCurrentf>0) u8g2.print(F("Charging"));
  else if(PackCurrentf<0) u8g2.print(F("Discharging"));
  else if(PackCurrentf==0) u8g2.print(F("Idle"));

 // Mosfet Status
  u8g2.setFont(u8g2_font_5x8_mf);
  u8g2.setCursor(100, 55);
  u8g2.print("C:");
  if(chargeFet)u8g2.print("ON");
  else u8g2.print("OFF");
  u8g2.setCursor(100, 63); 
  u8g2.print("D:");
  if(dischargeFet)u8g2.print("ON");
  else u8g2.print("OFF");

} while (u8g2.nextPage());


}


void displaySummary (void) {

u8g2.firstPage();
  do {

  // Power Meter
  u8g2.drawFrame(3,0,61,39);
  u8g2.setFont(u8g2_font_7x13_mf); // choose font 
  u8g2.setCursor(6, 12);
  u8g2.print(PackVoltagef,2);
  u8g2.setCursor(52, 12);
  u8g2.print("V");
  u8g2.setCursor(6, 24);
  u8g2.print(PackCurrentf,1);
  u8g2.setCursor(52, 24);
  u8g2.print("A");
  u8g2.setCursor(6, 36);
  u8g2.print(PackVoltagef *PackCurrentf,1);
  u8g2.setCursor(52, 36);
  u8g2.print("W");

  //SOC Meter
  u8g2.setCursor(70, 12);
  float stateOfCharge = RSOC;
  u8g2.print(stateOfCharge,0);
  u8g2.print("%");
  u8g2.setCursor(70, 24);
  u8g2.print(RemainCapacityf,0);
  u8g2.print("Ah");
  u8g2.setCursor(70, 36);
  u8g2.print(Temp_probe_1f,1);
  u8g2.print("c");

  //Pack status
  u8g2.setFont(u8g2_font_5x8_mf); // choose font 
  u8g2.setCursor(4, 47);
  if(AFEerror) u8g2.print(F("AFE ERROR"));
  else if(shortCircuit) u8g2.print(F("SHORT CIRCUIT"));
  else if(cellOver) u8g2.print(F("Cell Overvoltage"));
  else if(cellUnder) u8g2.print(F("Cell Undervoltage"));
  else if(packOver) u8g2.print(F("Pack Overvoltage"));
  else if(PackUnder) u8g2.print(F("Pack Undervoltage"));
  else if(chargeOverTemp) u8g2.print(F("Charge Overtemp"));
  else if(chargeUnderTemp) u8g2.print(F("Charge Undertemp"));
  else if(dischargeOverTemp) u8g2.print(F("Discharge Overtemp"));
  else if(dischargeUnderTemp) u8g2.print(F("Discharge Undertemp"));
  else if(chargeOvercurrent) u8g2.print(F("Charge Overcurrent"));
  else if(dischargeOvercurrent) u8g2.print(F("Discharge Overcurrent"));
  else if(PackCurrentf>0) u8g2.print(F("Charging"));
  else if(PackCurrentf<0) u8g2.print(F("Discharging"));
  else if(PackCurrentf==0) u8g2.print(F("Idle"));

  //Cell and MOSFET States
  u8g2.setCursor(4, 55);
  u8g2.print(myCellVoltages[0],3);
  u8g2.print("v");
  if(balancerStates[0]==1) u8g2.print("*");
  u8g2.setCursor(43, 55);
  u8g2.print(myCellVoltages[1],3);
  u8g2.print("v");
  if(balancerStates[1]==1) u8g2.print("*");
  u8g2.setCursor(100, 55);
  u8g2.print("C:");
  if(chargeFet)u8g2.print("ON");
  else u8g2.print("OFF");
  
  u8g2.setCursor(4, 63);
  u8g2.print(myCellVoltages[2],3);
  u8g2.print("v");
  if(balancerStates[2]==1) u8g2.print("*");
  u8g2.setCursor(43, 63);
  u8g2.print(myCellVoltages[3],3);
  u8g2.print("v");
  if(balancerStates[3]==1) u8g2.print("*");
  u8g2.setCursor(100, 63);
  u8g2.print("D:");
  if(dischargeFet)u8g2.print("ON");
  else u8g2.print("OFF");
  
  } while (u8g2.nextPage());


}

