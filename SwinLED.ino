

// 
/*
    Name:       SwinLed.ino
    Version:	2019-07-15 22:10
    Author:     EVONET1\Chris Moller

	This comes from https://forum.arduino.cc/index.php?topic=60014.0
*/

//
// This is fully tested for the West box, but is likely to need fixes to the lookup table for East
//
//  You will also need filestuff.h and filestuff.cpp in the same folder.

#define MAX_ROUTING_ENTITIES 90

#include "Arduino.h"
#include <Wire.h>
#include <Centipede.h>
#include "fileStuff.h"

const String swVersion = "1.0.7 21Jul2019";


const bool DEBUG = false;
//Set this true to output everything that goes to the display to the serial port as well
//Note that in this case, the program will NOT keep time! 

#define WESTPIN  12   //short pin to ground if EAST

Centipede CS;       // create Centipede object

unsigned int loopCount;

void setup()
{
	Serial.begin(9600);


	hwInit();

	if (DEBUG) {
		Serial.print("This board is ");
		if (isWest()) {
			Serial.println("West");
		} else {
			Serial.println("East");
		}
	}
	timerInit();
	clearLeds();
	while (detectSwitches() != 0);  //spin until all current states recorded
	if (DEBUG) Serial.println("Running...");


}

bool clearFlag = true;

void loop()
{

	unsigned int startSwitch = detectSwitches();
	if (bitRead(startSwitch, 15)) {    //something has changed
		if (!bitRead(startSwitch, 14)) {    //unless all clear
			
			//only clear everything if last time we came through there were no pending switch changes
			if (clearFlag) {
				clearLeds();
				clearFlag = false;
			}
			//this is where the meat of the program is!
			byte routingEntity = lowByte(startSwitch);
			extend(routingEntity, true, 0xFF);  //do westward
			extend(routingEntity, false, 0xFF);  //do estward
			//timer1.reset(5);
			//checkLeds();
		}
		else {
			clearLeds();
		}

	}
	else {
		clearFlag = true;
	}

	//else not quite sure how we're going to deal with all levers OFF!
	

}

//================================================================
//       Subroutines
//================================================================

void extend(int entityNo, bool westward, int comingFromEntity) {
	//entityNo is the entity we're at, determining where to go next
	//westward determines whether we're working westwards or eastwards
	//comingFromEntity is the entity we've just come from (0xFF if starting from here)

	//returns the next entity along the route, if any
	// set first if this is the point where we are starting out from
	//calls itself recursively until done


	//find out what the three options are for this entity
	byte aTo, bTo, cTo;		//entities that a, b and c lead to
	if (isWest()) {
		aTo = westNet[entityNo].aLeadsTo;
		bTo = westNet[entityNo].bLeadsTo;
		cTo = westNet[entityNo].cLeadsTo;
	}
	else {
		aTo = eastNet[entityNo].aLeadsTo;
		bTo = eastNet[entityNo].bLeadsTo;
		cTo = eastNet[entityNo].cLeadsTo;
	}


	/*  If we're coming here for the first time, then
			If the point is leading west and we're going west,
			or the point is leading east, and we're going east
				then we're deemed to be coming on on 'a'
			If we're on a trailing point, then
				If the point is not operated, we're coming in on 'b'
				If the point is operated, we're coming in on 'c'

		If we're not coming here for the first time, then we check which of the 
		three 'a','b',or 'c' leads back to the entity we're coming from, and we
		are deemed to be coming from there.
	*/

	bool operated = readSwitch(entityNo);
	bool leadWest = leadsWest(entityNo);
	char arrivingAt;	//which leg, a,b, or c we're coming in from

	//If this is a stop section signal, and the switch is backed, just show a red and stop here
	if (stopEntity(entityNo) && !operated) {
		setLed(entityNo, false, true, false);   //set Red LED
		return; //no more possible
	}

	if (comingFromEntity == 0xFF) {	//first time
		if (westward == leadWest) {  //leading
			arrivingAt = 'a';
		}
		else {
			if (operated) {	//operated
				arrivingAt = 'c';
			}
			else {
				arrivingAt = 'b';
			}
		}
	}
	else {	//not first time
    //work out which of the three lines we are coming in on
		if (comingFromEntity == aTo) arrivingAt = 'a';
		if (comingFromEntity == bTo) arrivingAt = 'b';
		if (comingFromEntity == cTo) arrivingAt = 'c';
	}

  if (DEBUG) {
     if (westward) {
      Serial.print("W<- coming from ");
    }
    else {
      Serial.print("->E coming from ");

    }
    Serial.print(comingFromEntity);
    Serial.print(" arriving at ");
    Serial.print(entityNo);
    Serial.print("(");
    if (leadWest) {
      Serial.print("(LW)(");
    }else{
      Serial.print("(LW)(");
    }
		Serial.print(arrivingAt);
		if (operated) {
			Serial.print(")(operated)");
		}
		else {
			Serial.print(")(not operated)");
		}
	}


	//so now we know what the options are, and which leg we're coming in on

	//next find which leg we're going to go out on, if any
	byte nextEntity;
	switch (arrivingAt) {
	case 'a':		//coming in on 'a' is never a problem
		if (operated) {
			nextEntity = cTo;
		}
		else {
			nextEntity = bTo;
		}
		break;
	case 'b':
		if (operated) {	
			nextEntity = 0;  //opposing point!!
		}
		else {
			nextEntity = aTo;
		}
		break;
	case 'c':
		if (operated) {	
			nextEntity = aTo;
		}
		else {
			nextEntity = 0;  //opposing point!!
		}
		break;
	default:
		nextEntity = 0;  //opposing point!!
	}	//end switch


	if (DEBUG) {
		Serial.print(" next will be ");
		Serial.print(nextEntity);
	}

	if (nextEntity == 0) {
    if (DEBUG) {
      Serial.print(" set Red, quit");
     }
		setLed(entityNo, false, true, false);   //set Red LED
		return; //no more possible
	}


	if (nextEntity == 0xFF) {
    if (DEBUG) {
      Serial.print(" set White/green, quit");
     }
		setLed(entityNo, true, true, false);   //set White LED, but still give up going further
		return; //no more possible
	}

 if (DEBUG) {
    Serial.print(" set White/green");
   }
	setLed(entityNo, true, true, false);   //set white/green LED

	extend(nextEntity, westward, entityNo);	 //recursive call

	return;

}



