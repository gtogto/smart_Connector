#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <sys/time.h>

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

#define PAYLOARD 5
#define NULL 6
#define END 7

#define BUF_LEN 1024

volatile int STOP=FALSE; 

void main(int argc, char *argv[])
{
	/* UART */
	int fd, res, ret;
	//char debug_rxd;
	struct termios oldtio,newtio;
	char buf[1024];
	int i ;
	unsigned char test[9] = {0x5B, 0x00, 0x00, 0x00, 0x06, 0x01, 0x02, 0x00, 0x5D};

	/* TCP Socket */
	int s, n;
        char *haddr;
        struct sockaddr_in server_addr;
        //struct sockaddr_in server_addr : 서버의 소켓주소 구조체
        char bufT[BUF_LEN+1];	

	/**/
	char recv_copy_buf[1024];
	int pkt_len;
	char start_p = '[';
	char end_p = ']';

	/**/
	char debug_rxd;
	char state_ch1;
	char rxd1_buf[1024];
	int SPAS_length;
	int ch1_cnt;

	// 20200920_
	int flag;
	int k;
	fd_set fs_status;
		
	if(argc != 2)
        {
                printf("usage : %s ip_Address\n", argv[0]);
                exit(0);
        }
        haddr = argv[1];
 
        if((s = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        {//소켓 생성과 동시에 소켓 생성 유효검사
                printf("can't create socket\n");
                exit(0);
        }
 
        bzero((char *)&server_addr, sizeof(server_addr));
        //서버의 소켓주소 구조체 server_addr을 NULL로 초기화
 
        server_addr.sin_family = AF_INET;
        //주소 체계를 AF_INET 로 선택
        server_addr.sin_addr.s_addr = inet_addr(argv[1]);
        //32비트의 IP주소로 변환
        server_addr.sin_port = htons(PORT);
        //daytime 서비스 포트 번호
 
        if(connect(s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {//서버로 연결요청
                printf("can't connect.\n");
                exit(0);
        }
	else{
		printf("Connect OK\r\n");
	}

      
	// Filling server information 
        /* 
          Open modem device for reading and writing and not as controlling tty
          because we don't want to get killed if linenoise sends CTRL-C.
        */
	fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NONBLOCK ); 	// O_NONBLOCK
	if (fd <0) {perror(MODEMDEVICE); printf("uart open error\n");  exit(-1);}
	tcgetattr(fd,&oldtio); /* save current serial port settings */
	bzero(&newtio, sizeof(newtio)); /* clear struct for new port settings */        
        /* 
          BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
          CRTSCTS : output hardware flow control (only used if the cable has
                    all necessary lines. See sect. 7 of Serial-HOWTO)
          CS8     : 8n1 (8bit,no parity,1 stopbit)
          CLOCAL  : local connection, no modem contol
          CREAD   : enable receiving characters
        */
	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
        /*
          IGNPAR  : ignore bytes with parity errors
          ICRNL   : map CR to NL (otherwise a CR input on the other computer
                    will not terminate input)
          otherwise make device raw (no other input processing)
        */
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
        /* 
          now clean the modem line and activate the settings for the port
        */
	tcflush(fd, TCIFLUSH);
        tcsetattr(fd,TCSANOW,&newtio);        
        
	// 20200920_
	flag = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flag | O_NONBLOCK);	// O_NONBLOCK 

	state_ch1 = START;
	while(1){
		if((n=read(s,bufT,BUF_LEN)) > 0)
	        {//서버가 보내오는 daytime 데이터의 수신
        		bufT[n] = NULL;
			pkt_len = ((int)bufT[0] << 24) | ((int)bufT[1] << 16) | ((int)bufT[2] << 8) | ((int)bufT[3]);
			if((pkt_len+1) == n)
			{
				write(fd,&start_p,1);
				write(fd,bufT,n);
				write(fd,&end_p,1);
			}
		}

		/* Serial receive function */
		if ((n=read(fd, &debug_rxd, 1))>0) {
			
			switch( state_ch1 ){
				case START :
					if( debug_rxd == START_CODE ){
						state_ch1 = LENGTH1 ;
						rxd1_buf[0] = START_CODE ;
					}
					break ;

				case LENGTH1 :
					SPAS_length =  (int)debug_rxd << 24 ;
					state_ch1 = LENGTH2 ;
					rxd1_buf[1] = debug_rxd ;
					break ;

				case LENGTH2 :
					SPAS_length = SPAS_length   | ( (int)debug_rxd << 16) ;
					state_ch1 = LENGTH3 ;
					rxd1_buf[2] = debug_rxd ;
					break ;

				case LENGTH3:
					SPAS_length = SPAS_length    |( (int)debug_rxd << 8) ;
					state_ch1 = LENGTH4 ;
					rxd1_buf[3] = debug_rxd ;
					break ;
	
				case LENGTH4:
					SPAS_length = SPAS_length   | ( (int)debug_rxd ) ;
					ch1_cnt = 5 ;
					rxd1_buf[4] = debug_rxd ;
					state_ch1 = PAYLOARD ;
					break ;

				case PAYLOARD :
					rxd1_buf[ ch1_cnt++] = debug_rxd ;
					if( ch1_cnt == (SPAS_length+1) ) state_ch1 = NULL ;
					break ;

				case NULL :
					if( debug_rxd == 0x00 ){
						state_ch1 = END ;
						rxd1_buf[ ch1_cnt++]  = 0x00 ;
					}
					else state_ch1 = START ;
					break ;

				case END :
					if( debug_rxd == END_CODE ){
						rxd1_buf[ ch1_cnt++]  = END_CODE ;
						for(i=0;i<ch1_cnt; i++)printf("%02X. ",rxd1_buf[i] ) ;
						write(s,(rxd1_buf+1),(ch1_cnt-2));
						printf("Received OK....\n") ;	
					}
					state_ch1 = START;
					ch1_cnt = 0 ;
					break ;
			}
		}
	}	
	
	tcsetattr(fd,TCSANOW,&oldtio);
	close(s);
	return 0;
}
