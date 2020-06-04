//
// gcc chess3.cpp -o chess3 -lwiringPi
//
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define ADAFRUIT_MOTORHAT       0x60

#define PWM_M1_PWM      8
#define PWM_M1_IN2      9
#define PWM_M1_IN1      10
#define PWM_M2_PWM      13
#define PWM_M2_IN2      12
#define PWM_M2_IN1      11
#define PWM_M3_PWM      2
#define PWM_M3_IN2      3
#define PWM_M3_IN1      4
#define PWM_M4_PWM      7
#define PWM_M4_IN2      6
#define PWM_M4_IN1      5

#define PWM_FREQUENCY   1600.0
#define PWM_PRESCALE    0xFE

#define PWM_MODE1       0x00
#define PWM_MODE2       0x01
#define PWM_LED0_ON_L   0x06
#define PWM_LED0_ON_H   0x07
#define PWM_LED0_OFF_L  0x08
#define PWM_LED0_OFF_H  0x09

#define PWM_RESTART     0x80
#define PWM_SLEEP       0x10
#define PWM_ALLCALL     0x01
#define PWM_INVRT       0x10
#define PWM_OUTDRV      0x04

#define PWM_ALL_LED_ON_L        0xFA
#define PWM_ALL_LED_ON_H        0xFB
#define PWM_ALL_LED_OFF_L       0xFC
#define PWM_ALL_LED_OFF_H       0xFD

#define MOTOR_FORWARD   1
#define MOTOR_BACKWARD	2
#define MOTOR_BRAKE     3
#define MOTOR_RELEASE   4

#define STEPS_PER_REV	200
#define MICROSTEPS	8
#define SINGLE		1
#define DOUBLE		2
#define INTERLEAVE	3
#define MICROSTEP	4

#define LIMIT_SWITCH_Z 	23
#define LIMIT_SWITCH_Y	17
#define LIMIT_SWITCH_X	27
#define LIMIT_SWITCH_X1	5
#define MAGNET 22
#define XSTEP 130
#define YSTEP 130

typedef unsigned short word;

class Adafruit_MotorHAT {
	
	word i2caddr, i2c;
	word in1[4]={99,99,99,99}, in2[4]={99,99,99,99}, pwm[4]={99,99,99,99};
	word revsteps[2], currentstep[2], steppingcounter[2];
	word Microstep_Curve[9]={0, 50, 98, 142, 180, 212, 236, 250, 255};
	double sec_per_step[2];
	
	public:
	static word Hat_Addr[4];
	Adafruit_MotorHAT(word addr=ADAFRUIT_MOTORHAT) {
		
		for(int i=0;i<4;i++) {
			if(Hat_Addr[i] == addr) {
				printf("Moror Hat %x already in use!\n", addr);
				exit(3);
			}
		}
		int j=99;
		for(int i=0;i<4;i++) {
			if(Hat_Addr[i] == 0) j=i;
		}
		
		if(j == 99) {
			printf("No room for more Hats (max 4 allowed)!\n");
			exit(4);
		} else {
			Hat_Addr[j] = addr;
		}
				
		i2caddr=addr;
		
	        // Setup I2C
	        i2c = wiringPiI2CSetup(i2caddr);
		printf("Motor Hat %x created!\n",addr);
		
	        // Setup PWM
	        setAllPWM(i2c, 0, 0);
	        wiringPiI2CWriteReg8(i2c, PWM_MODE2, PWM_OUTDRV);
	        wiringPiI2CWriteReg8(i2c, PWM_MODE1, PWM_ALLCALL);
	        delay(5);
	        word mode1 = wiringPiI2CReadReg8(i2c, PWM_MODE1) & ~PWM_SLEEP;
	        wiringPiI2CWriteReg8(i2c, PWM_MODE1, mode1);
	        delay(5);
	
	        // Set PWM frequency
	        word prescale = (int)(ceil(25000000.0 / 4096.0 / PWM_FREQUENCY - 1.0));
	        printf("prescale value is %d\n", prescale);
	        word oldmode = wiringPiI2CReadReg8(i2c, PWM_MODE1);
	        word newmode = oldmode & 0x7F | 0x10;
	        wiringPiI2CWriteReg8(i2c, PWM_MODE1, newmode);
	        wiringPiI2CWriteReg8(i2c, PWM_PRESCALE, prescale);
	        wiringPiI2CWriteReg8(i2c, PWM_MODE1, oldmode);
	        delay(5);
	        wiringPiI2CWriteReg8(i2c, PWM_MODE1, oldmode | 0x80);
	};
	