bool leadsWest(byte entityNo) {
	//find if a point leads to the west

	bool westVal;
	if (isWest()){
		westVal = (westNet[entityNo].switchLine > 127);
	}
	else {
		westVal = (eastNet[entityNo].switchLine > 127);
	}
	return (westVal);
}



byte leads(byte entityNo, char which) {
	//find where A, B or C leads
	byte leadsTo;
	if (isWest()){
		switch (which) {
		case 'a':
			leadsTo = westNet[entityNo].aLeadsTo & 0x7F;
			break;
		case 'b':
			leadsTo = westNet[entityNo].bLeadsTo;
			break;
		case 'c':
			leadsTo = westNet[entityNo].cLeadsTo;
			break;
		default:
			leadsTo = 0xff;
			break;
		}
	}
	else {
		switch (which) {
		case 'a':
			leadsTo = eastNet[entityNo].aLeadsTo;
			break;
		case 'b':
			leadsTo = eastNet[entityNo].bLeadsTo;
			break;
		case 'c':
			leadsTo = eastNet[entityNo].cLeadsTo;
			break;
		default:
			leadsTo = 0xff;
			break;
		}
	}
	return(leadsTo);
}



//================================================================
//                      I/O routines
//================================================================

void hwInit()  //initialise the I/O
{
	//initialise the hardware

	// initialize digital pin LED_BUILTIN as an output.
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(WESTPIN, INPUT_PULLUP);


	Wire.begin(); // start I2C

	CS.initialize(); // set all registers to default

  if (isWest()) {   //extra LEDs output for West stop boards
    for (int dInit = 2; dInit <= 7; dInit++) {
      pinMode(dInit, OUTPUT);
    }
  }

	//	Serial.begin(9600);

	//Map which Centipede lines are inputs (all other are outputs)
	unsigned int ioMap[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	byte thisSwitch;
	for (int ioIndex = 0; ioIndex < MAX_ROUTING_ENTITIES; ioIndex++) {
		if (isWest()) {
			if (westNet[ioIndex].aLeadsTo == 0) break;  //end of list
			thisSwitch = westNet[ioIndex].switchLine;
		}
		else {
			if (eastNet[ioIndex].aLeadsTo == 0) break;  //end of list
			thisSwitch = eastNet[ioIndex].switchLine;
		}
		//		thisSwitch = testNet[ioIndex].switchLine + 3;
		if (thisSwitch != 255) {   //ignore any routing entity with a switchline of 255
			thisSwitch = thisSwitch & 0x7F;
			bitSet(ioMap[thisSwitch / 16], thisSwitch % 16);
      if (DEBUG) {
  			Serial.print("Setting ");
  			Serial.print(thisSwitch);
  			Serial.println(" as input");
      }
		}
	}
	//now implement port directions and pullups
	for (byte thisPort = 0; thisPort < 8; thisPort++) {
		CS.portMode(thisPort, ioMap[thisPort]); // set directions
		CS.portPullup(thisPort, ioMap[thisPort]); // set pullups

	}


  // **** We could implement a warning here if some switches are set! *****


	//TWBR = 12; // uncomment for 400KHz I2C (on 16MHz Arduinos)

	timerInit();

}


//bits for 128 entities - last recorded switch values
unsigned int entitySwitchValues[8] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };


unsigned int detectSwitches() {
	//Return 0 if nothing has changed
	//Set MSB(15) if something has changed
	//Set Bit 14 if the new situation is all switches backed ON
	//Set Bit 13 to the value of the switch that changed - Set to 1 if just pulled OFF, 0 if backed ON
	//Set bits 7...0 to the routing entity
	//Return the routing entity of the (first) switch that has changed.  
	bool changed = false;  // true if anything disagrees with entitySwitchValues[]
	bool allOn = true;   // false if any lever at all is pulled OFF
	bool thisPin;
	unsigned int justChanged = 0;

 
	for (int entityCounter = 0; entityCounter < MAX_ROUTING_ENTITIES; entityCounter++) {
		//check all routing entities
		byte switchLine;
		if (isWest()) {
//			if (westNet[entityCounter].aLeadsTo == 0) break;  //end of list
			switchLine = westNet[entityCounter].switchLine;
		}
		else {
//			if (eastNet[entityCounter].aLeadsTo == 0) break;  //end of list
			switchLine = eastNet[entityCounter].switchLine;
		}

		if (switchLine != 0xFF) {   //ignore all routing elements that do not have a valid switch line
	
			switchLine &= 0x7F;  //remove west-facing info
			thisPin = CS.digitalRead(switchLine);  //=1 if backed ON, 0 if pulled OFF
			if (!thisPin) {
				allOn = false;	//at least one lever is pulled OFF
			}
			byte byteSelect = entityCounter / 16;
			byte bitSelect = entityCounter % 16;
			if (thisPin != bitRead(entitySwitchValues[byteSelect], bitSelect)) {	//something has changed
				if (!changed) {		//this is the first change spotted
					changed = true;
					justChanged = entityCounter;
					bitSet(justChanged, 15);	//set MSB to flag change
					if (thisPin) {
						bitSet(justChanged, 13);  //set B13 to indicate value
					}
					bitWrite(entitySwitchValues[byteSelect], bitSelect, thisPin);
					if (DEBUG) {
						Serial.print("RE=");
						Serial.print(entityCounter);
						Serial.print(" SWL=");
						Serial.print(switchLine);
						Serial.print(" VAL=");
						Serial.println(thisPin); 
					}

				}
			}
		}
	}

	if (allOn && changed) {	//all levers are backed OFF, and this represents a change
		bitSet(justChanged, 14);
	}

  if ((DEBUG) && (justChanged !=0)) {
    Serial.print("detectSwitches(");
    Serial.print(justChanged,HEX);
    Serial.println(")");
  }


	return(justChanged);
}


bool stopEntity(byte routingEntity) {
	//Return true if this entity is a Stop Section with Red/Green LED

	//List all the routing entities that need to have their sense inverted
	byte eastInvert[] = {
		6, 7, 41, 42, 43, 255
	};
	byte westInvert[] = {
		6, 7, 8, 64, 67, 74, 255
	};

	bool invert = false;
	byte checkInvert = 0;

	//Find out if the LED is in the appropriate Invert list
	if (isWest()) {
		while (westInvert[checkInvert] != 255) {
			if (westInvert[checkInvert] == routingEntity) {
				invert = true;
			}
			checkInvert++;
		}
	}
	else {  //East
		while (eastInvert[checkInvert] != 255) {
			if (eastInvert[checkInvert] == routingEntity) {
				invert = true;
			}
			checkInvert++;
		}
	}
	return(invert);
}



void setLed(byte routingEntity, bool whiteGreen, bool onOff, bool override) {
	//turn a specified routing entity's white/green or red LED on (if onOff=TRUE) or Off
	
	byte ledNo;
  bool thisLed;

	if (routingEntity < MAX_ROUTING_ENTITIES) {	//if beyond end of list, ignore
		if (isWest()) {
			if (whiteGreen) {
				ledNo = westNet[routingEntity].whiteLed;  //white or green
			}
			else {
				ledNo = westNet[routingEntity].redLed;
			}
		}
		else {
			if (whiteGreen) {
				ledNo = eastNet[routingEntity].whiteLed;  //white or green
			}
			else {
				ledNo = eastNet[routingEntity].redLed;
			}
		}

		if (ledNo != 255) {
      thisLed = onOff;
      if (stopEntity(routingEntity)) {
        thisLed = !thisLed;   //invert for stop sections
      }

      if (DEBUG) {
        if (whiteGreen) {
          Serial.print(" W");
        }
        else {
          Serial.print(" R");
        }
        Serial.print(ledNo);
        if (onOff) {
          Serial.println("=1");
        }
        else {
          Serial.println("=0");
        }
      }
      
	  //now actually do it
	  if (ledNo >= 194) {		//on the Arduino itself
		  digitalWrite(ledNo - 192, !thisLed);
	  }
	  else {
      CS.digitalWrite(ledNo, !thisLed);		//on a Centipede
	  }

    }
  }
}



