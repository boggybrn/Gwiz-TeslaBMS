#ifndef _GWIZPACKINTERFACE_H_
#define _GWIZPACKINTERFACE_H_

#include <stdint.h>

class GwizPackInterface 
{

public:
    virtual float getGwizPackVoltage() = 0;
    virtual float getHighestCellVoltage() = 0;
    virtual float getLowestCellVoltage() = 0;
    virtual float getLowestTemperature() = 0;
    virtual float getHighestTemperature() = 0;
};

#endif