#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>
#include "sdkconfig.h"

#include "BleConnectionStatus.h"
#include "BleGamepad.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
  #include "esp32-hal-log.h"
  #define LOG_TAG ""
#else
  #include "esp_log.h"
  static const char* LOG_TAG = "BLEDevice";
#endif

static const uint8_t _hidReportDescriptor[] = {

  USAGE_PAGE(1),       0x01, // USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x04, // USAGE (Joystick - 0x04; Gamepad - 0x05; Multi-axis Controller - 0x08)
  COLLECTION(1),       0x01, // COLLECTION (Application)
  USAGE(1),            0x01, //   USAGE (Pointer)
  COLLECTION(1),       0x00, //   COLLECTION (Physical)
  REPORT_ID(1),        0x01, //     REPORT_ID (1)
  // ------------------------------------------------- Buttons (1 to 128)
  USAGE_PAGE(1),       0x09, //     USAGE_PAGE (Button)
  USAGE_MINIMUM(1),    0x01, //     USAGE_MINIMUM (Button 1)
  USAGE_MAXIMUM(1),    0x80, //     USAGE_MAXIMUM (Button 128)
  LOGICAL_MINIMUM(1),  0x00, //     LOGICAL_MINIMUM (0)
  LOGICAL_MAXIMUM(1),  0x01, //     LOGICAL_MAXIMUM (1)
  REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
  REPORT_COUNT(1),     0x80, //     REPORT_COUNT (128)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ; 16 bytes

  // ------------------------------------------------- Padding
//  REPORT_SIZE(1),      0x01, //     REPORT_SIZE (1)
//  REPORT_COUNT(1),     0x02, //     REPORT_COUNT (2)
//  HIDINPUT(1),         0x03, //     INPUT (Constant, Variable, Absolute) ;2 bit padding
  // ------------------------------------------------- X/Y position, Z/rZ position

  USAGE_PAGE(1),       0x01, //     USAGE_PAGE (Generic Desktop)
  USAGE(1),            0x30, //     USAGE (X)
  USAGE(1),            0x31, //     USAGE (Y)
  USAGE(1),            0x32, //     USAGE (analog1)
  USAGE(1),            0x33, //     USAGE (analog2)
  USAGE(1),            0x34, //     USAGE (analog3)
  USAGE(1),            0x35, //     USAGE (analog4)
  USAGE(1),            0x36, //     USAGE (analog5)
  USAGE(1),            0x37, //     USAGE (analog6)
  USAGE(1),            0x38, //     USAGE (analog7)
  USAGE(1),            0x39, //     USAGE (analog8)
  LOGICAL_MINIMUM(1),  0x81, //     LOGICAL_MINIMUM (-127)
  LOGICAL_MAXIMUM(1),  0x7f, //     LOGICAL_MAXIMUM (127)
  REPORT_SIZE(1),      0x08, //     REPORT_SIZE (8)
  REPORT_COUNT(1),     0x08, //     REPORT_COUNT (10)
  HIDINPUT(1),         0x02, //     INPUT (Data, Variable, Absolute) ;10 bytes (X,Y,a1,a2,a3,a4,a5,a6,a7,a8)

  USAGE_PAGE(1), 	     0x01, //     USAGE_PAGE (Generic Desktop)
  USAGE(1), 		       0x39, //     USAGE (Hat switch)
  USAGE(1), 		       0x39, //     USAGE (Hat switch)
  LOGICAL_MINIMUM(1),  0x01, //		  LOGICAL_MINIMUM (1)
  LOGICAL_MAXIMUM(1),  0x08, //     LOGICAL_MAXIMUM (8)
  REPORT_SIZE(1), 	   0x04, //		  REPORT_SIZE (4)
  REPORT_COUNT(1), 	   0x02, //	   	REPORT_COUNT (2)
  HIDINPUT(1), 		   0x02, //		INPUT (Data, Variable, Absolute) ;1 byte Hat1, Hat2
 
  END_COLLECTION(0),       // END_COLLECTION
  END_COLLECTION(0)        // END_COLLECTION
};

BleGamepad::BleGamepad(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel)
{
  this->_buttons[0] = 0;
  this->_buttons[1] = 0;
  this->deviceName = deviceName;
  this->deviceManufacturer = deviceManufacturer;
  this->batteryLevel = batteryLevel;
  this->connectionStatus = new BleConnectionStatus();
}

