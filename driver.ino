/*
 * Arduino UNO - SCPI Digital I/O Controller
 *
 * Communication:
 *   USB Serial
 *   115200 baud
 *   8N1
 *
 * Commands:
 *
 *   *IDN?
 *   *RST
 *   *CLS
 *   *OPC?
 *
 *   SYST:ERR?
 *   SYST:VERS?
 *
 *   DIG:IN:CONF <pin>
 *   DIG:IN? <pin>
 *
 *   DIG:OUT:CONF <pin>
 *   DIG:OUT <pin>,<0|1>
 *   DIG:OUT? <pin>
 *
 */

#include <Arduino.h>

// ============================================================
// DEVICE INFORMATION
// ============================================================

#define DEVICE_MANUFACTURER "BOSON-ENGINEERING"
#define DEVICE_MODEL        "BOSON-DIO"
#define DEVICE_SERIAL       "001"
#define FIRMWARE_VERSION    "1.0.0"
#define SCPI_VERSION        "1999.0"


// ============================================================
// SCPI ERROR CODES
// ============================================================

#define SCPI_NO_ERROR           0
#define SCPI_INVALID_COMMAND   -100
#define SCPI_INVALID_PARAMETER -101
#define SCPI_INVALID_PIN       -102
#define SCPI_WRONG_MODE        -103


// ============================================================
// ERROR QUEUE
// ============================================================

#define ERROR_QUEUE_SIZE 10

struct ScpiError
{
    int code;
    const char* message;
};

ScpiError errorQueue[ERROR_QUEUE_SIZE];

uint8_t errorHead = 0;
uint8_t errorTail = 0;
uint8_t errorCount = 0;


// ============================================================
// COMMAND BUFFER
// ============================================================

#define COMMAND_BUFFER_SIZE 100

char commandBuffer[COMMAND_BUFFER_SIZE];
uint8_t commandIndex = 0;


// ============================================================
// ERROR MANAGER
// ============================================================

void pushError(int code, const char* message)
{
    if (errorCount >= ERROR_QUEUE_SIZE)
    {
        return;
    }

    errorQueue[errorTail].code = code;
    errorQueue[errorTail].message = message;

    errorTail++;

    if (errorTail >= ERROR_QUEUE_SIZE)
    {
        errorTail = 0;
    }

    errorCount++;
}


void clearErrors()
{
    errorHead = 0;
    errorTail = 0;
    errorCount = 0;
}


void queryError()
{
    if (errorCount == 0)
    {
        Serial.println("0,\"No error\"");
        return;
    }

    Serial.print(errorQueue[errorHead].code);
    Serial.print(",\"");
    Serial.print(errorQueue[errorHead].message);
    Serial.println("\"");

    errorHead++;

    if (errorHead >= ERROR_QUEUE_SIZE)
    {
        errorHead = 0;
    }

    errorCount--;
}


// ============================================================
// PIN VALIDATION
// ============================================================

bool isValidDigitalPin(int pin)
{
    /*
     * Arduino UNO:
     *
     * Digital pins:
     * 0 ... 13
     *
     * Pin 0/1 are normally used for Serial.
     */

    if (pin < 0 || pin > 13)
    {
        return false;
    }

    return true;
}


// ============================================================
// RESET
// ============================================================

void resetDevice()
{
    /*
     * Configure all digital pins as INPUT
     * except 0/1 which are used by Serial.
     */

    for (int pin = 2; pin <= 13; pin++)
    {
        pinMode(pin, INPUT);
    }

    clearErrors();
}


// ============================================================
// *IDN?
// ============================================================

void commandIDN()
{
    Serial.print(DEVICE_MANUFACTURER);
    Serial.print(",");
    Serial.print(DEVICE_MODEL);
    Serial.print(",");
    Serial.print(DEVICE_SERIAL);
    Serial.print(",");
    Serial.println(FIRMWARE_VERSION);
}


// ============================================================
// *RST
// ============================================================

void commandRST()
{
    resetDevice();
}


// ============================================================
// *OPC?
// ============================================================

void commandOPC()
{
    Serial.println("1");
}


// ============================================================
// SYST:VERS?
// ============================================================

void commandSystemVersion()
{
    Serial.println(SCPI_VERSION);
}


// ============================================================
// DIGITAL OUTPUT CONFIGURATION
// ============================================================

void configureDigitalOutput(int pin)
{
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }

    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }

    pinMode(pin, OUTPUT);

    digitalWrite(pin, LOW);
}


// ============================================================
// DIGITAL INPUT CONFIGURATION
// ============================================================

void configureDigitalInput(int pin)
{
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }

    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }

    pinMode(pin, INPUT);
}


// ============================================================
// DIGITAL OUTPUT
// ============================================================

void setDigitalOutput(int pin, int value)
{
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }

    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }

    if (value != 0 && value != 1)
    {
        pushError(
            SCPI_INVALID_PARAMETER,
            "Invalid output value"
        );

        return;
    }

    /*
     * Make sure the pin is configured as output.
     */

    pinMode(pin, OUTPUT);

    digitalWrite(
        pin,
        value == 1 ? HIGH : LOW
    );
}


// ============================================================
// DIGITAL OUTPUT QUERY
// ============================================================

