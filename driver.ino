
/*
 * ================================================================
 *  ARDUINO NANO - SCPI UNIVERSAL I/O CONTROLLER
 * ================================================================
 *
 *  Version : 2.0.0
 *
 *  Matériel :
 *    Arduino Nano classique basé sur ATmega328P
 *
 *  Communication :
 *    USB / Serial
 *    115200 bauds
 *    8N1
 *
 * ================================================================
 *  RESSOURCES SUPPORTÉES
 * ================================================================
 *
 *  DIGITAL INPUT  (DI)
 *    D2 ... D13
 *
 *  DIGITAL OUTPUT (DO)
 *    D2 ... D13
 *
 *  ANALOG INPUT   (AI)
 *    A0 ... A7
 *    ADC 10 bits
 *    Valeur retournée : 0 ... 1023
 *
 *  ANALOG OUTPUT  (AO)
 *    D3, D5, D6, D9, D10, D11
 *
 *    ATTENTION :
 *    Le Nano ATmega328P ne possède PAS de DAC.
 *    Les AO sont donc des sorties PWM.
 *
 *    Valeur :
 *      0   = 0 %
 *      255 = 100 %
 *
 * ================================================================
 *  COMMANDES SCPI
 * ================================================================
 *
 *  IEEE 488.2
 *
 *    *IDN?
 *    *RST
 *    *CLS
 *    *OPC?
 *
 *  SYSTEM
 *
 *    SYST:ERR?
 *    SYST:VERS?
 *
 *  DIGITAL INPUT
 *
 *    DIG:IN:CONF <pin>
 *    DIG:IN? <pin>
 *
 *  DIGITAL OUTPUT
 *
 *    DIG:OUT:CONF <pin>
 *    DIG:OUT <pin>,<0|1>
 *    DIG:OUT? <pin>
 *
 *  ANALOG INPUT
 *
 *    ANA:IN:CONF <channel>
 *    ANA:IN? <channel>
 *
 *  ANALOG OUTPUT / PWM
 *
 *    ANA:OUT:CONF <pin>
 *    ANA:OUT <pin>,<0..255>
 *    ANA:OUT? <pin>
 *
 * ================================================================
 *  EXEMPLES
 * ================================================================
 *
 *    *IDN?
 *
 *    DIG:IN:CONF 2
 *    DIG:IN? 2
 *
 *    DIG:OUT:CONF 4
 *    DIG:OUT 4,1
 *    DIG:OUT? 4
 *
 *    ANA:IN:CONF 0
 *    ANA:IN? 0
 *
 *    ANA:OUT:CONF 3
 *    ANA:OUT 3,128
 *    ANA:OUT? 3
 *
 * ================================================================
 */


#include <Arduino.h>


// =================================================================
// DEVICE INFORMATION
// =================================================================

#define DEVICE_MANUFACTURER "BOSON-ENGINEERING"
#define DEVICE_MODEL        "BOSON-NANO-IO"
#define DEVICE_SERIAL       "001"

#define FIRMWARE_VERSION    "2.0.0"
#define SCPI_VERSION        "1999.0"


// =================================================================
// SERIAL CONFIGURATION
// =================================================================

#define SERIAL_BAUDRATE 9600


// =================================================================
// SCPI ERROR CODES
// =================================================================

#define SCPI_NO_ERROR           0
#define SCPI_INVALID_COMMAND   -100
#define SCPI_INVALID_PARAMETER -101
#define SCPI_INVALID_PIN       -102
#define SCPI_WRONG_MODE        -103


// =================================================================
// ERROR QUEUE
// =================================================================

#define ERROR_QUEUE_SIZE 10


struct ScpiError
{
    int code;
    const char* message;
};


ScpiError errorQueue[ERROR_QUEUE_SIZE];

uint8_t errorHead  = 0;
uint8_t errorTail  = 0;
uint8_t errorCount = 0;


// =================================================================
// COMMAND BUFFER
// =================================================================

#define COMMAND_BUFFER_SIZE 100

char commandBuffer[COMMAND_BUFFER_SIZE];

uint8_t commandIndex = 0;


// =================================================================
// ERROR MANAGER
// =================================================================

/*
 * Ajoute une erreur dans la file d'erreurs SCPI.
 *
 * Exemple :
 *
 *    pushError(
 *        SCPI_INVALID_PIN,
 *        "Invalid pin"
 *    );
 */