void BleGamepad::begin(void)
{
  xTaskCreate(this->taskServer, "server", 20000, (void *)this, 5, NULL);
}

void BleGamepad::end(void)
{
}

void BleGamepad::setAxes(signed char x, signed char y, signed char a1, signed char a2, signed char a3, signed char a4, signed char a5, signed char a6, signed char a7, signed char a8, signed char hat)
{
  if (this->isConnected())
  {
    uint8_t m[27];
    
    m[0] = _buttons[0];
	  m[1] = (_buttons[0] >> 8);
	  m[2] = (_buttons[0] >> 16);
	  m[3] = (_buttons[0] >> 24);
    m[4] = (_buttons[0] >> 32); 
    m[5] = (_buttons[0] >> 40);
    m[6] = (_buttons[0] >> 48);
    m[7] = (_buttons[0] >> 56);

    m[8] = _buttons[1];
	  m[9] = (_buttons[1] >> 8);
	  m[10] = (_buttons[1] >> 16);
	  m[11] = (_buttons[1] >> 24);
    m[12] = (_buttons[1] >> 32); 
    m[13] = (_buttons[1] >> 40);
    m[14] = (_buttons[1] >> 48);
    m[15] = (_buttons[1] >> 56);
    
    m[16] = x;
    m[17] = y;
    m[18] = a1;
    m[19] = a2;
    m[20] = a3;
    m[21] = a4;
    m[22] = a5;
    m[23] = a6;
    m[24] = a6;
    m[25] = a6;
	  m[26] = hat;
    this->inputGamepad->setValue(m, sizeof(m));
    this->inputGamepad->notify();
  }
}

// indexed button (1..128)
void BleGamepad::press(uint8_t b)
{
  uint64_t bitmask = (1 << b);
  char index = ( b > 64 ? 1 : 0);
  uint64_t result = _buttons[index] | bitmask;
  if (result != _buttons[index]) {
    _buttons[index] = result;
    this->setAxes(0,0,0,0,0,0,0,0,0,0,0);
  }
}

// indexed button (1..128)
void BleGamepad::release(uint8_t b)
{
  uint64_t bitmask = (1 << b);
  char index = ( b > 64 ? 1 : 0);
  uint64_t result = _buttons[index] & ~bitmask;
  if (result != _buttons[index]) {
    _buttons[index] = result;
    this->setAxes(0,0,0,0,0,0,0,0,0,0,0);
  }
}

// indexed button (1..128)
bool BleGamepad::isPressed(uint8_t b)
{
  uint64_t bitmask = (1 << b);
  char index = ( b > 64 ? 1 : 0);
  if ((bitmask & _buttons[index]) > 0)
    return true;
  return false;
}

bool BleGamepad::isConnected(void) {
  return this->connectionStatus->connected;
}

void BleGamepad::setBatteryLevel(uint8_t level) {
  this->batteryLevel = level;
}

void BleGamepad::taskServer(void* pvParameter) {
  BleGamepad* BleGamepadInstance = (BleGamepad *) pvParameter; //static_cast<BleGamepad *>(pvParameter);
  BLEDevice::init(BleGamepadInstance->deviceName);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(BleGamepadInstance->connectionStatus);

  BleGamepadInstance->hid = new BLEHIDDevice(pServer);
  BleGamepadInstance->inputGamepad = BleGamepadInstance->hid->inputReport(1); // <-- input REPORTID from report map
  BleGamepadInstance->connectionStatus->inputGamepad = BleGamepadInstance->inputGamepad;

  BleGamepadInstance->hid->manufacturer()->setValue(BleGamepadInstance->deviceManufacturer);

  BleGamepadInstance->hid->pnp(0x01,0x02e5,0xabcd,0x0110);
  BleGamepadInstance->hid->hidInfo(0x00,0x01);

  BLESecurity *pSecurity = new BLESecurity();

  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  BleGamepadInstance->hid->reportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
  BleGamepadInstance->hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_GAMEPAD);
  pAdvertising->addServiceUUID(BleGamepadInstance->hid->hidService()->getUUID());
  pAdvertising->start();
  BleGamepadInstance->hid->setBatteryLevel(BleGamepadInstance->batteryLevel);

  ESP_LOGD(LOG_TAG, "Advertising started!");
  vTaskDelay(portMAX_DELAY); //delay(portMAX_DELAY);
}