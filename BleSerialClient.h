#pragma once
#include "BLEDevice.h"
#include "ByteRingBuffer.h"


#define BLE_BUFFER_SIZE ESP_GATT_MAX_ATTR_LEN //must be greater than MTU, less than ESP_GATT_MAX_ATTR_LEN
#define MIN_MTU 50
#define RX_BUFFER_SIZE 4096
#define FLUSH_TIME 1000

class BleSerialClient : public BLECharacteristicCallbacks, public BLEClientCallbacks, public BLEAdvertisedDeviceCallbacks, public Stream
{


public:

// methods

	BleSerialClient();
  void begin(const char *name, bool enable_led = false, int led_pin = 13);
	void end();
	// void onRead(BLERemoteCharacteristic *pCharacteristic);
	int available();
	int read();
	size_t readBytes(uint8_t *buffer, size_t bufferSize);
	int peek();
	size_t write(uint8_t byte);
	void flush();
	size_t write(const uint8_t *buffer, size_t bufferSize);
	size_t print(const char *value);
	void onConnect(BLEClient *pClient);
	void onDisconnect(BLEClient *pClient);
  void onResult(BLEAdvertisedDevice advertisedDevice);
  void bleLoop();	
  bool connectToServer();
  bool connected();
  static  ByteRingBuffer<RX_BUFFER_SIZE> receiveBuffer;

// objects
      	BLEClient *pClient;
        BLEAdvertisedDevice* myDevice;
	      BLEAdvertising *pAdvertising;
	      BLEDevice *pBLEDevice;



//Serial Characteristics
	BLERemoteCharacteristic *TxCharacteristic;
	BLERemoteCharacteristic *RxCharacteristic;

// primative types
	bool enableLed = false;
	int ledPin = 13;

protected:
	size_t transmitBufferLength;
	bool bleConnected;

private:

// methods
  bool checkMTU();
	BleSerialClient(BleSerialClient const &other) = delete;		 // disable copy constructor
	void operator=(BleSerialClient const &other) = delete; // disable assign constructor
  void bleScan ();

// objects
	BLEScan* pBLEScan;
  BLEUUID serviceUUID;
  BLEUUID charRxUUID;
  BLEUUID charTxUUID;


// primatives 

	size_t numAvailableLines;
	uint8_t transmitBuffer[BLE_BUFFER_SIZE];
	bool started = false;
  boolean doConnect = false;
  boolean doScan = false;
  uint16_t MTU;
	uint16_t maxTransferSize = BLE_BUFFER_SIZE;
  uint32_t flush_100ms = 0;
	int flush_time = FLUSH_TIME;

  /*
	Change UUID here if required
	

	const char *BLE_SERIAL_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
	const char *BLE_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
	const char *BLE_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
*/
	const char *BLE_SERIAL_SERVICE_UUID = "ff00";
	const char *BLE_RX_UUID = "ff01";
	const char *BLE_TX_UUID = "ff02";


};