void pushError(int code, const char* message)
{
    // Si la file est pleine, on ignore la nouvelle erreur.
    if (errorCount >= ERROR_QUEUE_SIZE)
    {
        return;
    }

    errorQueue[errorTail].code = code;
    errorQueue[errorTail].message = message;

    errorTail++;

    // Retour au début de la file circulaire.
    if (errorTail >= ERROR_QUEUE_SIZE)
    {
        errorTail = 0;
    }

    errorCount++;
}


/*
 * Efface toutes les erreurs.
 */
void clearErrors()
{
    errorHead  = 0;
    errorTail  = 0;
    errorCount = 0;
}


/*
 * Retourne la première erreur disponible.
 *
 * Exemple :
 *
 *    SYST:ERR?
 *
 * Réponse :
 *
 *    0,"No error"
 *
 * ou :
 *
 *    -102,"Invalid pin"
 */
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


// =================================================================
// DIGITAL PIN VALIDATION
// =================================================================

/*
 * Vérifie si une broche numérique existe sur le Nano.
 *
 * Nano ATmega328P :
 *
 *    D0 ... D13
 *
 * D0 et D1 sont réservées à la communication série.
 *
 * Le driver autorise donc :
 *
 *    D2 ... D13
 */
bool isValidDigitalPin(int pin)
{
    if (pin < 0 || pin > 13)
    {
        return false;
    }

    return true;
}


/*
 * Vérifie si une broche peut être utilisée par le système
 * comme entrée/sortie utilisateur.
 *
 * D0 et D1 sont réservées au Serial.
 */
bool isUserDigitalPin(int pin)
{
    if (!isValidDigitalPin(pin))
    {
        return false;
    }

    if (pin == 0 || pin == 1)
    {
        return false;
    }

    return true;
}


// =================================================================
// ANALOG INPUT VALIDATION
// =================================================================

/*
 * Vérifie si un canal analogique existe.
 *
 * Nano ATmega328P :
 *
 *    A0 ... A7
 *
 * Les canaux A6 et A7 sont uniquement analogiques.
 */
bool isValidAnalogInput(int channel)
{
    if (channel < 0 || channel > 7)
    {
        return false;
    }

    return true;
}


// =================================================================
// PWM / ANALOG OUTPUT VALIDATION
// =================================================================

/*
 * Liste des broches PWM du Nano ATmega328P.
 *
 * PWM :
 *
 *    D3
 *    D5
 *    D6
 *    D9
 *    D10
 *    D11
 *
 * Ces sorties sont utilisées comme "AO".
 */
bool isValidAnalogOutputPin(int pin)
{
    switch (pin)
    {
        case 3:
        case 5:
        case 6:
        case 9:
        case 10:
        case 11:
            return true;

        default:
            return false;
    }
}


// =================================================================
// RESET
// =================================================================

/*
 * Réinitialise toutes les ressources.
 *
 * État après reset :
 *
 *    D2-D13 = INPUT
 *
 *    AO PWM = 0
 *
 *    erreurs effacées
 */
void resetDevice()
{
    /*
     * Configure toutes les broches numériques utilisateur
     * comme entrées.
     */
    for (int pin = 2; pin <= 13; pin++)
    {
        pinMode(pin, INPUT);
    }


    /*
     * Met toutes les sorties PWM à 0.
     *
     * Cela évite qu'une ancienne valeur PWM reste active.
     */
    analogWrite(3,  0);
    analogWrite(5,  0);
    analogWrite(6,  0);
    analogWrite(9,  0);
    analogWrite(10, 0);
    analogWrite(11, 0);


    /*
     * Effacement de la file d'erreurs.
     */
    clearErrors();
}


// =================================================================
// *IDN?
// =================================================================

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


// =================================================================
// *RST
// =================================================================

void commandRST()
{
    resetDevice();
}


// =================================================================
// *OPC?
// =================================================================

void commandOPC()
{
    /*
     * L'opération est toujours terminée immédiatement.
     */
    Serial.println("1");
}


// =================================================================
// SYST:VERS?
// =================================================================

void commandSystemVersion()
{
    Serial.println(SCPI_VERSION);
}


// =================================================================
// DIGITAL INPUT CONFIGURATION
// =================================================================

void configureDigitalInput(int pin)
{
    /*
     * Vérification de la broche.
     */
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }


    /*
     * D0 et D1 sont utilisés par Serial.
     */
    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }


    /*
     * Configuration en entrée.
     */
    pinMode(pin, INPUT);
}


// =================================================================
// DIGITAL INPUT QUERY
// =================================================================

