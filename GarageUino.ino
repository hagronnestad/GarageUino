/**
 * GarageUino 1.0 - An Arduino project!
 *
 * My first Arduino project that actually resulted in a product I use.
 * It's an Internet enabled garage port opener with a LCD display.
 *
 * It uses an Arduino Uno, an ethernet board, a relay shield and other
 * smaller components neatly packed in a black plastic box with an
 * ethernet and a USB port.
 *
 * It can also be operated remotely from an Android app on my phone!
 * 
 * @author Hein André Grønnestad <hag@haugnett.no>
 * @link http://xdevelopers.net
 * 
 */


#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <SPI.h>
#include <Ethernet.h>

// Initialize LCD library with I2C address 0x27 and 20 by 4 characters
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Custom chars
uint8_t P1[8] = {B10000, B10000, B10000, B10000, B10000, B10000, B10000, B10000}; // |
uint8_t P2[8] = {B11000, B11000, B11000, B11000, B11000, B11000, B11000, B11000}; // ||
uint8_t P3[8] = {B11100, B11100, B11100, B11100, B11100, B11100, B11100, B11100}; // |||
uint8_t P4[8] = {B11110, B11110, B11110, B11110, B11110, B11110, B11110, B11110}; // ||||
uint8_t P5[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111}; // |||||

// Inputs
int DI_BUTTON = A1;                     //   Button - Open / Close Garage Door
int DI_SENSOR_DOOR_CLOSED = 2;          //   Sensor Garage Door Closed
int DI_SENSOR_DOOR_OPEN = 8;            //   Sensor Garage Door Open

// Outputs
int DO_LED_GREEN = 3;                   // ~ LED Green - Power
int DO_LED_RED = 5;                     // ~ LED Red - Fault / Error
int DO_LED_YELLOW_DOOR_CLOSED = 6;      // ~ LED Yellow - Garage Door Closed
int DO_LED_YELLOW_DOOR_OPEN = 9;        // ~ LED Yellow - Garage Door Open
int DO_R1_DOOR_OPENER = 7;              //   Relay Shield R1 - Garage Door Opener Remote Control Toggle

// Timing variables
unsigned long currentMillis = 0;
unsigned long lastMillis = 0;
int tick10Counter = 0;

// LCD buffer, whatever is written to screen is also stored here
String lcdBuffer[4];

// Misc
bool isConnectedToLAN = false;

bool doorOpen = false;
bool doorOpenLast = false;
bool doorClosed = false;
bool doorClosedLast = false;
bool doorOpening = false;
bool doorClosing = false;
unsigned long doorStartedMovingMillis = 0;
unsigned long doorJammedTimeout = 25000;
unsigned long doorOpenTime = 20000;
unsigned long doorCloseTime = 20000;

bool hasError = false;

bool buttonPressed = false;
bool buttonPressedLast = false;
bool buttonIsActive = false;

// Ethernet
EthernetServer server = EthernetServer(443);
String receivedData;

bool clientButtonPressed = false;



