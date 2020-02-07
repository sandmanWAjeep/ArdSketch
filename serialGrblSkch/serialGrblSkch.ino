/*
 *      change program to only send next gcode when ok is recieved
 *      ***add speed override buttons to touch screen

 !	hold command
 0x9E	toggles the spindle enable if grbl is in hold
 0x90	set feed to 100%
 0x91	+10% feed
 0x92	-10% feed
 ~	resume
 0x18	soft reset

 buttons[19] = feed + 10%
 buttons[18] = feed - 10%
 buttons[17] = soft reset
 *
 */

#include <SD.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <TouchScreen.h>
#include <UTFTGLUE.h>

 //Set-up the touchscreen
#define TOUCH_ORIENTATION  LANDSCAPE
UTFTGLUE myGLCD(0x9486, A2, A1, A3, A4, A0); //Parameters should be adjusted to your Display/Schield model
const int XP = 8, XM = A2, YP = A3, YM = 9; //ID=0x9486
//const int TS_LEFT = 136, TS_RT = 907, TS_TOP = 942, TS_BOT = 139;              //portrait values
//const int TS_LEFT = 961, TS_RT = 94, TS_TOP = 906, TS_BOT = 115;                 //landscape values
//const int TS_LEFT = 907, TS_RT = 115, TS_TOP = 95, TS_BOT = 955;
const int TS_LEFT = 955, TS_RT = 95, TS_TOP = 907, TS_BOT = 115;
int pixel_x, pixel_y;     //Touch_getXY() updates global vars
MCUFRIEND_kbv tft;
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
Adafruit_GFX_Button buttons[20];
#define MINPRESSURE 200
#define MAXPRESSURE 1000

boolean restart = true;
Servo carriageServo;  // create servo object to control a servo
Servo penServo;
bool toolIsHigh = false;
bool toolIsLow = false;
bool SpindleIsOn = false;
bool raiseTool = false;
bool lowerTool = false;
int servoCMD = 90;    // variable to store the servo position
int numFilesOnCard = 1;
char* fileNameArray[20];
int fileSelected = 10;
bool fileHasBeenSelected = false;
String inString = "";    // string to hold input
float arcVoltSetPoint = 2.5;

#define toolHighPin    25   // from laser
#define toolLowPin     27   // from laser
#define Spindle_EnablePin  23   // from GRBL controller
#define carriageServoPin       45
#define modeSwitchPin  29
#define penServoPin    44
#define upButtonPin    31
#define downButtonPin  35
#define plasmaVoltPin  A15
#define SD_CS_PIN SS
#define penDownPos    180
#define penUpPos    20
#define lowerToolVal   75
#define stayStillVal   90
#define raiseToolVal   105
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
int penServoCMD = penUpPos;

