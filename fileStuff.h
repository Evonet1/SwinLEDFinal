
// fileStuff.h
//
// Don't really need inclusion guards here, but it's not
// a bad habit to get into
//
// Declare external variables and give function prototypes here
// for stuff to be included from your main sketch (or from
// anywhere else in your project
//
// davekw7x
//
#ifndef FILESTUFF_H__
#define FILESTUFF_H_

#include "Arduino.h"

struct netList {
	byte aLeadsTo;  //no of entity that A goes to  - add 128 if point is leading when travelling westwards
	byte bLeadsTo;  //no of entity that B goes to, connect to A if not operated
	byte cLeadsTo;  //no of entity that C goes to, connect to A if operated (set to 0xFF if a track)
	byte switchLine;	//signal line to interrogate switch
	byte whiteLed;  //white LED associated with this entity
	byte redLed;  //red LED associated with this entity
};

extern netList westNet[];	//changed from extern
extern netList eastNet[];

#endif
