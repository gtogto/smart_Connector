#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include <time.h>
#include <pthread.h>

/* baudrate settings are defined in <asm/termbits.h>, which is
included by <termios.h> */
#define BAUDRATE B115200            
/* change this definition for the correct port */
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define PORT 5001	/* UDP port */

#define FALSE 0
#define TRUE 1

/* receive for serial data */
#define START_CODE '['
#define END_CODE ']'

#define START 0
#define LENGTH1 1
#define LENGTH2 2
#define LENGTH3 3
#define LENGTH4 4
#define LENGTH5 5
#define PAYLOAD 5
#define END 7

#define RECV_1 '('
#define RECV_2 ')'

#define LORA_START '<'
#define LORA_END   '>'
#define DEVICE_START '('
#define DEVICE_END   ')'

#define SC_KEEPALIVE_ID		'K'
#define SC_ERASE_TAG		'E'
#define SC_UPLOAD_TAG		'U'
#define SC_CHANGE_PERIOD_TAG	'W'
#define SC_DOWNLOAD_TAG		'P'

#define TAG_WAKEUP_SC		'B'
#define TAG_UPLOAD_SC		'P'
#define MCU_DINPUT_SC		'I'
#define SC_DOUTPUT_MCU		'O'

#define BUF_LEN 1024

volatile int STOP=FALSE; 
int hex_to_ascii(char c, char d);
int hex_to_int(char c);


