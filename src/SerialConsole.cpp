/*
 * SerialConsole.cpp
 *
 Copyright (c) 2017 EVTV / Collin Kidder

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 */

//#include <due_wire.h>
#include <EEPROM.h>
#include "SerialConsole.h"
#include "Logger.h"
#include "BMSModuleManager.h"
#include "GwizPack.h"

template <class T>
inline Print &operator<<(Print &obj, T arg)
{
    obj.print(arg);
    return obj;
} //Lets us stream SerialUSB

extern EEPROMSettings settings;
extern BMSModuleManager bms;
extern GwizPack gwiz;

bool printPrettyDisplay;
uint32_t prettyCounter;
int whichDisplay;

SerialConsole::SerialConsole()
{
    init();
}

void SerialConsole::init()
{
    //State variables for serial console
    ptrBuffer = 0;
    state = STATE_ROOT_MENU;
    loopcount = 0;
    cancel = false;
    printPrettyDisplay = false;
    prettyCounter = 0;
    whichDisplay = 0;
}

void SerialConsole::loop()
{
    if (SERIALCONSOLE.available())
    {
        serialEvent();
    }
    if (printPrettyDisplay && (millis() > (prettyCounter + 3000)))
    {
        prettyCounter = millis();
        if (whichDisplay == 0)
            bms.printPackSummary();
        if (whichDisplay == 1)
            bms.printPackDetails();
        if (whichDisplay == 2)
            gwiz.printPackDetails();
    }
}

void SerialConsole::printMenu()
{
    Logger::console("\n*************SYSTEM MENU *****************");
    Logger::console("Enable line endings of some sort (LF, CR, CRLF)");
    Logger::console("Most commands case sensitive\n");
    Logger::console("GENERAL SYSTEM CONFIGURATION\n");
    Logger::console("   E = dump system EEPROM values");
    Logger::console("   h = help (displays this message)");
    Logger::console("   S = Sleep all boards");
    Logger::console("   W = Wake up all boards");
    Logger::console("   C = Clear all board faults");
    Logger::console("   F = Find all connected boards");
    Logger::console("   R = Renumber connected boards in sequence");
    Logger::console("   B = Attempt balancing for 50 seconds");
    Logger::console("   b = stop balancing");
    Logger::console("   p = Toggle output of pack summary every 3 seconds");
    Logger::console("   d = Toggle output of pack details every 3 seconds");
    Logger::console("   g = Toggle output of G-Wiz details every 3 seconds");

    Logger::console("   LOGLEVEL=%i - set log level (0=debug, 1=info, 2=warn, 3=error, 4=off)", Logger::getLogLevel());

    Logger::console("\nBATTERY MANAGEMENT CONTROLS\n");
    Logger::console("   VOLTLIMHI=%f - High limit for cells in volts", settings.OverVSetpoint);
    Logger::console("   VOLTLIMLO=%f - Low limit for cells in volts", settings.UnderVSetpoint);
    Logger::console("   TEMPLIMHI=%f - High limit for cell temperature in degrees C", settings.OverTSetpoint);
    Logger::console("   TEMPLIMLO=%f - Low limit for cell temperature in degrees C", settings.UnderTSetpoint);
    Logger::console("   BALVOLT=%f - Voltage at which to begin cell balancing", settings.balanceVoltage);
    Logger::console("   BALHYST=%f - How far voltage must dip before balancing is turned off", settings.balanceHyst);
    Logger::console("   BALTOL=%f - Tollerance from lowest cell to trigger balancing in another cell", settings.balanceTollerance);
    Logger::console("   CHARGE=%l - Charge in Battery in mA Seconds", settings.chargeInmASeconds);

    float OverVSetpoint;
    float UnderVSetpoint;
    float OverTSetpoint;
    float UnderTSetpoint;
    float balanceVoltage;
    float balanceHyst;
}

/*	There is a help menu (press H or h or ?)

    Commands are submitted by sending line ending (LF, CR, or both)
 */
void SerialConsole::serialEvent()
{
    int incoming;
    incoming = SERIALCONSOLE.read();
    if (incoming == -1)
    { //false alarm....
        return;
    }

    if (incoming == 10 || incoming == 13)
    { //command done. Parse it.
        handleConsoleCmd();
        ptrBuffer = 0; //reset line counter once the line has been processed
    }
    else
    {
        cmdBuffer[ptrBuffer++] = (unsigned char)incoming;
        if (ptrBuffer > 79)
            ptrBuffer = 79;
    }
}