	word getAddr() {
		return i2caddr;
	};
	
	void setAllPWM(word i2c, word on, word off){
		// Sets all PWM channels
	        wiringPiI2CWriteReg8(i2c, PWM_ALL_LED_ON_L, on & 0xFF);
	        wiringPiI2CWriteReg8(i2c, PWM_ALL_LED_ON_H, on >> 8);
	        wiringPiI2CWriteReg8(i2c, PWM_ALL_LED_OFF_L, off & 0xFF);
	        wiringPiI2CWriteReg8(i2c, PWM_ALL_LED_OFF_H, off >> 8);
	};
	
	void setPWM(word i2c, word channel, word on, word off){
		// Sets a single PWM channel
	        wiringPiI2CWriteReg8(i2c, PWM_LED0_ON_L + 4 * channel, on & 0xFF);
	        wiringPiI2CWriteReg8(i2c, PWM_LED0_ON_H + 4 * channel, on >> 8);
	        wiringPiI2CWriteReg8(i2c, PWM_LED0_OFF_L + 4 * channel, off & 0xFF);
	        wiringPiI2CWriteReg8(i2c, PWM_LED0_OFF_H + 4 * channel, off >> 8);
	};
	
	void setPin(word i2c, word pin, word value){
	        if(pin < 0 || pin > 15){
	                printf("PWM pin must be between 0 and 15 inclusive.  Received '%d'\n", pin);
	                return;
	        }
	
	        switch(value){
	                case 0:
	                        setPWM(i2c, pin, 0, 4096);
	                        break;
	                case 1:
	                        setPWM(i2c, pin, 4096, 0);
	                        break;
	                default:
	                        printf("PWM pin value must be 0 or 1.  Received '%d'\n", pin);
	                        return;
	        }
	};
	
	void addStepperMotor(word motor, word steps=STEPS_PER_REV) {
		if(in1[motor-1] != 99 && in1[motor] != 99) {
			printf("Stepper Motor %d already added!\n", motor);
			exit(1);
		}
		printf("Creating Stepper #%d\n", motor);
		revsteps[motor-1] = steps;
		sec_per_step[motor-1] = 0.1;
		steppingcounter[motor-1] = 0;
		currentstep[motor-1] = 0;
		switch(motor){
			case 1:
				in1[0] = PWM_M1_IN1;
				in2[0] = PWM_M1_IN2;
				in1[1] = PWM_M2_IN1;
				in2[1] = PWM_M2_IN2;
				pwm[0] = PWM_M1_PWM;
				pwm[1] = PWM_M2_PWM;
				break;
			case 2:
				in1[2] = PWM_M3_IN1;
				in2[2] = PWM_M3_IN2;
				in1[3] = PWM_M4_IN1;
				in2[3] = PWM_M4_IN2;
				pwm[2] = PWM_M3_PWM;
				pwm[3] = PWM_M4_PWM;
				break;
			default:
	                        printf("Invalid stepper motor number '%d'\n", motor);
	                        return;
		}
		printf("Created Stepper #%d\n", motor);
	};
	
	void setStepperSpeed(word motor, word rpm) {
		printf("Setting Stepper speed to %d rpm\n", rpm);
		switch (motor) {
			case 1:
				sec_per_step[0] = 60.0 / (revsteps[0] * rpm);
				steppingcounter[0] = 0;
				break;
			case 2:
				sec_per_step[1] = 60.0 / (revsteps[1] * rpm);
				steppingcounter[1] = 0;
				break;
			default:
	                        printf("Unsupported stepper motor '%s'\n", motor);
	                        break;
		}
	};
	