void main(int argc, char *argv[])
{
	/* UART */
	int fd, res, ret;
	//char debug_rxd;
	struct termios oldtio,newtio;
	char buf[1024];
	int i ;
	//unsigned char test[9] = {0x5B, 0x00, 0x00, 0x00, 0x06, 0x01, 0x02, 0x00, 0x5D};

	/* TCP Socket */
	int s, n;
        char *haddr;
        struct sockaddr_in server_addr;
        //struct sockaddr_in server_addr : 서버의 소켓주소 구조체
        char bufT[BUF_LEN+1];	

	/* Compaire to tcp and uart length */
	char recv_copy_buf[1024];
	int pkt_len;
	char start_p = '[';
	char end_p = ']';

	/*UART function*/
	char uart_rxd;
	char uart_rx_state=START;
	char uart_buf[1024];
	int SPAS_length;
	int uart_rx_cnt;
	int mcu_rx_length ;
	int flag_mcu_rx_ok = 0  ;
	
	// 20200920_
	int flag;
	int k;

	fd_set fs_status;

	// 20200924_ gto
	char uart_send_buf[1024] = {0,};
	char *bye = "bye";
	
	char boot_1 = '(';
	char boot_2 = 'K';
	char boot_3 = '1';
	char boot_4 = ')';

	int rdcnt;

        if((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        {//소켓 생성과 동시에 소켓 생성 유효검사
		printf("can't create socket\n");
		exit(0);
        }
        bzero((char *)&server_addr, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(argv[1]);
        server_addr.sin_port = htons(PORT);
        if(connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)        {
                printf("can't connect.\n");
                exit(0);
        }
	else{
		printf("TCP Connect OK\r\n");
	}

	fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK ); 	// O_NONBLOCK
	if (fd <0) {perror(MODEMDEVICE); printf("uart open error\n");  exit(-1);}
	else{
		printf("UART Connect OK\r\n");
	}
	tcgetattr(fd,&oldtio); /* save current serial port settings */
	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */        
	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR | ICRNL;
	newtio.c_oflag = 0;
	newtio.c_lflag = 0; 	 // ICANON; or 1
	newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
	newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtio.c_cc[VERASE]   = 0;     /* del */
	newtio.c_cc[VKILL]    = 0;     /* @ */
	newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
	newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtio.c_cc[VMIN]     = 0;     /* blocking read until 1 character arrives */
	newtio.c_cc[VSWTC]    = 0;     /* '\0' */
	newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
	newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtio.c_cc[VEOL]     = 0;     /* '\0' */
	newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtio.c_cc[VEOL2]    = 0;     /* '\0' */
	tcflush(fd, TCIFLUSH);
        tcsetattr(fd,TCSANOW,&newtio);

	// 20200920_
	flag = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flag | O_NONBLOCK);	// O_NONBLOCK 


	write(fd, &start_p, 1);	write(fd, &boot_1, 1);	write(fd, &boot_2, 1);	write(fd, &boot_3, 1);	write(fd, &boot_4, 1);	write(fd, &end_p, 1);
	printf(" while start ...\r\n") ;
	while(1){

		if((n=read(s,bufT,BUF_LEN)) > 0)
	        {
			//서버가 보내오는 daytime 데이터의 수신
        		bufT[n] = NULL;
			pkt_len = ((int)bufT[0] << 24) | ((int)bufT[1] << 16) | ((int)bufT[2] << 8) | ((int)bufT[3]);
			/*
			if((pkt_len+1) == n)
			{
				write(fd,&start_p,1);
				write(fd,bufT,n);
				write(fd,&end_p,1);
			}*/
			printf("TCP receive: %s", bufT);
			//write(s, n, strlen(bufT)+1);
			//write(s,(rxd1_buf+1),(ch1_cnt-2));
			//write(fd, bufT, n);

		}

		/* Serial receive function */
		if ((n=read(fd, &uart_rxd, 1))>0) {
			switch( uart_rx_state ){
				case START :
					if( uart_rxd == START_CODE ){
						uart_rx_state =  PAYLOAD ;
						uart_rx_cnt = 0 ;
					}
					break ;
				case PAYLOAD :
					uart_buf[ uart_rx_cnt++] = uart_rxd ;
					if( uart_rxd == START_CODE ){
						uart_rx_state = PAYLOAD ;
						uart_rx_cnt = 0 ;
					}
					else if( uart_rxd == END_CODE ){
						flag_mcu_rx_ok = 1 ;
						mcu_rx_length = uart_rx_cnt-1 ;
						uart_rx_state = START ;
						//for( k=0;k<mcu_rx_length;k++)printf("%c",uart_buf[k]) ;
                                                //printf("Received OK....\n") ;
					}
					if( uart_rx_cnt > 254 ) {
						uart_rx_state = START ;
						uart_rx_cnt = 0 ;
					}
					//write(s,(rxd1_buf+1),(ch1_cnt-2));
					break ;
			}
			//printf("%s\r\n", rxd1_buf);
		}
		if( flag_mcu_rx_ok ){
			flag_mcu_rx_ok = 0 ;
			if(  (uart_buf[0] == DEVICE_START ) && (uart_buf[ mcu_rx_length - 1 ] == DEVICE_END )){
				if( uart_buf[1] == SC_KEEPALIVE_ID ){ //send keep alive  message to SPAS  
					printf("Keepalive message trigger Ok... \r\n") ;
				}
				else if( uart_buf[1] == MCU_DINPUT_SC ){
					printf("Data input report OK....\r\n");
				}
			}
			if(  (uart_buf[0] == LORA_START ) && (uart_buf[ mcu_rx_length - 1 ] == LORA_END )){
                                if( uart_buf[13] == TAG_WAKEUP_SC ){ //send keep alive  message to SPAS
                                        printf("TAG Wakeup  message  Ok... \r\n") ;
                                }
                                else if( uart_buf[13] == TAG_UPLOAD_SC ){
                                        printf("TAG_UPload report OK....\r\n");
                                }
                        }
		}
	}
	
	tcsetattr(fd,TCSANOW,&oldtio);
	close(s);
	//return 0;
}

/* Convert function */
int hex_to_int(char c){ 
	int first = c/16 - 3; 
	int second = c % 16; 
	int result = first*10 + second; 
	if(result > 9) result--; 
	return result; 
} 

int hex_to_ascii(char c, char d){ 
	int high = hex_to_int(c) * 16; 
	int low = hex_to_int(d); 
	return high+low; 
} 
