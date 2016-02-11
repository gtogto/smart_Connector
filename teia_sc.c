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

#define BAUDRATE B115200
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1
#define PORT 8011   //5001
#define FALSE 0
#define TRUE 1

/* receive for serial data */
#define START_CODE '['
#define END_CODE ']'
#define START 0
#define PAYLOAD 5
#define END 7
#define LORA_START '<'
#define LORA_END   '>'
#define DEVICE_START '('
#define DEVICE_END   ')'
// SPAS command ID list================
#define SC_KEEPALIVE_ID		'K'
#define SC_ERASE_TAG		'E'
#define SC_UPLOAD_TAG		'U'
#define SC_CHANGE_PERIOD_TAG	'W'
#define SC_DOWNLOAD_TAG		'P'

#define TAG_WAKEUP_SC		'B'
#define TAG_UPLOAD_SC		'P'
#define MCU_DINPUT_SC		'I'
#define SC_DOUTPUT_MCU		'O'
//======================================
#define BUF_LEN 1024

#define KEEPALIVE_CMD		0x81  //  SC ->  SPAS
#define LINEINFO_REQ_CMD	0x8B  //  SC ->  SPAS
#define LINEINFO_ANS_CMD	0x8C  //  SPAS -> SC
#define LSINFO_CMD		0x91  //  SC -> SPAS
#define ALARM_CMD		0x94  //  SPAS -> SC
#define TAGWRITE_CMD		0x97  //  SPAS -> SC    ==  264
#define TAGERASE_CMD		0x97  //  SPAS -> SC    ==   35

#define LINEINFO_ANS_LENGTH	31
#define LINEINFO_REQ_LENGTH	23
#define KEEPALIVE_LENGTH        20
#define LIMITSWITCH_LENGTH	43

#define KEEPALIVE_PERIOD	'9'
#define SC_ID1	'0'
#define SC_ID2  '0'
#define SC_ID3  '1'
//---------------------------------------------------[ length            ]-[ID]-[ block size          ]-[ no block ]-[  ] [   reserved                                    ]  [     data             ]  [ null]
unsigned char lineinfo_req[LINEINFO_REQ_LENGTH+1]= { 0x00, 0x00, 0x00, 23, 0x8B, 0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  SC_ID1, SC_ID2, SC_ID3,   0x00 } ;
unsigned char keepalive_msg[KEEPALIVE_LENGTH+1]  = { 0x00, 0x00, 0x00, 20, 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x31, 0x32, 0x33, 0x35, 0x36,                            0x00 } ;
unsigned char limitsw_msg[LIMITSWITCH_LENGTH+1]  = { 0x00, 0x00, 0x00, 43, 0x91, 0x00, 0x00, 0x00,   23, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, } ;
unsigned char boot_report[6] = {'[','(','K',KEEPALIVE_PERIOD,')',']'} ;
//=====================================================================================================================================================================================
volatile int STOP=FALSE; 
int hex_to_ascii(char c, char d);
int hex_to_int(char c);


