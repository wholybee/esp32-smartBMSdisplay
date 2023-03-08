//#define MySerial MySoftwareSerial
#define MySerial SerialBT
static bool haveBT = true;

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
bool MyAlarm = false;
bool myBeep = false;

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
bool chargeFet = false;
bool dischargeFet = false;
bool cellOver = false;
bool cellUnder = false;
bool packOver = false;
bool PackUnder = false;
bool chargeOverTemp = false;
bool chargeUnderTemp = false;
bool dischargeOverTemp = false;
bool dischargeUnderTemp = false;
bool chargeOvercurrent = false;
bool dischargeOvercurrent = false;
bool shortCircuit = false;
bool AFEerror = false;


// objects we need

SoftwareSerial MySoftwareSerial(RX, TX);
U8G2_SSD1309_128X64_NONAME2_1_4W_SW_SPI u8g2( U8G2_R2, SCL , SDA  , CS , DC , RST );
OverkillSolarBms2 bms;
ProtectionStatus prot_status;