	void releaseStepper(word motor) {
		printf("Clearing stepper pins\n");
		switch (motor) {
			case 1:
				setPin(i2c, in1[motor-1], 0);
				setPin(i2c, in1[motor],   0);
				setPin(i2c, in2[motor-1], 0);
				setPin(i2c, in2[motor],   0);
				break;
			case 2:
				setPin(i2c, in1[motor], 0);
				setPin(i2c, in1[motor+1], 0);
				setPin(i2c, in2[motor],0);
				setPin(i2c, in2[motor+1], 0);
				break;
			default:
	                        printf("Unsupported stepper motor '%s'\n", motor);
	                        break;
		}
	};
	
	word oneStep(word motor, word dir, word style) {
		word pwm_a = 255;
		word pwm_b = 255;
		// printf("currentstep[%d] = %d\n", motor-1, currentstep[motor-1]);
		// first determine what sort of stepping procedure we're up to
		switch (style) {
			case SINGLE:
				if((currentstep[motor-1]/(MICROSTEPS/2)) % 2)
					// we're at an odd step, weird
					if(dir == MOTOR_FORWARD)
						currentstep[motor-1] += MICROSTEPS/2;
					else
						currentstep[motor-1] -= MICROSTEPS/2;
				else
					// go to next even step
					if(dir == MOTOR_FORWARD)
						currentstep[motor-1] += MICROSTEPS;
					else
						currentstep[motor-1] -= MICROSTEPS;
				break;
			case DOUBLE:
				if(!(currentstep[motor-1]/(MICROSTEPS/2)) % 2)
					// we're at an even step, weird
					if(dir == MOTOR_FORWARD)
						currentstep[motor-1] += MICROSTEPS/2;
					else
						currentstep[motor-1] -= MICROSTEPS/2;
				else
					// go to next odd step
					if(dir == MOTOR_FORWARD)
						currentstep[motor-1] += MICROSTEPS;
					else
						currentstep[motor-1] -= MICROSTEPS;
				break;
			case INTERLEAVE:
				if(dir == MOTOR_FORWARD)
					currentstep[motor-1] += MICROSTEPS/2;
				else
					currentstep[motor-1] -= MICROSTEPS/2;
				break;
			case MICROSTEP:
				if(dir == MOTOR_FORWARD)
					currentstep[motor-1] += 1;
				else
					currentstep[motor-1] -= 1;
				// go to next 'step' and wrap around
				currentstep[motor-1] += MICROSTEPS * 4;
				currentstep[motor-1] %= MICROSTEPS * 4;
				pwm_a = 0;
				pwm_b = 0;
				if(currentstep[motor-1] >= 0 && (currentstep[motor-1] < MICROSTEPS)) {
					pwm_a = Microstep_Curve[MICROSTEPS - currentstep[motor-1]];
					pwm_b = Microstep_Curve[currentstep[motor-1]];
					
				}else if(currentstep[motor-1] >= MICROSTEPS && (currentstep[motor-1] < MICROSTEPS*2)) {
					pwm_a = Microstep_Curve[currentstep[motor-1] - MICROSTEPS];
					pwm_b = Microstep_Curve[MICROSTEPS*2 - currentstep[motor-1]];
					
				}else if(currentstep[motor-1] >= MICROSTEPS*2 && (currentstep[motor-1] < MICROSTEPS*3)) {
					pwm_a = Microstep_Curve[MICROSTEPS*3 - currentstep[motor-1]];
					pwm_b = Microstep_Curve[currentstep[motor-1] - MICROSTEPS*2];
					
				}else if(currentstep[motor-1] >= MICROSTEPS*3 && (currentstep[motor-1] < MICROSTEPS*4)) {
					pwm_a = Microstep_Curve[currentstep[motor-1] - MICROSTEPS*3];
					pwm_b = Microstep_Curve[MICROSTEPS*4 - currentstep[motor-1]];
				}	
				break;
			default:
				printf("Unsupported stepper style '%s'\n", style);
				exit(2);
	                        break;
		}
		// printf("pwm_a %d pwm_b %d\n",pwm_a, pwm_b);
		// go to next 'step' and wrap around
		currentstep[motor-1] += MICROSTEPS * 4;
		currentstep[motor-1] %= MICROSTEPS * 4;
		
		// only really used for microstepping, otherwise always on!
		switch (motor) {
			case 1:
				setPWM(i2c, pwm[motor-1], 0, pwm_a*16);
				setPWM(i2c, pwm[motor],   0, pwm_b*16);
				break;
 			case 2:
                                setPWM(i2c, pwm[motor],     0, pwm_a*16);
                                setPWM(i2c, pwm[motor+1],   0, pwm_b*16);
                                break;
		}		
		// set up coil energizing!
		word coils[4] = {0,0,0,0};
		
					 
		if(style == MICROSTEP) {
			if(currentstep[motor-1] >= 0 && (currentstep[motor-1] < MICROSTEPS)) { 
				coils[0]=coils[1]=1;
			}else if(currentstep[motor-1] >= MICROSTEPS && (currentstep[motor-1] < MICROSTEPS*2)){
				coils[1]=coils[2]=1;
			}else if(currentstep[motor-1] >= MICROSTEPS*2 && (currentstep[motor-1] < MICROSTEPS*3)){
				coils[2]=coils[3]=1;
			}else if(currentstep[motor-1] >= MICROSTEPS*3 && (currentstep[motor-1] < MICROSTEPS*4)){
				coils[0]=coils[3]=1;
			}
		}else {
			word step2coils[8][4] = { {1,0,0,0},
						  {1,1,0,0},
						  {0,1,0,0},
						  {0,1,1,0},
						  {0,0,1,0},
						  {0,0,1,1},
						  {0,0,0,1},
						  {1,0,0,1} };
			
			word i = currentstep[motor-1]/(MICROSTEPS/2);
			coils[0] = step2coils[i][0];
			coils[1] = step2coils[i][1];
			coils[2] = step2coils[i][2];
			coils[3] = step2coils[i][3];
		}
		
		
		//printf("coils state = [%d,%d,%d,%d]\n", coils[0],coils[1],coils[2],coils[3]);
		switch (motor) {
			case 1:
				setPin(i2c, in1[motor-1], coils[1]);
				setPin(i2c, in1[motor],   coils[2]);
				setPin(i2c, in2[motor-1], coils[3]);
				setPin(i2c, in2[motor],   coils[0]);
				break;
			case 2:
				setPin(i2c, in1[motor],     coils[1]);
				setPin(i2c, in1[motor+1],   coils[2]);
				setPin(i2c, in2[motor],     coils[3]);
				setPin(i2c, in2[motor+1],   coils[0]);
				break;
		}

		
		//printf("lateststep is %d\n",currentstep[motor-1]);
		return currentstep[motor-1];
	};
	