unsigned char smartconnectorID[3] ;
unsigned char lineID[4] ;
unsigned char processID[4] ;
unsigned char limitswitch ;
//========================================================================================================================================
//========================================================================================================================================
//========================================================================================================================================
void main(int argc, char *argv[])
{
	/* UART */
	int fd, res, ret;
	//char debug_rxd;
	struct termios oldtio,newtio;
	char buf[1024];
	int i,k ;
	//unsigned char test[9] = {0x5B, 0x00, 0x00, 0x00, 0x06, 0x01, 0x02, 0x00, 0x5D};

	/* TCP Socket */
	int s, tcp_size, flag;
        char *haddr;
        struct sockaddr_in server_addr;
        //struct sockaddr_in server_addr : 서버의 소켓주소 구조체

	/* Compaire to tcp and uart length */
	char recv_copy_buf[1024];
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
	int uart_size ;

	// network communication function
	int flag_spas_erase_cmd = 0 ;
	int flag_spas_upload_cmd = 0 ;
	int flag_spas_period_cmd = 0 ;
	int flag_spas_download_cmd = 0 ;
 	char tcp_buf[BUF_LEN+1];
	int tcp_rx_length ;

	fd_set fs_status;

	// Get time information
	time_t the_time ;
	char time_str[16] ;
	struct tm *tm_ptr ;



	char *erase = "[<ffddeeccbbaaE0>]" ; // 18
	char *upload = "[<ffddeeccbbaaU0>]" ;
	char *period = "[<ffddeeccbbaaW0>]" ;
	char *download =  "[<ffddeeccbbaaP01234567890123456789>]" ; //38

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
	while(1){
        	if(connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)        {
                	printf("can't connect.\n");
                	exit(0);
        	}
		else{
			printf("TCP Connect OK\r\n");
			break ;
		}
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
	flag = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flag | O_NONBLOCK);	// O_NONBLOCK 


	write(s, lineinfo_req,LINEINFO_REQ_LENGTH+1) ; 
	while(1){ 
		if((tcp_size = read(s,tcp_buf,BUF_LEN)) > 0)  {
                        tcp_rx_length  = ((int)tcp_buf[0] << 24) | ((int)tcp_buf[1] << 16) | ((int)tcp_buf[2] << 8) | ((int)tcp_buf[3]);
                        if( (tcp_rx_length == LINEINFO_ANS_LENGTH) && ( tcp_buf[4] == LINEINFO_ANS_CMD)  ){
				smartconnectorID[0] = tcp_buf[20] ; smartconnectorID[1] = tcp_buf[21] ; smartconnectorID[2] = tcp_buf[22] ;
				if( (smartconnectorID[0] == SC_ID1 ) && ( smartconnectorID[1] == SC_ID2) && ( smartconnectorID[2] == SC_ID3 )){
					lineID[0] = tcp_buf[23] ;  lineID[1] = tcp_buf[24] ; lineID[2] = tcp_buf[25] ; lineID[3] = tcp_buf[26] ;
					processID[0] = tcp_buf[27]; processID[1] = tcp_buf[28];processID[2] = tcp_buf[29];processID[3] = tcp_buf[30];
					printf(" Line info answer OK...\r\n") ;
					break ;
				}
                        }
		}
	}
        write(fd, boot_report, 6); 
        printf(" while start ...\r\n") ;


	time(&the_time) ;
	tm_ptr = localtime(&the_time) ;
	sprintf( time_str, "%04d%02d%02d%02d%02d%02d", tm_ptr->tm_year+1900, tm_ptr->tm_mon+1, tm_ptr->tm_mday, tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec) ;
	printf("%s\n",time_str) ;



	while(1){

		if((tcp_size=read(s,tcp_buf,BUF_LEN)) > 0)
	        {
			tcp_rx_length  = ((int)tcp_buf[0] << 24) | ((int)tcp_buf[1] << 16) | ((int)tcp_buf[2] << 8) | ((int)tcp_buf[3]);
			if( tcp_buf[4] == 0 ){ 
			
			}
			/*
			if((pkt_len+1) == n)
			{
				write(fd,&start_p,1);
				write(fd,bufT,n);
				write(fd,&end_p,1);
			}*/
			printf("TCP rx length =  %d \r\n", tcp_rx_length);
			//write(s, n, strlen(bufT)+1);
			//write(s,(rxd1_buf+1),(ch1_cnt-2));
			//write(fd, bufT, n);

		}
		if( flag_spas_erase_cmd ){
			printf(" SPAS Erase comd\r\n" ) ;

		}
		if( flag_spas_upload_cmd ){
			printf(" SPAS Upload cmd \r\n") ;

		}
		if( flag_spas_period_cmd ) {
			printf("SPAS period cmd \r\n") ;

		}
		if( flag_spas_download_cmd){
			printf("SPAS download cmd \r\n") ;

		}

		/* Serial receive function */
		if ((uart_size =read(fd, &uart_rxd, 1))>0) {
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
		}
		if( flag_mcu_rx_ok ){
			flag_mcu_rx_ok = 0 ;
			if(  (uart_buf[0] == DEVICE_START ) && (uart_buf[ mcu_rx_length - 1 ] == DEVICE_END )){
				if( uart_buf[1] == SC_KEEPALIVE_ID ){ //send keep alive  message to SPAS 
					write(s, keepalive_msg, KEEPALIVE_LENGTH+1) ; 
					printf("Keepalive message trigger Ok... \r\n") ;
				}
				else if( uart_buf[1] == MCU_DINPUT_SC ){
					limitswitch = 0 ;
					for(i=0;i<8;i++){
						if( uart_buf[i+2] == 'L' ){
							limitswitch  =  limitswitch | ( 0x01 << i ) ;
						}
						else limitswitch  =  limitswitch | ( 0x00 << i ) ;
					} 
					if( limitswitch ){
						printf("Data input report OK....%02X\r\n", limitswitch);
						limitsw_msg[20] = lineID[0] ; limitsw_msg[21] = lineID[1] ;limitsw_msg[22] = lineID[2] ;limitsw_msg[23] = lineID[3] ;
						limitsw_msg[24] = processID[0] ;  limitsw_msg[25] = processID[1] ; limitsw_msg[26] = processID[2] ; limitsw_msg[27] = processID[3] ;
						limitsw_msg[28] = limitswitch ;
						write(s, limitsw_msg, 29) ;
						
					}
				}
			}
			if(  (uart_buf[0] == LORA_START ) && (uart_buf[ mcu_rx_length - 1 ] == LORA_END )){
                                if( uart_buf[13] == TAG_WAKEUP_SC ){ //send keep alive  message to SPAS
                                        printf("TAG Wakeup  message  Ok... \r\n") ;
					write(fd, period, 18) ;
                                }
                                else if( uart_buf[13] == TAG_UPLOAD_SC ){
                                        printf("TAG_UPload report OK....\r\n");
					write(fd,download, 38) ;
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