void clearLeds() {
	//turn all LEDs off
 
  if (DEBUG) {
    Serial.println("clearLeds()");
  }


	for (int i = 0; i < MAX_ROUTING_ENTITIES; i++) {
		if (stopEntity(i)) {
			bool switchVal = readSwitch(i);
			setLed(i, true, switchVal, false);	//turn green on if pulled, otherwise off
			setLed(i, false, !switchVal, false);   //turn red off if pulled, otherwise on
		}
		else {	//turn both LEDs off
			setLed(i, false, false, true);
			setLed(i, true, false, true);
		}
	}
}



bool readSwitch(byte entityNo) {
	//find the value of the switch associated with this routing entity

	byte thisSwitch;
	bool switchVal;
	if (isWest()){
		thisSwitch = westNet[entityNo].switchLine;
	}
	else {
		thisSwitch = eastNet[entityNo].switchLine;
	}

	if (thisSwitch == 0xff) {
		switchVal = false;
	}
	else {
		thisSwitch &= 0x7F;
		switchVal = !CS.digitalRead(thisSwitch);  //switchVal = 0 if backed ON, 1 if pulled OFF
	}

/*	if (DEBUG) {
		Serial.print("RE=");
		Serial.print(entityNo);
		Serial.print(" SWL=");
		Serial.print(thisSwitch);
		Serial.print(" VAL=");
		Serial.println(switchVal);
	}
*/
	return (switchVal);
}



bool isWest() {
	//return TRUE if we're operating WEST
	return(!digitalRead(WESTPIN));
}


//================================================================
//                      Timer routines
//================================================================


	/*
	Hardware Timer1 Interrupt
	Flash LED every second
	Code from: http://www.hobbytronics.co.uk/arduino-timer-interrupts
	*/


#define ledPin 13
int timer1_counter;
bool timerExpired;
void timerInit()
{
	pinMode(ledPin, OUTPUT);

	// initialize timer1 
	noInterrupts();           // disable all interrupts
	TCCR1A = 0;
	TCCR1B = 0;

	// Set timer1_counter to the correct value for our interrupt interval
	//timer1_counter = 64911;   // preload timer 65536-16MHz/256/100Hz
	//timer1_counter = 64286;   // preload timer 65536-16MHz/256/50Hz
	timer1_counter = 34286;   // preload timer 65536-16MHz/256/2Hz

	TCNT1 = timer1_counter;   // preload timer
	TCCR1B |= (1 << CS12);    // 256 prescaler 
	TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt

	timerExpired = false;
	interrupts();             // enable all interrupts
}

ISR(TIMER1_OVF_vect)        // interrupt service routine 
{
	TCNT1 = timer1_counter;   // preload timer
	digitalWrite(ledPin, digitalRead(ledPin) ^ 1);
	timerExpired = true;
}



//================================================================
//                      Centipede routines
//================================================================


// Example code for Centipede Library
// Works with Centipede Shield or MCP23017 on Arduino I2C port

/* Available commands
.digitalWrite([0...127], [LOW...HIGH]) - Acts like normal digitalWrite
.digitalRead([0...127]) - Acts like normal digitalRead
.pinMode([0...127], [INPUT...OUTPUT]) - Acts like normal pinMode
.portWrite([0...7], [0...65535]) - Writes 16-bit value to one port (chip)
.portRead([0...7]) - Reads 16-bit value from one port (chip)
.portMode([0...7], [0...65535]) - Write I/O mask to one port (chip)
.pinPullup([0...127], [LOW...HIGH]) - Sets pullup on input pin
.portPullup([0...7], [0...65535]) - Sets pullups on one port (chip)
.portInterrupts([0...7],[0...65535],[0...65535],[0...65535]) - Configure interrupts
[device number],[use interrupt on pin],[default value],[interrupt when != default]
.portCaptureRead(0...7) - Reads 16-bit value registers as they were when interrupt occurred
.init() - Sets all registers to initial values

Examples
CS.init();
CS.pinMode(0,OUTPUT);
CS.digitalWrite(0, HIGH);
int recpin = CS.digitalRead(0);
CS.portMode(0, 0b0111111001111110); // 0 = output, 1 = input
CS.portWrite(0, 0b1000000110000001); // 0 = LOW, 1 = HIGH
int recport = CS.portRead(0);
CS.pinPullup(1,HIGH);
CS.portPullup(0, 0b0111111001111110); // 0 = no pullup, 1 = pullup
CS.portInterrupts(0,0b0000000000000001,0b0000000000000000,0b0000000000000001);

*/