void queryDigitalInput(int pin)
{
    /*
     * Vérification de la broche.
     */
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }


    /*
     * D0 et D1 sont réservées au Serial.
     */
    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }


    /*
     * Lecture numérique.
     *
     * Résultat :
     *
     *    0 = LOW
     *    1 = HIGH
     */
    int value = digitalRead(pin);

    Serial.println(value);
}


// =================================================================
// DIGITAL OUTPUT CONFIGURATION
// =================================================================

void configureDigitalOutput(int pin)
{
    /*
     * Vérification de la broche.
     */
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }


    /*
     * D0 et D1 réservées au Serial.
     */
    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }


    /*
     * Configure la broche comme sortie.
     */
    pinMode(pin, OUTPUT);


    /*
     * Valeur initiale :
     *
     * LOW
     */
    digitalWrite(pin, LOW);
}


// =================================================================
// DIGITAL OUTPUT SET
// =================================================================

void setDigitalOutput(int pin, int value)
{
    /*
     * Vérification de la broche.
     */
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }


    /*
     * D0 et D1 réservées au Serial.
     */
    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }


    /*
     * Une sortie numérique accepte uniquement :
     *
     *    0
     *    1
     */
    if (value != 0 && value != 1)
    {
        pushError(
            SCPI_INVALID_PARAMETER,
            "Invalid output value"
        );

        return;
    }


    /*
     * Configure automatiquement la broche en OUTPUT.
     */
    pinMode(pin, OUTPUT);


    /*
     * Écriture de la valeur.
     */
    if (value == 1)
    {
        digitalWrite(pin, HIGH);
    }
    else
    {
        digitalWrite(pin, LOW);
    }
}


// =================================================================
// DIGITAL OUTPUT QUERY
// =================================================================

void queryDigitalOutput(int pin)
{
    /*
     * Vérification de la broche.
     */
    if (!isValidDigitalPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid pin"
        );

        return;
    }


    /*
     * D0 et D1 réservées au Serial.
     */
    if (pin == 0 || pin == 1)
    {
        pushError(
            SCPI_WRONG_MODE,
            "Pin reserved for Serial"
        );

        return;
    }


    /*
     * Lecture de l'état actuel de la broche.
     */
    int value = digitalRead(pin);

    Serial.println(value);
}


// =================================================================
// ANALOG INPUT CONFIGURATION
// =================================================================

void configureAnalogInput(int channel)
{
    /*
     * Vérification du canal A0-A7.
     */
    if (!isValidAnalogInput(channel))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid analog input"
        );

        return;
    }


    /*
     * Conversion du canal analogique en numéro de broche.
     *
     * Exemple :
     *
     *    channel 0 -> A0
     *    channel 1 -> A1
     *
     *    etc.
     */
    int pin = A0 + channel;


    /*
     * Configure la broche en entrée.
     *
     * Sur Arduino, analogRead() fonctionne déjà comme une entrée
     * analogique, mais cette configuration rend le comportement
     * explicite.
     */
    pinMode(pin, INPUT);
}


// =================================================================
// ANALOG INPUT QUERY
// =================================================================

void queryAnalogInput(int channel)
{
    /*
     * Vérification du canal.
     */
    if (!isValidAnalogInput(channel))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Invalid analog input"
        );

        return;
    }


    /*
     * Conversion :
     *
     *    0 -> A0
     *    1 -> A1
     *    ...
     *    7 -> A7
     */
    int pin = A0 + channel;


    /*
     * Lecture ADC.
     *
     * ATmega328P :
     *
     *    résolution = 10 bits
     *
     *    0 ... 1023
     */
    int value = analogRead(pin);


    /*
     * Envoi de la valeur au PC.
     */
    Serial.println(value);
}


// =================================================================
// ANALOG OUTPUT / PWM CONFIGURATION
// =================================================================

void configureAnalogOutput(int pin)
{
    /*
     * Vérifie que la broche possède une fonction PWM.
     */
    if (!isValidAnalogOutputPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Pin is not PWM capable"
        );

        return;
    }


    /*
     * Configure la broche comme sortie.
     */
    pinMode(pin, OUTPUT);


    /*
     * Valeur initiale = 0.
     */
    analogWrite(pin, 0);
}


// =================================================================
// ANALOG OUTPUT / PWM SET
// =================================================================

void setAnalogOutput(int pin, int value)
{
    /*
     * Vérification de la broche PWM.
     */
    if (!isValidAnalogOutputPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Pin is not PWM capable"
        );

        return;
    }


    /*
     * Vérification de la plage PWM.
     *
     * Arduino :
     *
     *    0   = 0 %
     *    255 = 100 %
     */
    if (value < 0 || value > 255)
    {
        pushError(
            SCPI_INVALID_PARAMETER,
            "Invalid PWM value"
        );

        return;
    }


    /*
     * Configure automatiquement la broche.
     */
    pinMode(pin, OUTPUT);


    /*
     * Génère le signal PWM.
     */
    analogWrite(pin, value);
}