void SerialConsole::handleConsoleCmd()
{

    if (state == STATE_ROOT_MENU)
    {
        if (ptrBuffer == 1)
        { //command is a single ascii character
            handleShortCmd();
        }
        else
        { //if cmd over 1 char then assume (for now) that it is a config line
            handleConfigCmd();
        }
    }
}

/*For simplicity the configuration setting code uses four characters for each configuration choice. This makes things easier for
 comparison purposes.
 */
void SerialConsole::handleConfigCmd()
{
    int i;
    int newValue;
    float newFloat;
    bool needEEPROMWrite = false;

    //Logger::debug("Cmd size: %i", ptrBuffer);
    if (ptrBuffer < 6)
        return;               //4 digit command, =, value is at least 6 characters
    cmdBuffer[ptrBuffer] = 0; //make sure to null terminate
    String cmdString = String();
    unsigned char whichEntry = '0';
    i = 0;

    while (cmdBuffer[i] != '=' && i < ptrBuffer)
    {
        cmdString.concat(String(cmdBuffer[i++]));
    }
    i++; //skip the =
    if (i >= ptrBuffer)
    {
        Logger::console("Command needs a value..ie TORQ=3000");
        Logger::console("");
        return; //or, we could use this to display the parameter instead of setting
    }

    // strtol() is able to parse also hex values (e.g. a string "0xCAFE"), useful for enable/disable by device id
    newValue = strtol((char *)(cmdBuffer + i), NULL, 0);
    newFloat = strtof((char *)(cmdBuffer + i), NULL);
    cmdString.toUpperCase();

    if (cmdString == String("LOGLEVEL"))
    {
        switch (newValue)
        {
        case 0:
            Logger::setLoglevel(Logger::Debug);
            settings.logLevel = 0;
            Logger::console("setting loglevel to 'debug'");
            break;
        case 1:
            Logger::setLoglevel(Logger::Info);
            settings.logLevel = 1;
            Logger::console("setting loglevel to 'info'");
            break;
        case 2:
            Logger::console("setting loglevel to 'warning'");
            settings.logLevel = 2;
            Logger::setLoglevel(Logger::Warn);
            break;
        case 3:
            Logger::console("setting loglevel to 'error'");
            settings.logLevel = 3;
            Logger::setLoglevel(Logger::Error);
            break;
        case 4:
            Logger::console("setting loglevel to 'off'");
            settings.logLevel = 4;
            Logger::setLoglevel(Logger::Off);
            break;
        }
        needEEPROMWrite = true;
    }
    else if (cmdString == String("VOLTLIMHI"))
    {
        if (newFloat >= 0.0f && newFloat <= 6.00f)
        {
            settings.OverVSetpoint = newFloat;
            needEEPROMWrite = true;
            Logger::console("Cell Voltage Upper Limit set to: %f", settings.OverVSetpoint);
        }
        else
            Logger::console("Invalid upper cell voltage limit. Please enter a value 0.0 to 6.0");
    }
    else if (cmdString == String("VOLTLIMLO"))
    {
        if (newFloat >= 0.0f && newFloat <= 6.0f)
        {
            settings.UnderVSetpoint = newFloat;
            needEEPROMWrite = true;
            Logger::console("Cell Voltage Lower Limit set to %f", settings.UnderVSetpoint);
        }
        else
            Logger::console("Invalid lower cell voltage limit. Please enter a value 0.0 to 6.0");
    }
    else if (cmdString == String("BALVOLT"))
    {
        if (newFloat >= 0.0f && newFloat <= 6.0f)
        {
            settings.balanceVoltage = newFloat;
            needEEPROMWrite = true;
            Logger::console("Balance voltage set to %f", settings.balanceVoltage);
        }
        else
            Logger::console("Invalid balancing voltage. Please enter a value 0.0 to 6.0");
    }
    else if (cmdString == String("BALHYST"))
    {
        if (newFloat >= 0.0f && newFloat <= 1.0f)
        {
            settings.balanceHyst = newFloat;
            needEEPROMWrite = true;
            Logger::console("Balance hysteresis set to %f", settings.balanceHyst);
        }
        else
            Logger::console("Invalid balance hysteresis. Please enter a value 0.0 to 1.0");
    }
    else if (cmdString == String("BALTOL"))
    {
        if (newFloat >= 0.0f && newFloat <= 1.0f)
        {
            settings.balanceTollerance = newFloat;
            needEEPROMWrite = true;
            Logger::console("Balance tollerance set to %f", settings.balanceTollerance);
        }
        else
            Logger::console("Invalid balance tollerance. Please enter a value 0.0 to 1.0");
    }
    
    else if (cmdString == String("TEMPLIMHI"))
    {
        if (newFloat >= 0.0f && newFloat <= 100.0f)
        {
            settings.OverTSetpoint = newFloat;
            needEEPROMWrite = true;
            Logger::console("Module Temperature Upper Limit set to: %f", settings.OverTSetpoint);
        }
        else
            Logger::console("Invalid temperature upper limit please enter a value 0.0 to 100.0");
    }
    else if (cmdString == String("TEMPLIMLO"))
    {
        if (newFloat >= -20.00f && newFloat <= 120.0f)
        {
            settings.UnderTSetpoint = newFloat;
            needEEPROMWrite = true;
            Logger::console("Module Temperature Lower Limit set to: %f", settings.UnderTSetpoint);
        }
        else
            Logger::console("Invalid temperature lower limit please enter a value between -20.0 and 120.0");
    }
    else if (cmdString == String("PWMC"))
    {
        if (newValue <= 0xFF)
        {
            analogWrite(CHG_CURRENT_PORT, 0xFF - newValue); //note that these signals are inverted, so flip in s/w
            Logger::console("PWMC value set to  %d", newValue);
        }
        else
            Logger::console("Invalid pwm setting - it should be < 255");
    }
    else if (cmdString == String("PWMV"))
    {
        if (newValue <= 0xFF)
        {
            analogWrite(CHG_VOLTAGE_PORT, 0xFF - newValue);
            Logger::console("PWMV value set to  %d", newValue);
        }
        else
            Logger::console("Invalid pwm setting - it should be < 255");
    }
    else if (cmdString == String("CHARGE"))
    {
        settings.chargeInmASeconds = newValue;
        needEEPROMWrite = true;
        Logger::console("Charge in battery set to %l", settings.chargeInmASeconds);
    }
    else
    {
        Logger::console("Unknown command");
    }
    if (needEEPROMWrite)
    {
        EEPROM.put(EEPROM_PAGE, settings);
    }
}

