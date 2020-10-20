#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>

using namespace std;

//服务器端口号,需要与服务器端一致,不要修改(确认该端口号未被占用)
#define Port 8847
//服务器地址,根据需要修改,目前比特大陆盒子IP是192.168.1.180
#define adder "192.168.1.180"
#define BuffSize 1024 

int main ( int agrc, char *argv[] )
{
	//socket初始化
    int Socket;

    struct sockaddr_in Server_Address;  
    Socket = socket ( AF_INET, SOCK_STREAM, 0 );
    if ( Socket == -1 )
    {   
        printf ("Can Not Create A Socket!");    
    }

    Server_Address.sin_family = AF_INET;
    Server_Address.sin_port = htons ( Port );
    Server_Address.sin_addr.s_addr = inet_addr(adder);
    if ( Server_Address.sin_addr.s_addr == INADDR_NONE )
    {
        printf ( "Bad Address!" );
    }   
    connect ( Socket, (struct sockaddr *)&Server_Address, sizeof (Server_Address) );

    printf("conneted socket\n");
	
	//读取要传输到比特大陆盒子中的图片文件
    FILE *in = fopen("~/Desktop/test.jpg","rb");
    char Buffer[BuffSize] = {0};
    int len;
    int i = 0;
	//读取文件并通过send发送到比特大陆盒子
    while ((len = fread(Buffer+2, 1, sizeof(Buffer)-3, in)) > 0)
    {           
        Buffer[0] = len / 256;
        Buffer[1] = len % 256;
        int ilen =0;    
        len +=2;
        while(ilen < len ) {        
            int k =send(Socket, Buffer+ilen, len-ilen, 0);
            if(k < 0) {
                printf("send failed\n");
            }
            ilen += k;
        }
    }
    //该字符串是文件传输结束标志
    strcpy( Buffer+2, "FightForWuGaoSuo2019_YiJiaHe_XD_" );
    len = strlen(Buffer+2);
        Buffer[0] = len / 256;
        Buffer[1] = len % 256;
    
    int ilen =0;
    len +=2;
    while(ilen < len ) {        
        int k =send(Socket, Buffer+ilen, len-ilen, 0);
        if(k < 0) {
            printf("send failed\n");
        }
        ilen += k;
    }

	//接受服务器端返回的识别结果
    char Buf[BUFSIZ];
    ilen =0;
    len =2;
    while(ilen < len ) {
        int k = recv(Socket, Buf+ilen, len-ilen, 0);
        if(k < 0) {
            printf("recv faield\n");
            break;
        }
        ilen += k;
    }
    len = Buf[0] * 256 + Buf[1] +2;
    while(ilen < len ) {
        int k = recv(Socket, Buf+ilen, len-ilen, 0);
        if(k<0) {
            printf("recv faield\n");
            break;
        }
        ilen += k;
    }
	//打印识别结果,Buff前两位是传输校验位
    printf("recv result = %s\n", Buf+2);
    string recvstr(Buf+2);
    cout << recvstr << endl;
    if (strcmp(Buf,"ACK") == 0) //"ACK"暂未使用
    {
         printf("Recive ACK\n");
    }        
    //释放资源
    close (Socket);
    fclose(in);

    printf("end\n");
    return 0;   
}
