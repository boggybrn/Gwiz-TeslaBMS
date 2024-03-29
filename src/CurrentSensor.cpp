#include <Arduino.h>
#include "CurrentSensor.h"
#include "config.h"
#include "Logger.h"

extern EEPROMSettings settings;

float CurrentSensor::getCurrentInAmps(void)
{
    return -(float)((float)CANmilliamps / 1000);        // sign reversed for display ie -ve = charging, +ve = driving
}

signed long CurrentSensor::getChargeInmASeconds(void)
{
    return chargeInmASeconds;
}
void CurrentSensor::setChargeInmASeconds(signed long charge)
{
    chargeInmASeconds = charge;
}

void CurrentSensor::init(void)
{
    can.begin(STD_ID_LEN, BR500K, PORTA_11_12_XCVR); // 11b IDs, 500k bit rate, transceiver chip, portA pins 11,12
    can.filterMask16Init(0, 0, 0x7ff, 0, 0);         // filter bank 0, filter 0: don't pass any, flt 1: pass all msgs
}

void CurrentSensor::service(void)
{
    if (can.receive(canId, canFltIdx, canRxbytes) > -1) // poll for rx from CAN-Bus current sensor
    {
        //SERIALCONSOLE.printf("Can Rx %x ", canId);
        //for (int i = 0; i < 8; i++)
        //{
        //    SERIALCONSOLE.printf("%x", canRxbytes[i]);
        //}

        uint32_t inbox = 0;
        for (int i = 0; i < 4; i++)
        {
            inbox = (inbox << 8) | canRxbytes[i];
        }
        CANmilliamps = inbox;
        if (CANmilliamps > 0x80000000)
        {
            CANmilliamps -= 0x80000000;
        }
        else
        {
            CANmilliamps = (0x80000000 - CANmilliamps) * -1;
        }

        Logger::info("Current = %dmA", CANmilliamps);

        chargeInmASeconds += CANmilliamps;              //note that this assumes service() is called once a second!
        if(chargeInmASeconds > settings.chargeInmASeconds)
        {
            chargeInmASeconds = settings.chargeInmASeconds;     //limit to the maximum configured for the pack
        }
        Logger::info("Charge = %lmAS", chargeInmASeconds);
    }
}