// =================================================================
// ANALOG OUTPUT / PWM QUERY
// =================================================================

/*
 * IMPORTANT :
 *
 * analogRead() n'est PAS utilisable pour connaître la valeur
 * PWM programmée sur une sortie.
 *
 * Nous conservons donc la dernière valeur demandée dans un tableau.
 */

int pwmValues[14];


/*
 * Lecture de la dernière valeur PWM programmée.
 */
void queryAnalogOutput(int pin)
{
    /*
     * Vérification de la broche PWM.
     */
    if (!isValidAnalogOutputPin(pin))
    {
        pushError(
            SCPI_INVALID_PIN,
            "Pin is not PWM capable"
        );

        return;
    }


    /*
     * Retourne la dernière consigne.
     */
    Serial.println(pwmValues[pin]);
}


// =================================================================
// STRING TRIM
// =================================================================

/*
 * Supprime les espaces et caractères CR/LF inutiles
 * au début et à la fin de la commande.
 */
void trimString(char* str)
{
    int length = strlen(str);


    /*
     * Suppression des caractères à la fin.
     */
    while (length > 0 &&
           (
               str[length - 1] == ' '  ||
               str[length - 1] == '\r' ||
               str[length - 1] == '\n'
           ))
    {
        str[length - 1] = '\0';

        length--;
    }


    /*
     * Recherche du premier caractère non-espace.
     */
    int start = 0;

    while (str[start] == ' ')
    {
        start++;
    }


    /*
     * Déplacement de la chaîne si nécessaire.
     */
    if (start > 0)
    {
        memmove(
            str,
            str + start,
            strlen(str + start) + 1
        );
    }
}


// =================================================================
// COMMAND PARSER
// =================================================================

void processCommand(char* command)
{
    /*
     * Nettoyage de la commande.
     */
    trimString(command);


    /*
     * Commande vide.
     */
    if (strlen(command) == 0)
    {
        return;
    }


    // =============================================================
    // IEEE 488.2
    // =============================================================

    /*
     * Identification de l'instrument.
     */
    if (strcmp(command, "*IDN?") == 0)
    {
        commandIDN();

        return;
    }


    /*
     * Reset.
     */
    if (strcmp(command, "*RST") == 0)
    {
        commandRST();

        return;
    }


    /*
     * Clear status/errors.
     */
    if (strcmp(command, "*CLS") == 0)
    {
        clearErrors();

        return;
    }


    /*
     * Operation complete.
     */
    if (strcmp(command, "*OPC?") == 0)
    {
        commandOPC();

        return;
    }


    // =============================================================
    // SYSTEM
    // =============================================================

    /*
     * Lecture de la première erreur.
     */
    if (strcmp(command, "SYST:ERR?") == 0)
    {
        queryError();

        return;
    }


    /*
     * Version SCPI.
     */
    if (strcmp(command, "SYST:VERS?") == 0)
    {
        commandSystemVersion();

        return;
    }


    // =============================================================
    // DIGITAL INPUT CONFIGURATION
    // =============================================================

    /*
     * Exemple :
     *
     *    DIG:IN:CONF 2
     */
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


    // =============================================================
    // DIGITAL INPUT QUERY
    // =============================================================

    /*
     * Exemple :
     *
     *    DIG:IN? 2
     */
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


    // =============================================================
    // DIGITAL OUTPUT CONFIGURATION
    // =============================================================

    /*
     * Exemple :
     *
     *    DIG:OUT:CONF 4
     */
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


    // =============================================================
    // DIGITAL OUTPUT QUERY
    // =============================================================

    /*
     * Exemple :
     *
     *    DIG:OUT? 4
     */
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


    // =============================================================
    // DIGITAL OUTPUT SET
    // =============================================================

    /*
     * Exemple :
     *
     *    DIG:OUT 4,1
     */
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


    // =============================================================
    // ANALOG INPUT CONFIGURATION
    // =============================================================

    /*
     * Exemple :
     *
     *    ANA:IN:CONF 0
     *
     * correspond à A0.
     */
    if (strncmp(command, "ANA:IN:CONF ", 12) == 0)
    {
        int channel;


        if (sscanf(command + 12, "%d", &channel) != 1)
        {
            pushError(
                SCPI_INVALID_PARAMETER,
                "Invalid parameter"
            );

            return;
        }


        configureAnalogInput(channel);

        return;
    }


    // =============================================================
    // ANALOG INPUT QUERY
    // =============================================================

    /*
     * Exemple :
     *
     *    ANA:IN? 0
     *
     * Réponse :
     *
     *    512
     *
     * pour une tension approximative de 2.5 V avec une référence
     * ADC de 5 V.
     */
    if (strncmp(command, "ANA:IN? ", 8) == 0)
    {
        int channel;


        if (sscanf(command + 8, "%d", &channel) != 1)
        {
            pushError(
                SCPI_INVALID_PARAMETER,
                "Invalid parameter"
            );

            return;
        }


        queryAnalogInput(channel);

        return;
    }


    // =============================================================
    // ANALOG OUTPUT CONFIGURATION
    // =============================================================

    /*
     * Exemple :
     *
     *    ANA:OUT:CONF 3
     *
     * D3 est une sortie PWM.
     */
    if (strncmp(command, "ANA:OUT:CONF ", 13) == 0)
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


        configureAnalogOutput(pin);

        return;
    }


    // =============================================================
    // ANALOG OUTPUT QUERY
    // =============================================================

    /*
     * Exemple :
     *
     *    ANA:OUT? 3
     *
     * Retour :
     *
     *    128
     *
     * si la dernière consigne était ANA:OUT 3,128.
     */
    if (strncmp(command, "ANA:OUT? ", 9) == 0)
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


        queryAnalogOutput(pin);

        return;
    }


    // =============================================================
    // ANALOG OUTPUT SET
    // =============================================================

    /*
     * Exemple :
     *
     *    ANA:OUT 3,128
     */
    if (strncmp(command, "ANA:OUT ", 8) == 0)
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


        /*
         * Écriture PWM.
         */
        setAnalogOutput(pin, value);


        /*
         * Mémorisation de la consigne uniquement si la valeur
         * est valide.
         */
        if (isValidAnalogOutputPin(pin) &&
            value >= 0 &&
            value <= 255)
        {
            pwmValues[pin] = value;
        }


        return;
    }


    // =============================================================
    // UNKNOWN COMMAND
    // =============================================================

    /*
     * Si aucune commande connue n'a été trouvée.
     */
    pushError(
        SCPI_INVALID_COMMAND,
        "Undefined command"
    );
}