File myFile;
File root;
void setup() {

	Serial.begin(115200);
	Serial1.begin(115200);
	carriageServo.attach(carriageServoPin);  // attaches the servo on pin 9 to the servo object
	penServo.attach(penServoPin);
	carriageServo.write(servoCMD);              // start with servo at highest position 
	penServo.write(penServoCMD);
	pinMode(toolHighPin, INPUT_PULLUP);
	pinMode(toolLowPin, INPUT_PULLUP);
	pinMode(Spindle_EnablePin, INPUT_PULLUP);
	pinMode(upButtonPin, INPUT_PULLUP);
	pinMode(downButtonPin, INPUT_PULLUP);
	pinMode(modeSwitchPin, INPUT_PULLUP);

	while (!Serial) {}

	//print list of files stored on SD card
	checkSD();
	root = SD.open("/");
	drawScreen();	
	delay(1000);

}
void loop() {
	printDirectory(root, 0);	
	//text color set here for use in rest of program
	tft.setTextSize(3);
	tft.setTextColor(BLACK, GREEN);	
	while (restart) {

		updateTouchscreen();

		if (fileHasBeenSelected) {
			sendGcode();
		}
		getInputs();
		updateTHC();

		/*while (!digitalRead(toolHighPin)) {  //start pen plotter control loop

			if (SpindleIsOn) { penServoCMD = penDownPos; }
			if (!SpindleIsOn) { penServoCMD = penUpPos; }
			penServo.write(penServoCMD);

			if (raiseTool || toolIsLow) {
				servoCMD = raiseToolVal;
				//Serial.println("going up");
			}
			else if (lowerTool || toolIsHigh) {
				servoCMD = lowerToolVal;
				//Serial.println("going down");
			}
			else {
				servoCMD = stayStillVal;
				//Serial.println("steady");
			}
		}//end pen plotter control loop

		while (digitalRead(toolHighPin)) {     //start plasma control loop
			int sensorValue = analogRead(plasmaVoltPin);
			float voltage = sensorValue * (5.0 / 1023.0);
		}  //end plasma control loop

		carriageServo.write(servoCMD);
		delay(50);*/
	}
}
void checkSD() {

	// Check if SD card is OK

	// On the Ethernet Shield, CS is pin 4. It's set as an output by default.
	// Note that even if it's not used as the CS pin, the hardware SS pin 
	// (10 on most Arduino boards, 53 on the Mega) must be left as an output 
	// or the SD library functions will not work.

	while (!SD.begin(53)) {
		Serial.println("Please insert SD card...\n");
		delay(1000);
	}

	Serial.println("SD card OK...\n");
	delay(1000);
	root = SD.open("/");
	Serial.println("done!");
}
void printDirectory(File dir, int numTabs) {	
	int cursor_y = 40;

	while (true) {
		SDFile entry = dir.openNextFile();
		if (!entry) {
			// no more files
			break;
		}
		if (!entry.isDirectory()) {   //do this for each file on the root of the card


			char* temp = entry.name();
			String toString = String(temp); //converted filename to String
			//toString = toString.substring(0, toString.length() - 4); //made a substring to exempt the file extension(remove the *.txt)
			toString.toCharArray(temp, toString.length() + 1); //converted it back to char array for it to be stored in char* fileNameArray
			fileNameArray[numFilesOnCard] = strdup(temp);
			//fileNameArray[numFilesOnCard] = entry.name();
			Serial.print("fileNameArray[");
			Serial.print(numFilesOnCard);
			Serial.print("] = ");
			Serial.println(fileNameArray[numFilesOnCard]);
			delay(500);
			buttons[numFilesOnCard].initButton(&tft, 110, cursor_y += 45, 200, 40, WHITE, CYAN, BLACK, temp, 2);
			buttons[numFilesOnCard].drawButton();
			numFilesOnCard++;
		}
		entry.close();
	}
	Serial.print("Number of files on card = ");
	Serial.println(numFilesOnCard);		
}
void openFileSD(int fromupdateTS) {  //fileSeleced is passed in from updateTS()
	Serial.print("number sent from updateTS = ");
	Serial.println(fromupdateTS);
	String fileName = fileNameArray[fromupdateTS];
	Serial.print("fileNameArray[fromupdateTS] is = ");
	Serial.println(fileNameArray[fromupdateTS]);
	Serial.print("fileName is = ");
	Serial.println(fileName);
	delay(3000);

	emptySerialBuf(0);

	if (!SD.exists(fileName)) {       //check if file already exists
		Serial.print("-- ");
		Serial.print(fileName);
		Serial.print(" doesn't exists");
		Serial.println(" --");
		Serial.println("Please select another file\n");
		delay(200);
	}
	else {
		myFile = SD.open(fileName, FILE_READ);		//create a new file
		Serial.print("-- ");
		Serial.print("File : ");
		Serial.print(fileName);
		Serial.print(" opened!");
		Serial.println(" --\n");
		fileHasBeenSelected = true;
	}
}
void emptySerialBuf(int serialNum) {
	//Empty Serial buffer
	if (serialNum == 0) {
		while (Serial.available())
			Serial.read();
	}
	else if (serialNum == 1) {
		while (Serial1.available())
			Serial1.read();
	}
}
void waitSerial(int serialNum) {

	// Wait for data on Serial
	//Argument serialNum for Serial number

	boolean serialAv = false;

	if (serialNum == 0) {
		while (!serialAv) {
			if (Serial.available())
				serialAv = true;
		}
	}
	else if (serialNum == 1) {
		while (!serialAv) {
			if (Serial1.available())
				serialAv = true;
		}
	}
}
String getSerial(int serialNum) {

	//Return String  from serial line reading
	//Argument serialNum for Serial number

	String inLine = "";
	waitSerial(serialNum);

	if (serialNum == 0) {
		while (Serial.available()) {
			inLine += (char)Serial.read();
			delay(2);
		}
		return inLine;
	}
	else if (serialNum == 1) {
		while (Serial1.available()) {
			inLine += (char)Serial1.read();
			delay(2);
		}
		return inLine;
	}
}
void sendGcode() {

	//READING GCODE FILE AND SEND ON SERIAL PORT TO GRBL

	//START GCODE SENDING PROTOCOL ON SERIAL 1

	String line = "";

	Serial1.print("\r\n\r\n");      //Wake up grbl
	delay(2);
	emptySerialBuf(1);
	if (myFile) {
		while (myFile.available()) {    //until the file's end
			line = readLine(myFile);  //read line in gcode file 
			Serial.print(line);   //send to serial monitor
			Serial1.print(line);    //send to grbl
			Serial.print(getSerial(1)); //print grbl return on serial
			updateTHC();
			checkButtons();
		}
	}
	else
		fileError();

	myFile.close();
	Serial.println("Finish!!\n");
	delay(2000);
}
void fileError() {

	// For file open or read error

	Serial.println("\n");
	Serial.println("File Error !");
}
String readLine(File f) {

	//return line from file reading

	char inChar;
	String line = "";

	do {
		inChar = (char)f.read();
		line += inChar;
	} while (inChar != '\n');    //keeps adding chars to line untill inChar = \n

	return line;
}
void getInputs() {
	toolIsHigh = !digitalRead(toolHighPin);
	toolIsLow = !digitalRead(toolLowPin);
	SpindleIsOn = !digitalRead(Spindle_EnablePin);
	raiseTool = !digitalRead(upButtonPin);
	lowerTool = !digitalRead(downButtonPin);
}
bool Touch_getXY(void) {
	TSPoint p = ts.getPoint();
	pinMode(YP, OUTPUT);      //restore shared pins
	pinMode(XM, OUTPUT);
	digitalWrite(YP, HIGH);   //because TFT control pins
	digitalWrite(XM, HIGH);
	bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
	if (pressed) {
		pixel_x = map(p.y, TS_LEFT, TS_RT, 0, 480); //.kbv makes sense to me
		pixel_y = map(p.x, TS_TOP, TS_BOT, 0, 320);
		Serial.print("pixel_x = ");
		Serial.println(pixel_x);
		Serial.print("pixel_y = ");
		Serial.println(pixel_y);
	}
	return pressed;
}
void updateTouchscreen() {
	bool down = Touch_getXY();

	for (int i = 0; i < numFilesOnCard; i++) {
		buttons[i].press(down && buttons[i].contains(pixel_x, pixel_y));
		if (buttons[i].justPressed()) {
			Serial.println("you pressed button ");
			Serial.println(i);
			delay(1000);
			int fileNumber = int(i);
			openFileSD(i);
		}
	}
}
void drawScreen() {
	tft.reset();
	uint16_t ID = tft.readID();
	tft.begin(ID);
	tft.setRotation(1);            //LANDSCAPE
	tft.fillScreen(BLACK);
	//print welcome message
	tft.setCursor(10, 20);
	tft.setTextSize(3);
	tft.println("Sanders' CNC Gantry");	
	tft.fillRect(380, 10, 100, 200, WHITE);   //   (x, y, width, height) xy is upper left corner of rectangle
	tft.fillRect(390, 90, 80, 40, GREEN);    // block for current arc voltage
	tft.fillRect(390, 160, 80, 40, GREEN);    // block for arc voltage setpoint	
	tft.setTextColor(BLACK);
	tft.setTextSize(2);
	tft.setCursor(410, 12);
	tft.print("arc");
	tft.setCursor(390, 32);
	tft.print("voltage");
	tft.setCursor(392, 70);
	tft.print("actual");
	tft.setCursor(382, 140);
	tft.print("setpoint");	
	drawButtons();
}
int intFromSerial() {
	// Read serial input:
	while (Serial.available() > 0) {
		int inChar = Serial.read();
		if (isDigit(inChar)) {
			// convert the incoming byte to a char and add it to the string:
			inString += (char)inChar;
			inString += (char)inChar;
		}
		// if you get a newline, print the string, then the string's value:
		if (inChar == '\n') {
			Serial.print("Value:");
			int valueIn = (inString.toInt());
			Serial.println(valueIn);
			Serial.print("String: ");
			Serial.println(inString);
			// clear the string for new input:
			inString = "";
			return valueIn;
		}
	}
}
void updateTHC() {
	float voltage = getVoltage();
	//put logic for adjusting torch height here
	if (voltage < (arcVoltSetPoint - 1)) {                //gap is to small, raise the torch
		servoCMD = raiseToolVal;
	}
	if (voltage > (arcVoltSetPoint + 1)) {                //gap is to wide, lower the torch
		servoCMD = lowerToolVal;
	}
	if (voltage == arcVoltSetPoint) {                //gap is ok, do not move torch
		servoCMD = stayStillVal;
	}
	carriageServo.write(servoCMD);


	static const unsigned long displayUpdateInterval = 1000; // ms
	static unsigned long lastRefreshTime = 0;
	if (millis() - lastRefreshTime >= displayUpdateInterval)
	{
		lastRefreshTime += displayUpdateInterval;
		displayVoltage(voltage);
	}
}
float getVoltage() {
	int sensorValue = analogRead(plasmaVoltPin);
	float voltage = sensorValue * (5.0 / 1023.0);
	return voltage;
}
void displayVoltage(float curVolts) {	
	tft.setCursor(393, 99);
	tft.print(curVolts);
	tft.setCursor(393, 169);
	tft.print(arcVoltSetPoint);
}
void drawButtons() {	
	Serial.println("drawing fixed buttons");
	delay(1000);
	//btn num                   xPos, yPos, W, H,  border, color,   text,  label,textsize
	buttons[19].initButton(&tft, 430, 270, 80, 80, YELLOW, MAGENTA, BLACK, "HOLD", 2);
	buttons[19].drawButton();
	buttons[18].initButton(&tft, 340, 270, 80, 80, YELLOW, MAGENTA, BLACK, "RES", 2);
	buttons[18].drawButton();
	buttons[17].initButton(&tft, 250, 270, 80, 80, YELLOW, MAGENTA, BLACK, "TORCH", 2);
	buttons[17].drawButton();
}
void checkButtons() {
	bool down = Touch_getXY();

	buttons[19].press(down && buttons[19].contains(pixel_x, pixel_y));
	if (buttons[19].justPressed()) {
		Serial.println("you pressed speed + ");
		Serial1.print("0x91");
		delay(1000);		
	}
	buttons[18].press(down && buttons[18].contains(pixel_x, pixel_y));
	if (buttons[18].justPressed()) {
		Serial.println("you pressed speed - ");
		Serial1.print("0x92");
		delay(1000);
	}
	buttons[17].press(down && buttons[17].contains(pixel_x, pixel_y));
	if (buttons[17].justPressed()) {
		Serial.println("you pressed toggle torch ");
		Serial1.print("0x9E");
		delay(1000);
	}
}
