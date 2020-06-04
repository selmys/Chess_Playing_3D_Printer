#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

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

void doMove(char *move) {
	int fc,fr,tc,tr;
	fc = move[0] - 'a';
	fr = move[1] - '1';
	tc = move[2] - 'a';
	tr = move[3] - '1';
	
	fr = 7-fr;
	tr = 7-tr;
	
	board[tr][tc] = board[fr][fc];
	board[fr][fc] = '-';
}

int main() {
	char move[6];
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
	printf("Enter move: ");
	scanf("%s",move);
    	if(!strcmp(move,"quit")) {
		move[4]='\n';
		move[5]='\0';
		write(pipe1[1],move,6);
        	move[4]='\0';
    	}
	while (strcmp(move,"quit")) {
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