// =================================================================
// SERIAL PROCESSING
// =================================================================

void processSerial()
{
    /*
     * Traite tous les caractères disponibles.
     */
    while (Serial.available() > 0)
    {
        char c = Serial.read();


        // =========================================================
        // FIN DE COMMANDE
        // =========================================================

        /*
         * Une commande peut être terminée par :
         *
         *    \n
         *
         * ou :
         *
         *    \r
         *
         * ou :
         *
         *    \r\n
         */
        if (c == '\n' || c == '\r')
        {
            /*
             * Si le buffer contient quelque chose,
             * on traite la commande.
             */
            if (commandIndex > 0)
            {
                commandBuffer[commandIndex] = '\0';

                processCommand(commandBuffer);

                commandIndex = 0;
            }
        }


        // =========================================================
        // AJOUT D'UN CARACTÈRE
        // =========================================================

        else
        {
            /*
             * Vérifie que le buffer n'est pas plein.
             */
            if (commandIndex <
                COMMAND_BUFFER_SIZE - 1)
            {
                commandBuffer[commandIndex] = c;

                commandIndex++;
            }


            // =====================================================
            // BUFFER OVERFLOW
            // =====================================================

            else
            {
                /*
                 * Commande trop longue.
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


// =================================================================
// INITIALISATION
// =================================================================

void setup()
{
    /*
     * Initialisation du port série.
     *
     * IMPORTANT :
     *
     * Le terminal série doit également être configuré à :
     *
     *    115200 bauds
     *
     *    8 bits
     *    No parity
     *    1 stop bit
     */
    Serial.begin(SERIAL_BAUDRATE);


    /*
     * Initialisation des valeurs PWM mémorisées.
     */
    for (int i = 0; i < 14; i++)
    {
        pwmValues[i] = 0;
    }


    /*
     * Petit délai permettant à la liaison série USB de
     * démarrer correctement.
     */
    delay(100);


    /*
     * Reset logiciel.
     */
    resetDevice();
}


// =================================================================
// MAIN LOOP
// =================================================================

void loop()
{
    /*
     * Le Nano reste en permanence à l'écoute
     * des commandes SCPI.
     */
    processSerial();
}