	void step(word motor, word steps, word dir, word style) {
		double s_per_s = sec_per_step[motor-1];
		word lateststep = 0;
		
		if(style == INTERLEAVE)
			s_per_s = s_per_s/2.0;
		if(style == MICROSTEP) {
			s_per_s /= MICROSTEPS;
			steps = steps * MICROSTEPS;
		}
		
		printf("Seconds per step is %g\n", s_per_s);
		printf("Number of steps is %d\n", steps);
		
		for(int i=0; i<steps; i++) {
			//printf("Step #%d\n",i);
			lateststep = oneStep(motor, dir, style);
			usleep(s_per_s * 1000000);
		}
		
		if(style == MICROSTEP) {
			// this is an edge case, if we are in between full steps, lets just keep going
			// so we end on a full step
			while ((lateststep != 0) && (lateststep != MICROSTEPS)) {
				lateststep = oneStep(motor, dir, style);
				usleep(s_per_s * 1000000);
			}
		}
	};
};

Adafruit_MotorHAT hat;	
Adafruit_MotorHAT hat1(0x61);	
word Adafruit_MotorHAT::Hat_Addr[4] = {0,0,0,0};
	
void setUp() {
	hat.addStepperMotor(2,200);
	hat.addStepperMotor(1,200);
	hat1.addStepperMotor(1,200);

	wiringPiSetupGpio ();

	pinMode (MAGNET, OUTPUT);
  	pinMode (LIMIT_SWITCH_Y, INPUT);
  	pinMode (LIMIT_SWITCH_X, INPUT);
	pinMode (LIMIT_SWITCH_Z, INPUT);
	pinMode (LIMIT_SWITCH_X1, INPUT);
}