/**
 * Set up
 */
 void setup() {
    //Serial.begin(9600);

    // Init LCD and show the boot message
    lcd.init();
    lcd.backlight();

    lcd.createChar(0, P1);
    lcd.createChar(1, P2);
    lcd.createChar(2, P3);
    lcd.createChar(3, P4);
    lcd.createChar(4, P5);
    

    // Write the project name and the booting text to the LCD
    lcdPrintLine("GarageUino 1.0", 0);
    lcdPrintLine("Booting...", 2);
    delay(2000);
    

    // Set pin modes and activate the pull up resistors on the digital inputs
    pinMode(DI_BUTTON, INPUT); digitalWrite(DI_BUTTON, HIGH);
    pinMode(DI_SENSOR_DOOR_CLOSED, INPUT); digitalWrite(DI_SENSOR_DOOR_CLOSED, HIGH);
    pinMode(DI_SENSOR_DOOR_OPEN, INPUT); digitalWrite(DI_SENSOR_DOOR_OPEN, HIGH);
    
    pinMode(DO_LED_GREEN, OUTPUT);
    pinMode(DO_LED_RED, OUTPUT);
    pinMode(DO_LED_YELLOW_DOOR_CLOSED, OUTPUT);
    pinMode(DO_LED_YELLOW_DOOR_OPEN, OUTPUT);
    pinMode(DO_R1_DOOR_OPENER, OUTPUT);
    
    
    // LED Test
    lcdPrintLine("LED Test", 2);
    lcdPrintLine("", 3);
    digitalWrite(DO_LED_GREEN, HIGH);
    digitalWrite(DO_LED_RED, HIGH);
    digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, HIGH);
    digitalWrite(DO_LED_YELLOW_DOOR_OPEN, HIGH);
    delay(2000);
    digitalWrite(DO_LED_GREEN, LOW);
    digitalWrite(DO_LED_RED, LOW);
    digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, LOW);
    digitalWrite(DO_LED_YELLOW_DOOR_OPEN, LOW);
    
    
    // Obtain an IP address through DHCP and update the LCD
    lcdPrintLine("DHCP", 2);
    lcdPrintLine("Obtaining IP...", 3);
    byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDA, 0x02};

    if (Ethernet.begin(mac) == 1) {
        // Start listening for connections
        server.begin();

        isConnectedToLAN = true;

        lcdPrintLine("DHCP Success!", 2);
        lcdPrintLine("IP: " + getIPAsString(), 3);
        delay(2000);
        
        // Show the IP under the title and clear the last two lines
        lcdPrintLine("LAN: " + getIPAsString(), 1);
        lcdPrintLine("", 2);
        lcdPrintLine("", 3);
        
    } else {
        isConnectedToLAN = false;

        digitalWrite(DO_LED_RED, HIGH);
        lcdPrintLine("DHCP Error!", 2);
        lcdPrintLine("Failed to obtain IP!", 3);
        delay(2000);
        lcdPrintLine("Not critical, go on.", 3);
        digitalWrite(DO_LED_RED, LOW);
        delay(1000);
        
        // Show the no network message and clear the last two lines
        lcdPrintLine("LAN: Not connected!", 1);
        lcdPrintLine("", 2);
        lcdPrintLine("", 3);
    }
    
    
    // Turn on the power LED to indicate that the device is functioning properly
    digitalWrite(DO_LED_GREEN, HIGH);
    lcdPrintLine("Ready!", 3);
    delay(1000);
    lcdPrintLine("", 3);
    

    // Set initial door position LED states
    // digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, digitalRead(DI_SENSOR_DOOR_CLOSED));
    // digitalWrite(DO_LED_YELLOW_DOOR_OPEN, digitalRead(DI_SENSOR_DOOR_OPEN));
}





/**
 * The loop
 */
void loop() {

    // Accept pending connections
    EthernetClient client = server.available();

    // Receive data
    if (client) {
        if (client.available()) {
            char c = client.read();

            if (c == char(13)) {
                
                if (receivedData == "password pushButton") {
                    // If there is an active error the button resets the error
                    // instead of activating the garage door sender
                    if (hasError) {
                        clearErrorMessage();

                    } else {
                        // Activate the garage door sender when the button is pressed
                        digitalWrite(DO_R1_DOOR_OPENER, HIGH);
                        delay(300);
                        digitalWrite(DO_R1_DOOR_OPENER, LOW);
                    }
                }

                if (receivedData == "password getLCD") {
                    client.print(lcdBuffer[0]);
                    client.print(lcdBuffer[1]);
                    client.print(lcdBuffer[2]);
                    client.print(lcdBuffer[3]);
                }

                if (receivedData == "password getLED") {
                    client.write(digitalRead(DO_LED_GREEN));
                    client.write(digitalRead(DO_LED_RED));
                    client.write(digitalRead(DO_LED_YELLOW_DOOR_CLOSED));
                    client.write(digitalRead(DO_LED_YELLOW_DOOR_OPEN));
                }
                
                receivedData = "";

            } else if (c != char(10)) {
                receivedData += c;
            }
        }
    }

    // Calles a set of functions at different intervals.
    createTicks();
}


/**
 * This method fires tick methods when it is placed in the loop
 * Configure tick periods as needed
 */
