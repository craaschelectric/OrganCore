// TempSensor.h
// Reads pipe-chamber temperature as ASCII degrees-C (one decimal, newline-
// terminated) from the shared Serial8 Rx, converts to a pitch offset in cents
// relative to TEMP_REFERENCE_DEGC (~CENTS_PER_DEGREE per degree), and notifies
// PitchManager when the offset changes.
//
// Serial8 is shared with the pipe-mirror MIDI out. The sketch begins the port
// once (at PIPE_TEMP_SERIAL_BAUD) and passes the same reference to both
// midiOutAttach() and tempSensorAttach(); tempSensorAttach() only stores the
// pointer, it does not begin the port.

#ifndef TEMPSENSOR_H
#define TEMPSENSOR_H

#include <Arduino.h>

void  tempSensorAttach(HardwareSerial& port);   // shared, already begun by sketch
void  tempSensorPoll();                          // call from loop(); self-timed by input

int   getTempOffsetCents();      // cents relative to TEMP_REFERENCE_DEGC
float getTempDegC();             // current temperature for display

#endif // TEMPSENSOR_H