void goHome() {
	word speed=120;
	word steps=20;
	hat.setStepperSpeed(1,speed);
	hat.setStepperSpeed(2,speed);
	hat1.setStepperSpeed(1,speed);
	while (digitalRead(LIMIT_SWITCH_Z)) {
		hat1.step(1,steps,MOTOR_FORWARD,DOUBLE); //up
		delay(5);
	}
	while (!digitalRead(LIMIT_SWITCH_Y)) {
		hat.step(2,steps,MOTOR_BACKWARD,DOUBLE); //backward
		delay(5);
	}
	while (!digitalRead(LIMIT_SWITCH_X)) {
		hat.step(1,steps,MOTOR_BACKWARD,DOUBLE); //right
		delay(5);
	}
}

void shutDown() {
	hat.releaseStepper(2);
	hat.releaseStepper(1);
	hat1.releaseStepper(1);
}

char board[8][8] = { {'r','n','b','q','k','b','n','r'},
		     {'p','p','p','p','p','p','p','p'},
		     {'-','-','-','-','-','-','-','-'},
		     {'-','-','-','-','-','-','-','-'},
		     {'-','-','-','-','-','-','-','-'},
		     {'-','-','-','-','-','-','-','-'},
		     {'P','P','P','P','P','P','P','P'},
		     {'R','N','B','Q','K','B','N','R'} };

void display() {
	int i,j;
	for(int i=0;i<8;i++) {
		printf("\e[31m%d\e[0m ",8-i);
		for(j=0;j<8;j++) {
			if(board[i][j] >= 'a' && board[i][j] <= 'z')
				printf("\e[33m%c \e[0m",board[i][j]);
			else
				printf("%c ",board[i][j]);
		}
		printf("\n");
	}
	printf("\e[31m  a b c d e f g h\e[0m\n");
}

void energizeMagnet() {
	digitalWrite (22, HIGH); 
}

void deEnergizeMagnet() {
	digitalWrite (22, LOW); 
}

void move2row(int r) {
	// we start at row 8
	// we want to move to row r
	// every row takes x motor revolutions
	int x = 128;
	int rows2skip = r;
	int total_revs = x * rows2skip;
	hat.step(2,total_revs,MOTOR_FORWARD,DOUBLE);
}

void move2col(int c) {
	// we start at column 8
	// we want to move to column c
	// every column take x motor revolutions
	int x = 125;
	int columns2skip = 7-c;
	int total_revs = x * columns2skip;
	hat.step(1,total_revs,MOTOR_FORWARD,DOUBLE);
}

void dropRod(char piece){
	int distance=0;
	switch (piece) {
		case 'P': distance = 2150;
				  break;
		case 'p': distance = 2150;
				  break;		  
		case 'R': distance = 2050;
				  break;
		case 'r': distance = 2050;
				  break;		  
		case 'N': distance = 1960;
				  break;
		case 'n': distance = 1960;
				  break;		  
		case 'B': distance = 1750;
				  break;
		case 'b': distance = 1750;
				  break;		  
		case 'Q': distance = 1650;
				  break;
		case 'q': distance = 1650;
				  break;		  
		case 'K': distance = 1500;
				  break;
		case 'k': distance = 1500;
				  break;		  
		default:  printf("Error\n");
				  shutDown();
				  exit(1);
	}
	hat1.setStepperSpeed(1,120);
	hat1.step(1,distance,MOTOR_BACKWARD,DOUBLE);
}


void doMove(char *move) {
	int fc,fr,tc,tr;
	fc = move[0] - 'a';
	fr = move[1] - '1';
	tc = move[2] - 'a';
	tr = move[3] - '1';
	
	fr = 7-fr;
	tr = 7-tr;
	
	if(board[tr][tc] != '-') {
		// remove captured piece first
		// we always start at home position
		move2row(tr);
		move2col(tc);
		dropRod(board[tr][tc]);
		energizeMagnet();
		// Lift the piece
		while (digitalRead(LIMIT_SWITCH_Z)) {
			hat1.step(1,10,MOTOR_FORWARD,DOUBLE);
			delay(10);
		}
		// Move piece off the board
		while (!digitalRead(LIMIT_SWITCH_X1)) {
			hat.step(1,10,MOTOR_FORWARD,DOUBLE);
			delay(10);
		}
		// Lower rod and drop piece
		dropRod(board[tr][tc]);
		deEnergizeMagnet();
		goHome();
	}
	move2row(fr);
	move2col(fc);
	dropRod(board[fr][fc]);
	energizeMagnet();
	goHome();
	move2row(tr);
	move2col(tc);
	dropRod(board[fr][fc]);
	deEnergizeMagnet();
	goHome();
	board[tr][tc] = board[fr][fc];
	board[fr][fc] = '-';
}

