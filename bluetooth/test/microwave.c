#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main (int argc, char *argv[])
{
	int trig = 2 ;
	int echo = 8 ;
	int start_time, end_time ;
	int distance ;

	if (wiringPiSetup() == -1)
	{
		printf ("[%s:%d] fail to call wiringPiSetup function: %s \n", __func__, __LINE__, strerror(errno));
		exit(1) ;
	}

	pinMode(trig, OUTPUT);
	pinMode(echo , INPUT);

	while (1)
	{
		digitalWrite (trig, LOW);  // Init Trigger
		delay (1000);	// 1 sec
		digitalWrite(trig, HIGH) ;	// send signal
		delayMicroseconds(10) ;		// 10 * 0.000001 sec
		digitalWrite(trig, LOW) ;	// receive signal

		while (digitalRead(echo) == 0) ;	
		start_time = micros() ;	// get microseconds after program runs
		
		while (digitalRead(echo) == 1) ;
		end_time = micros() ;

		distance = (end_time - start_time) / 29 / 2 ;	// Ultrasound is 1cm per 29 microsecond, 2 is roundtrip.
		printf("distance %d cm\n", distance) ;
	}
}
