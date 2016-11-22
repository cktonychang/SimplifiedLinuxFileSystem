/* Program Name: FC.c 
   Writer: Tony_Chang
   Purpose:  As a client of FS, send command given by user to the FS
   Usage: ./FC <DiskServerAddress>  <FSPort=12356>
	For example: ./FC localhost  12356
*/


#include"header.h"
void error(const char *msg){
    perror(msg);
    exit(0);
}
static char buf[8196];
void main(int argc,char *argv[]){

	if( argc != 3 ) error("usage error!");
	int sockfd; int len; int fs_port;
	static struct sockaddr_in 
			serv_adr;
	struct hostent *host;

	host = gethostbyname(argv[1]);
	if(host == (struct hostent *) NULL){
		error("gethostname ");
	}
	fs_port = atoi(argv[2]);
	bzero(&serv_adr, sizeof( serv_adr));
	serv_adr.sin_family = AF_INET;
	memcpy(&serv_adr.sin_addr, host->h_addr, host->h_length);
	serv_adr.sin_port = htons(fs_port);

	if(( sockfd = socket( AF_INET, SOCK_STREAM, 0 )) < 0 ){
		error( "generate error" );
	}

	if( connect(sockfd, (struct sockaddr *)&serv_adr,sizeof(serv_adr))<0){
		error("connect error");
	}

	char temp[2];
	do{
		write(fileno(stdout),"> ",3);
		if((len = read(fileno(stdin),buf,8196))>0)
		{			
			write(sockfd,buf,len);
			if((len = read(sockfd,buf,8196))> 0 )
				write(fileno(stdout),buf,len);
		}
	}while(buf[0] != 'e');
	lab : 
	close(sockfd);
	exit(0);
}
