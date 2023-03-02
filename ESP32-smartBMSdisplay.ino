#include <bms2.h>
#include <SoftwareSerial.h> // Second serial port for connecting to the BMS
#include <U8g2lib.h> // Needed for the display
#include <SPI.h> // Needed for the display
// includes for bt
#include "BleSerialClient.h"


//#define MySerial MySoftwareSerial
#define MySerial SerialBT
static bool haveBT = true;

#define TRUE 1
#define FALSE 0
#define ALARM_PIN 16
#define SCL 25
#define SDA 26
#define RST 27
#define DC 14
#define CS 12

#define RX 25
#define TX 27



static BleSerialClient SerialBT;
uint8_t unitMACAddress[6];  // Use MAC address in BT broadcast and display
char deviceName[20];        // The serial string that is broadcast.

uint32_t timestamp_100ms = 0;
uint32_t buzzerTimestamp = 0;

float povt = 15.0;    // pack over voltage trigger
float puvt = 9.6;   // pack under voltage trigger
float covt = 3.750;  // cell over voltage trigger
float cuvt = 2.4;   // cell under voltage trigger
float coct = 120.0;   // charge over current trigger
float doct = 120.0;   // discharge over current trigger
bool MyAlarm = FALSE;


// Global battery stat variables (For printing to displays)
float CellMin = 5.0; // Default value > max possible cell votlage
float CellMax = 0.0;
float Cellavg = 0.0; 
float Celldiff=0.0;
float myCellVoltages[8] = {0,0,0,0,0,0,0,0};
int balancerStates[8] = {0,0,0,0,0,0,0,0};
int myNumCells = 0;
float PackVoltagef = 0.0;
float PackCurrentf = 0.0;
float RemainCapacityf = 0.0;
int RSOC = 0;
float Temp_probe_1f = 0.0;
float Temp_probe_2f = 0.0;
bool chargeFet = FALSE;
bool dischargeFet = FALSE;
bool cellOver = FALSE;
bool cellUnder = FALSE;
bool packOver = FALSE;
bool PackUnder = FALSE;
bool chargeOverTemp = FALSE;
bool chargeUnderTemp = FALSE;
bool dischargeOverTemp = FALSE;
bool dischargeUnderTemp = FALSE;
bool chargeOvercurrent = FALSE;
bool dischargeOvercurrent = FALSE;
bool shortCircuit = FALSE;
bool AFEerror = FALSE;


// objects we need

SoftwareSerial MySoftwareSerial(RX, TX);
U8G2_SSD1309_128X64_NONAME2_1_4W_SW_SPI u8g2( U8G2_R2, SCL , SDA  , CS , DC , RST );
OverkillSolarBms2 bms;
ProtectionStatus prot_status;


void setup() {
Serial.println(F("BMS Monitor Init"));

Serial.begin(115200);
MySoftwareSerial.begin(9600);
if (haveBT) {
  startBluetooth();
  SerialBT.bleLoop(); 
}
pinMode(ALARM_PIN, OUTPUT);
digitalWrite(ALARM_PIN, LOW);


bms.begin(&MySerial);
u8g2.begin(); 
u8g2.setContrast(1); // 0-255 display brightness


timestamp_100ms = millis();
log_i("bms.main_task start");
  while(1) {
      bms.main_task(true);
      if (haveBT==true) SerialBT.bleLoop();      
      if ((millis()-timestamp_100ms) >= 10000) {
          break;
      }
      
  }

checkProtectionParms();
myNumCells = bms.get_num_cells();
Serial.print(F("Number of Cells:"));
Serial.println(myNumCells);
Serial.println(F("Init Complete."));
}

void loop() { 
//        if (millis() - timestamp_100ms >= 50) {
            myNumCells = bms.get_num_cells();
            bms.main_task(true);
            updateBasic();      // Pack Voltage, Current, SOC, etc.
            updateCells();      // Cell volages and Balancer status
            updateStatus();     // Protection information
            setMyAlarm ();        // Do we need to sound MyAlarm of pending protection?            
            updateDisplay();
            if (haveBT==true) SerialBT.bleLoop();
            timestamp_100ms = millis();            
 //       }
        buzzer ();
        
}



void startBluetooth() {
  // Get unit MAC address
  esp_read_mac(unitMACAddress, ESP_MAC_WIFI_STA);
  
  // Convert MAC address to Bluetooth MAC (add 2): https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html#mac-address
  unitMACAddress[5] += 2;                                                          
  
  //Create device name
  sprintf(deviceName, "BleBridge-%02X%02X", unitMACAddress[4], unitMACAddress[5]); 

  //Init BLE Serial
  SerialBT.begin(deviceName);
  SerialBT.setTimeout(10);
}