void SerialConsole::handleShortCmd()
{
    uint8_t val;

    switch (cmdBuffer[0])
    {
    case 'h':
    case '?':
    case 'H':
        printMenu();
        break;
    case 'S':
        Logger::console("Sleeping all connected boards");
        bms.sleepBoards();
        break;
    case 'W':
        Logger::console("Waking up all connected boards");
        bms.wakeBoards();
        break;
    case 'C':
        Logger::console("Clearing all faults");
        bms.clearFaults();
        break;
    case 'E':
        Logger::console("EEPROM version %d ", settings.version);
        Logger::console("OverVSetpoint %f", settings.OverVSetpoint);
        Logger::console("UnderVsetpoint %f ", settings.UnderVSetpoint);
        Logger::console("OverTSetpoint %f ", settings.OverTSetpoint);
        Logger::console("UnderTSetpoint %f ", settings.UnderTSetpoint);
        Logger::console("balanceVoltage %f ", settings.balanceVoltage);
        Logger::console("balanceHyst %f ", settings.balanceHyst);
        Logger::console("logLevel %d ", settings.logLevel);
        break;
    case 'F':
        bms.findBoards();
        break;
    case 'R':
        Logger::console("Renumbering all boards.");
        bms.renumberBoardIDs();
        break;
    case 'B':
        bms.balanceCells(gwiz.getLowestCellVoltage(), 50, 1);
        break;
    case 'b':
        bms.stopBalancing();
        break;    
    case 'p':
        if (whichDisplay == 1 && printPrettyDisplay)
            whichDisplay = 0;
        else
        {
            printPrettyDisplay = !printPrettyDisplay;
            if (printPrettyDisplay)
            {
                Logger::console("Enabling pack summary display, 3 second interval");
            }
            else
            {
                Logger::console("No longer displaying pack summary.");
            }
        }
        break;
    case 'd':
        if (whichDisplay == 0 && printPrettyDisplay)
            whichDisplay = 1;
        else
        {
            printPrettyDisplay = !printPrettyDisplay;
            whichDisplay = 1;
            if (printPrettyDisplay)
            {
                Logger::console("Enabling pack details display, 3 second interval");
            }
            else
            {
                Logger::console("No longer displaying pack details.");
            }
        }
        break;
    case 'g':
        if (whichDisplay != 2 && printPrettyDisplay)
        {
            whichDisplay = 2;
        }
        else
        {
            printPrettyDisplay = !printPrettyDisplay;
            whichDisplay = 2;
            if (printPrettyDisplay)
            {
                Logger::console("Enabling G-Wiz detail display, 3 second interval");
            }
            else
            {
                Logger::console("No longer displaying G-Wiz details.");
            }
        }
        break;
    }
}