void doCastleWhite(char *move) {
	char m[6];
	if(strcmp(move,"o-o")) {
		strcpy(m,"e1g1");
		doMove(m);
		strcpy(m,"h1f1");
		doMove(m);
	} else {
		strcpy(m,"e1c1");
		doMove(m);
		strcpy(m,"a1d1");
		doMove(m);
	}
}

void doCastleBlack(char *move) {
	char m[6];
	if(strcmp(move,"o-o")) {
		strcpy(m,"e8g8");
		doMove(m);
		strcpy(m,"h8f8");
		doMove(m);
	} else {
		strcpy(m,"e8c8");
		doMove(m);
		strcpy(m,"a8d8");
		doMove(m);
	}
}

int main() {
	char move[7];
	int pipe1[2],pipe2[2];
	char buffer[50], temp[50];
	int pid_child,n,l,pass=0;
	pipe(pipe1);
	for(n=0; n<2; n++){
		// Get previous flags
		int f = fcntl(pipe1[n], F_GETFL, 0);
		// Set bit for non-blocking flag
		f |= O_NONBLOCK;
		// Change flags on fd
		fcntl(pipe1[n], F_SETFL, f);
	}
	pipe(pipe2);
	for(n=0; n<2; n++){
		// Get previous flags
		int f = fcntl(pipe2[n], F_GETFL, 0);
		// Set bit for non-blocking flag
		f |= O_NONBLOCK;
		// Change flags on fd
		fcntl(pipe2[n], F_SETFL, f);
	}
	if(pid_child=fork()) {
		dup2(pipe1[0],0);
		dup2(pipe2[1],1);
		close(pipe1[1]);
		close(pipe2[0]);
		execlp("/usr/games/gnuchess","/usr/games/gnuchess","-xe",NULL);
		exit(0);
	}
	close(pipe1[0]);
	close(pipe2[1]);
	setUp();
	goHome();
	//exit(1);
	display();
	printf("Enter move: ");
	scanf("%s",move);
    	if(!strcmp(move,"quit")) {
		move[4]='\n';
		move[5]='\0';
		write(pipe1[1],move,6);
        	move[4]='\0';
    	}
	while (strcmp(move,"quit")) {
		if(move[0]=='o')
			doCastleWhite(move);
		else
			doMove(move);
		display();
		move[4]='\n';
		move[5]='\0';
		sleep(1);
		write(pipe1[1],move,6);
		if(pass==0) {
			l=0;
			memset(buffer,0,50);
			n=read(pipe2[0],buffer,50);
			if(n>0)l=n;
			while(l<44){
				n=read(pipe2[0],temp,50);
				if(n>0) {
					strncat(buffer,temp,n);
					l=l+n;
				}
				sleep(1);
			}
			pass=1;
			printf("l is %d\n",l);
			printf("BUFFER=%s\n",buffer);
			strncpy(move,buffer+21,4);
		} else {
			l=0;
			memset(buffer,0,50);	
			n=read(pipe2[0],temp,50);
			if(n>0)l=n;
			while(l<38){
				n=read(pipe2[0],temp,50);
				if(n>0) {
					strncat(buffer,temp,n);
					l=l+n;
				}
				sleep(1);
			}
			printf("l is %d\n",l);
			printf("BUFFER=%s\n",buffer);
			strncpy(move,buffer+15,4);
		}
		if(move[0]=='o') 
			doCastleBlack(move);
		else
			doMove(move);
		display();
		printf("Enter move: ");
		scanf("%s",move);
        if(!strcmp(move,"quit")) {
		    move[4]='\n';
		    move[5]='\0';
		    write(pipe1[1],move,6);
            move[4]='\0';
        }
	}
    wait(NULL);
	return 0;
}