void createTicks() {
    currentMillis = millis();
  
    // The UL after the compared value means that the value should be treated as an unsigned long.
    // currentMillis and lastMillis are both unsigned long variables and if they were to be compared
    // against an int, weird stuff would happen, because currentMillis and lastMillis would be
    // treated as ints as well.
    // 
    // Yes! I know I could have just created another unsigned long variable for the value, but then I 
    // wouldn't get to write all this stuff and you wouldn't have learned that it could be done like this.
    if ((currentMillis - lastMillis) >= 10UL) {
        lastMillis = currentMillis;
    
        tick10Counter++;

        tick10();
        if (! (tick10Counter % 5)) tick50();
        if (! (tick10Counter % 10)) tick100();
        if (! (tick10Counter % 25)) tick250();
        if (! (tick10Counter % 50)) tick500();
        if (! (tick10Counter % 100)) tick1000();
        if (! (tick10Counter % 500)) tick5000();
    
        if (tick10Counter >= 500) tick10Counter = 0;
    }
}


// Called every ~10 milliseconds
void tick10() {

}

// Called every ~50 milliseconds
void tick50() {

}

// Called every ~100 milliseconds
void tick100() {

    /**
     * Button logic
     *
     * NOTE! Signal is low when the button is pressed, that's why it's inverted.
     * 
     * The buttonIsActive variable makes sure that the button has to be released
     * before the "Button was pressed" code runs again. Otherwise the garage door
     * sender would be activated at the next loop/tick after an error gets reset.
     */
    buttonPressed = !digitalRead(DI_BUTTON);

    // Button was pressed
    if (buttonPressed && !buttonPressedLast && !buttonIsActive) {
        buttonIsActive = true;

        // If there is an active error the button resets the error
        // instead of activating the garage door sender
        if (hasError) {
            clearErrorMessage();

        } else {
            // Activate the garage door sender when the button is pressed
            digitalWrite(DO_R1_DOOR_OPENER, HIGH);
        }
    }

    // Button was released
    if (!buttonPressed && buttonPressedLast) {
        buttonIsActive = false;

        // Deactivate the garage door sender when the button is released
        digitalWrite(DO_R1_DOOR_OPENER, LOW);
    }

    buttonPressedLast = buttonPressed;



    /**
     * Door position logic
     */
    doorClosed = digitalRead(DI_SENSOR_DOOR_CLOSED);
    doorOpen = digitalRead(DI_SENSOR_DOOR_OPEN);

    // Garage door is fully open
    if (doorOpen && !doorOpenLast && !doorClosed) {
        digitalWrite(DO_LED_YELLOW_DOOR_OPEN, HIGH);
        digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, LOW);
        if (!hasError) lcdPrintLine("", 2);
        if (!hasError) lcdPrintLine("( ) Closed  Open (X)", 3);
        doorOpening = false;
        doorClosing = false;
    }

    // Garage door is fully closed
    if (doorClosed && !doorClosedLast && !doorOpen) {
        digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, HIGH);
        digitalWrite(DO_LED_YELLOW_DOOR_OPEN, LOW);
        if (!hasError) lcdPrintLine("", 2);
        if (!hasError) lcdPrintLine("(X) Closed  Open ( )", 3);
        doorOpening = false;
        doorClosing = false;
    }

    // Garage door started closing
    if (!doorOpen && doorOpenLast && !doorClosed) {
        digitalWrite(DO_LED_YELLOW_DOOR_OPEN, LOW);
        digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, LOW);
        if (!hasError) lcdPrintLine("", 3);
        doorOpening = false;
        doorClosing = true;
        doorStartedMovingMillis = millis();
    }

    // Garage door started opening
    if (!doorClosed && doorClosedLast && !doorOpen) {
        digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, LOW);
        digitalWrite(DO_LED_YELLOW_DOOR_OPEN, LOW);
        if (!hasError) lcdPrintLine("", 3);
        doorOpening = true;
        doorClosing = false;
        doorStartedMovingMillis = millis();
    }

    doorClosedLast = doorClosed;
    doorOpenLast = doorOpen;



    /**
     * Fault conditions
     */
    if (!hasError && doorOpen && doorClosed) {
        showErrorMessage("Position sensors!");
    }

    if (!hasError && (doorOpening || doorClosing) && ((millis() - doorStartedMovingMillis) >= doorJammedTimeout)) {
        showErrorMessage("Jammed / Half-open!");
    }


    /**
     * Progressbars
     */
    if (!hasError && doorOpening) {
        drawProgressbar("Opening...", (millis() - doorStartedMovingMillis), doorOpenTime);
    }
    if (!hasError && doorClosing) {
        drawProgressbar("Closing...", (millis() - doorStartedMovingMillis), doorCloseTime);
    }
}

// Called every ~250 milliseconds
void tick250() {}