void queryDigitalOutput(int pin)
{
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }

    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }

    int value = digitalRead(pin);

    Serial.println(value);
}


// ============================================================
// DIGITAL INPUT QUERY
// ============================================================

void queryDigitalInput(int pin)
{
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }

    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }

    int value = digitalRead(pin);

    Serial.println(value);
}


// ============================================================
// STRING TRIM
// ============================================================

void trimString(char* str)
{
    int length = strlen(str);

    while (length > 0 &&
           (str[length - 1] == ' ' ||
            str[length - 1] == '\r' ||
            str[length - 1] == '\n'))
    {
        str[length - 1] = '\0';
        length--;
    }

    int start = 0;

    while (str[start] == ' ')
    {
        start++;
    }

    if (start > 0)
    {
        memmove(
            str,
            str + start,
            strlen(str + start) + 1
        );
    }
}


// ============================================================
// COMMAND PARSER
// ============================================================

void processCommand(char* command)
{
    trimString(command);

    // --------------------------------------------------------
    // Empty command
    // --------------------------------------------------------

    if (strlen(command) == 0)
    {
        return;
    }


    // ========================================================
    // IEEE 488.2 COMMANDS
    // ========================================================

    if (strcmp(command, "*IDN?") == 0)
    {
        commandIDN();
        return;
    }


    if (strcmp(command, "*RST") == 0)
    {
        commandRST();
        return;
    }


    if (strcmp(command, "*CLS") == 0)
    {
        clearErrors();
        return;
    }


    if (strcmp(command, "*OPC?") == 0)
    {
        commandOPC();
        return;
    }


    // ========================================================
    // SYSTEM
    // ========================================================

    if (strcmp(command, "SYST:ERR?") == 0)
    {
        queryError();
        return;
    }


    if (strcmp(command, "SYST:VERS?") == 0)
    {
        commandSystemVersion();
        return;
    }


    // ========================================================
    // DIGITAL INPUT CONFIGURATION
    // ========================================================

    if (strncmp(command, "DIG:IN:CONF ", 12) == 0)
    {
        int pin;

        if (sscanf(command + 12, "%d", &pin) != 1)
        {
            pushError(
                SCPI_INVALID_PARAMETER,
                "Invalid parameter"
            );

            return;
        }

        configureDigitalInput(pin);

        return;
    }


    // ========================================================
    // DIGITAL INPUT QUERY
    // ========================================================

    if (strncmp(command, "DIG:IN? ", 8) == 0)
    {
        int pin;

        if (sscanf(command + 8, "%d", &pin) != 1)
        {
            pushError(
                SCPI_INVALID_PARAMETER,
                "Invalid parameter"
            );

            return;
        }

        queryDigitalInput(pin);

        return;
    }


    // ========================================================
    // DIGITAL OUTPUT CONFIGURATION
    // ========================================================

    if (strncmp(command, "DIG:OUT:CONF ", 13) == 0)
    {
        int pin;

        if (sscanf(command + 13, "%d", &pin) != 1)
        {
            pushError(
                SCPI_INVALID_PARAMETER,
                "Invalid parameter"
            );

            return;
        }

        configureDigitalOutput(pin);

        return;
    }


    // ========================================================
    // DIGITAL OUTPUT QUERY
    // ========================================================

    if (strncmp(command, "DIG:OUT? ", 9) == 0)
    {
        int pin;

        if (sscanf(command + 9, "%d", &pin) != 1)
        {
            pushError(
                SCPI_INVALID_PARAMETER,
                "Invalid parameter"
            );

            return;
        }

        queryDigitalOutput(pin);

        return;
    }


    // ========================================================
    // DIGITAL OUTPUT SET
    // ========================================================

    if (strncmp(command, "DIG:OUT ", 8) == 0)
    {
        int pin;
        int value;

        if (sscanf(
                command + 8,
                "%d,%d",
                &pin,
                &value
            ) != 2)
        {
            pushError(
                SCPI_INVALID_PARAMETER,
                "Invalid parameter"
            );

            return;
        }

        setDigitalOutput(pin, value);

        return;
    }


    // ========================================================
    // UNKNOWN COMMAND
    // ========================================================

    pushError(
        SCPI_INVALID_COMMAND,
        "Undefined command"
    );
}


// ============================================================
// SERIAL PROCESSING
// ============================================================

void processSerial()
{
    while (Serial.available() > 0)
    {
        char c = Serial.read();

        // ----------------------------------------------------
        // End of command
        // ----------------------------------------------------

        if (c == '\n' || c == '\r')
        {
            if (commandIndex > 0)
            {
                commandBuffer[commandIndex] = '\0';

                processCommand(commandBuffer);

                commandIndex = 0;
            }
        }

        // ----------------------------------------------------
        // Add character to buffer
        // ----------------------------------------------------

        else
        {
            if (commandIndex <
                COMMAND_BUFFER_SIZE - 1)
            {
                commandBuffer[commandIndex] = c;

                commandIndex++;
            }
            else
            {
                /*
                 * Buffer overflow
                 */

                commandIndex = 0;

                pushError(
                    SCPI_INVALID_PARAMETER,
                    "Command too long"
                );
            }
        }
    }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(9600);

    /*
     * Give Serial some time to initialize.
     */

    delay(100);

    resetDevice();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    processSerial();
}