void setMyAlarm (void) {
  if (PackVoltagef > povt) MyAlarm = TRUE;
  else if (PackVoltagef < puvt) MyAlarm = TRUE;
  else if (PackCurrentf > coct) MyAlarm = TRUE;
  else if (PackCurrentf < doct) MyAlarm = TRUE;
  else if (setCellMyAlarm()) MyAlarm = TRUE;
  else MyAlarm = FALSE;
  
}

bool setCellMyAlarm (void) {
  bool ca = FALSE;
  for (int c = 0;c < myNumCells;c++){
            if (myCellVoltages[c]>covt) ca = TRUE;
            if (myCellVoltages[c]<cuvt) ca = TRUE;
  }
  return ca;
}

void buzzer (void) {
  if (MyAlarm == FALSE) digitalWrite (ALARM_PIN, LOW);
  else {
    if (millis() - buzzerTimestamp >= 100) {
      bool b = !(digitalRead(ALARM_PIN));
      digitalWrite (ALARM_PIN, b);
      buzzerTimestamp = millis();
    }
  }
}



void checkProtectionParms (void) {
povt = bms.get_0x20_batt_over_volt_trig() / 1000;
puvt = bms.get_0x22_batt_under_volt_trig() / 1000;
covt = bms.get_0x24_cell_over_volt_trig() / 1000;
cuvt = bms.get_0x26_cell_under_volt_trig() / 1000;
coct = bms.get_0x28_charge_over_current_trig() / 1000;
doct = bms.get_0x29_discharge_over_current_release() /1000;
  
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

}


void updateCells (void) {
  
  for (int c = 0;c < myNumCells;c++){
    myCellVoltages[c]=bms.get_cell_voltage(c);
    balancerStates[c]=bms.get_balance_status(c);
    Serial.print("    Cell:");
    Serial.print(c+1);
    Serial.print(" ");
    Serial.print(myCellVoltages[c]);
    Serial.print(balancerStates[c]?'*':' ');

  }
  Serial.println();

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


  Serial.print(F("Mosfet Charge = "));
  Serial.print(chargeFet);
  Serial.print(F("  Mosfet DisCharge = "));
  Serial.println(dischargeFet);


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
  


}

void updateDisplay(void){
  u8g2.firstPage();
  do {

  // Power Meter
  u8g2.drawFrame(0,0,61,39);
  u8g2.setFont(u8g2_font_7x13_mf); // choose font 
  u8g2.setCursor(3, 12);
  u8g2.print(PackVoltagef,2);
  u8g2.setCursor(52, 12);
  u8g2.print("V");
  u8g2.setCursor(3, 24);
  u8g2.print(PackCurrentf,1);
  u8g2.setCursor(52, 24);
  u8g2.print("A");
  u8g2.setCursor(3, 36);
  u8g2.print(PackVoltagef *PackCurrentf,1);
  u8g2.setCursor(52, 36);
  u8g2.print("W");

  //SOC Meter
  u8g2.setCursor(70, 12);
  float stateOfCharge = RSOC;
  u8g2.print(stateOfCharge,0);
  u8g2.print("%");
  u8g2.setCursor(70, 24);
  u8g2.print(RemainCapacityf*12.8,0);
  u8g2.print("Wh");
  u8g2.setCursor(70, 36);
  u8g2.print(Temp_probe_1f,1);
  u8g2.print("c");

  //Pack status
  u8g2.setFont(u8g2_font_5x8_mf); // choose font 
  u8g2.setCursor(1, 47);
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
  u8g2.setCursor(1, 55);
  u8g2.print(myCellVoltages[0],3);
  u8g2.print("v");
  if(balancerStates[0]==1) u8g2.print("*");
  u8g2.setCursor(40, 55);
  u8g2.print(myCellVoltages[1],3);
  u8g2.print("v");
  if(balancerStates[1]==1) u8g2.print("*");
  u8g2.setCursor(100, 55);
  u8g2.print("C:");
  if(chargeFet)u8g2.print("ON");
  else u8g2.print("OFF");
  
  u8g2.setCursor(1, 63);
  u8g2.print(myCellVoltages[2],3);
  u8g2.print("v");
  if(balancerStates[2]==1) u8g2.print("*");
  u8g2.setCursor(40, 63);
  u8g2.print(myCellVoltages[3],3);
  u8g2.print("v");
  if(balancerStates[3]==1) u8g2.print("*");
  u8g2.setCursor(100, 63);
  u8g2.print("D:");
  if(dischargeFet)u8g2.print("ON");
  else u8g2.print("OFF");
  
  } while (u8g2.nextPage());
}