// Called every ~500 milliseconds
void tick500() {

    // Flash the door open LED when the door is opening
    if (doorOpening) {
        digitalWrite(DO_LED_YELLOW_DOOR_OPEN, !digitalRead(DO_LED_YELLOW_DOOR_OPEN));
    }

    // Flash the door closed LED when the door is closing
    if (doorClosing) {
        digitalWrite(DO_LED_YELLOW_DOOR_CLOSED, !digitalRead(DO_LED_YELLOW_DOOR_CLOSED));
    }
}

// Called every ~1000 milliseconds
void tick1000() {
    // server.println("0 " + lcdBuffer[0]);
    // server.println("1 " + lcdBuffer[1]);
    // server.println("2 " + lcdBuffer[2]);
    // server.println("3 " + lcdBuffer[3]);
}

// Called every ~5000 milliseconds
void tick5000() {}


/**
 * Prints a string on the specified line on the LCD
 * The text is automatically centered
 */
void lcdPrintLine(String text, int line) {
    lcdPrintLine(text, line, true);
}
void lcdPrintLine(String text, int line, bool centered) {
    String bufferedLine = lcdBuffer[line];
    bufferedLine.trim();
    if (bufferedLine == text) return;

    if (text.length() > 20) text = text.substring(0, 20);

    lcd.setCursor(0, line);
    // lcd.print("                    ");

    // int offset = centered ? floor((20 - text.length()) / 2) : 0;

    // lcd.setCursor(offset, line);
    text = createLCDString(text, centered);
    lcd.print(text);

    updateLCDBuffer(text, line);
}
String createLCDString(String text, bool centered) {
    String padding = "                    ";
    String ret = "";
    
    if (centered) {
        int offset = floor((20 - text.length()) / 2);
        ret = padding.substring(0, offset);
    }

    ret += text;
    ret += padding.substring(0, 20 - ret.length());

    return ret;
}
void updateLCDBuffer(String text, int line) {
    lcdBuffer[line] = text;
}


/**
 * Returns the current IP address as a string
 */
String getIPAsString() {
    String ip = "";
    
    for (int i=0; i < 4; i++) {
        ip += Ethernet.localIP()[i];
        
        if (i < 3) ip += ".";
    }
    
    ip.trim();
    return ip;
}


/**
 * Shows an error text on the LCD and turns on the red LED
 * @param error The error message to show on the LCD
 */
void showErrorMessage(String error) {
    hasError = true;
    digitalWrite(DO_LED_RED, HIGH);
    lcdPrintLine("ERROR!", 2);
    lcdPrintLine(error, 3);
}

/**
 * Clears any error message on the LCD and turns off the red LED
 */
void clearErrorMessage() {
    // We have to reset the doorStartedMovingMillis variable to be able to
    // activate the garage door sender. If not the error would instantly reappear.
    doorStartedMovingMillis = millis();

    hasError = false;

    digitalWrite(DO_LED_RED, LOW);
    lcdPrintLine("", 2);
    lcdPrintLine("", 3);
}



/**
 * Draw progressbar
 */
void drawProgressbar(String barName, unsigned long currentValue, unsigned long maxValue) {
    // Print the descriptive text for the progress bar.
    lcdPrintLine(barName, 2);

    // Calculate the percentage and cap it at 100%
    int percent = (currentValue * 100) / maxValue;
    if (percent > 100) percent = 100;

    // Calculate the number of whole characters to fill.
    // We have 20 characters available, so let's divide
    // the percent by 5 and then floor the result.
    int wholes = floor(percent / 5);

    // Print the whole characters to the LCD
    for (int i = 0; i < wholes; ++i)
    {
        lcd.setCursor(i, 3);
        lcd.write(4);
    }

    // Calculate the number of partials to add to the bar
    int partials = percent % 5;

    // Print the partials.
    // The partials are saved in the 0-3 position in the LCD,
    // partials will always be between 1 and 4, so we can just
    // subtract 1 and that will be the correct index.
    if (partials > 0) {
        lcd.setCursor(wholes, 3);
        lcd.write(partials - 1);
    }

    // Write a simpler progress bar to the lcdBuffer[]
    int barLength = (20 * percent) / 100;
    String barText = "####################";
    barText = barText.substring(0, barLength);

    // Update the LCD buffer with the progress bar
    updateLCDBuffer(createLCDString(barText, false), 3);
}