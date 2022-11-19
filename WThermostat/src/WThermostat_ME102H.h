#ifndef THERMOSTAT_ME102H_H
#define	THERMOSTAT_ME102H_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "WThermostat.h"

// Celcius/Farenheit            = Tuya DPID DEC  23 HEX 17 55AA0307000517040001002A
// Device on/off                = Tuya DPID DEC   1 HEX 01 55AA03070005010100010112
// Actual Temperature           = Tuya DPID DEC  24 HEX 18 55AA03070008180200040000001645
// Temperature Floor sensor     = Tuya DPID DEC 101 HEX 65 55AA0307000865020004000000007C
// Target Temperature           = Tuya DPID DEC  16 HEX 10 55AA03070008100200040000001940
// Shedual Mode Auto/Off        = Tuya DPID DEC   2 HEX 02 55AA03070005020400010116
// Max Heater Temperature       = Tuya DPID DEC  19 HEX 13 55AA03070008130200040000005A84
// Min Heater Temperature       = Tuya DPID DEC  26 HEX 1A 55AA030700081A0200040000001445
// Switch Difference in degrees = Tuya DPID DEC 106 HEX 6A 55AA030700086A0200040000000384
// Temperature correction       = Tuya DPID DEC  27 HEX 1B 55AA030700081B0200040000000032
// Sensor Selection             = Tuya DPID DEC  43 HEX 2B 55AA030700052B040001003E
// ?                            = Tuya DPID DEC 104 HEX 68 55AA0307000568040001017C
// ?                            = Tuya DPID DEC  45 HEX 2D 55AA030700052D0500010041
// Freeze Mode on/off           = Tuya DPID DEC 103 HEX 67 55AA03070005670100010077
// Schedules                    = Tuya DPID DEC 108 HEX 6C 55AA0307001C6C00001806001408000F0B1E0F0C1E0F11001616000F08001617000FDB
// Locked on/off                = Tuya DPID DEC  40 HEX 28 55AA03070005280100010038
// Heater on/off                = Tuya DPID DEC  36 HEX 24 55AA03070005240400010037

class WThermostat_ME102H : public WThermostat {
public :
  const char* SENSOR_SELECTION_INTERNAL = "internal";
  const char* SENSOR_SELECTION_FLOOR = "floor";
  const char* SENSOR_SELECTION_BOTH = "both";
  WThermostat_ME102H(WNetwork* network, WProperty* thermostatModel, WClock* wClock)
    : WThermostat(network, thermostatModel, wClock) {
    network->debug(F("WThermostat_ME102H created"));
  }

  virtual void configureCommandBytes() {
    this->byteDeviceOn = 0x01;
    this->byteSchedulesMode = 0x02;
    this->byteTemperatureTarget = 0x10;
    this->byteMaxHeaterTemperature = 0x13;
    this->byteTemperatureActual = 0x18;
    this->byteHeater = 0x24;
    this->byteLocked = 0x28;
    this->byteMinHeaterTemperature = 0x1A;
    this->byteTemperatureCorrection = 0x1B;
    this->byteTemperatureFloor = 0x65;
    this->byteFreezeMode = 0x67;
    this->byteSwitchDiff = 0x6A;
    this->byteSchedules = 0x6c;
    this->temperatureFactor = 1.0f;
    this->byteSchedulingPosHour = 0;
    this->byteSchedulingPosMinute = 1;
    this->byteSchedulingDays = 8;
    //custom
    this->byteSensorSelection = 0x2b;
  }

  virtual void initializeProperties() {
    WThermostat::initializeProperties();
    //schedules mode
    this->schedulesMode->clearEnums();
    this->schedulesMode->addEnumString(SCHEDULES_MODE_AUTO);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_OFF);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLIDAY);
    this->schedulesMode->addEnumString(SCHEDULES_MODE_HOLD);
    //sensorSelection
    this->sensorSelection = new WProperty("sensorSelection", "Sensor Selection", STRING, TYPE_THERMOSTAT_MODE_PROPERTY);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_INTERNAL);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_FLOOR);
    this->sensorSelection->addEnumString(SENSOR_SELECTION_BOTH);
    this->sensorSelection->setVisibility(MQTT);
    this->sensorSelection->setOnChange(std::bind(&WThermostat_ME102H::sensorSelectionToMcu, this, std::placeholders::_1));
    this->addProperty(this->sensorSelection);
  }

protected :

  virtual bool processStatusCommand(byte cByte, byte commandLength) {
		//Status report from MCU
		bool changed = false;
		bool knownCommand = WThermostat::processStatusCommand(cByte, commandLength);

		if (!knownCommand) {
      const char* newS;
      if (cByte == this->byteSensorSelection) {
        if (commandLength == 0x05) {
          //sensor selection -
          //internal: 55 aa 03 07 00 05 2b 04 00 01 00
          //floor:    55 aa 03 07 00 05 2b 04 00 01 01
          //both:     55 aa 03 07 00 05 2b 04 00 01 02
          newS = this->sensorSelection->getEnumString(receivedCommand[10]);
          if (newS != nullptr) {
            changed = ((changed) || (this->sensorSelection->setString(newS)));
            knownCommand = true;
          }
        }
      } else {
      //consume some unsupported commands
        switch (cByte) {
          case 0x17 :
            //Temperature Scale C /
            //MCU: 55 aa 03 07 00 05 17 04 00 01 00
            knownCommand = true;
            break;
          case 0x13 :
            //Temperature ceiling - 35C / MCU: 55 aa 03 07 00 08 13 02 00 04 00 00 00 23
            knownCommand = true;
            break;
          case 0x1a :
            //Lower limit of temperature - 5C / MCU: 55 aa 03 07 00 08 1a 02 00 04 00 00 00 05
            knownCommand = true;
            break;
          case 0x6a :
            //temp_differ_on - 1C / MCU: 55 aa 03 07 00 08 6a 02 00 04 00 00 00 01
            knownCommand = true;
            break;
          case 0x1b :
            //Temperature correction - 0C / MCU: 55 aa 03 07 00 08 1b 02 00 04 00 00 00 00
            knownCommand = true;
            break;
          case 0x67 :
            //freeze / MCU: 55 aa 03 07 00 05 67 01 00 01 00
            knownCommand = true;
            break;
          case 0x68 :
            //  programming_mode - weekend (2 days off) / MCU: 55 aa 03 07 00 05 68 04 00 01 01
            knownCommand = true;
            break;
          case 0x2d :
            //unknown Wifi state? /
            //MCU: 55 aa 03 07 00 05 2d 05 00 01 00
            knownCommand = true;
            break;
          case 0x24 :
            //unknown Wifi state? / MCU: 55 aa 03 07 00 05 24 04 00 01 00
            knownCommand = true;
            break;
        }
      }
    }
		if (changed) {
			notifyState();
		}
	  return knownCommand;
  }

  void sensorSelectionToMcu(WProperty* property) {
    if (!isReceivingDataFromMcu()) {
      byte sm = property->getEnumIndex();
      if (sm != 0xFF) {
        //send to device
        //internal:    55 aa 03 07 00 05 2b 04 00 01 00
        //floor:       55 aa 03 07 00 05 2b 04 00 01 01
        //both:        55 aa 03 07 00 05 2b 04 00 01 02
        unsigned char cm[] = { 0x55, 0xAA, 0x00, 0x06, 0x00, 0x05,
                               this->byteSensorSelection, 0x04, 0x00, 0x01, sm};
        commandCharsToSerial(11, cm);
      }
    }
  }

private :
  WProperty* sensorSelection;
  byte byteSensorSelection;

};

#endif
