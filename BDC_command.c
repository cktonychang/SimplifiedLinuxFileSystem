/* Program Name: BDC_command.c 
   Writer: Tony_Chang
   Purpose:  This client  work in a loop, having the user type
commands in the format of the above protocol, send the commands to the disk server,
and display the results to the user. 
   Usage: Client ./BDC <DiskServerAddress> <port=10356>
  (basic disk client, command line based)
	For example: ./BDC localhost 10356
*/
#include"header.h"
void error(const char *msg){
    perror(msg);
    exit(0);
}
int parseLine(char *line, char *command_array[]) {
	char *p;
	int count=0;
	p = strtok(line, " "); // divide string into different pieces
	while (p !=0 ) {
		command_array[count++] = p;
		p = strtok(NULL," ");// continue to divide the string if not empty
	}
	return count;
}
int main(int argc, char *argv[]){
	if(argc != 3) error("argument error!");
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *host;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("ERROR opening socket");
		exit(2);
	}

	//get host
	serv_addr.sin_family = AF_INET;
	host = gethostbyname(argv[1]);
	if (host == NULL) {
        	fprintf(stderr,"ERROR, no such host\n");
       	 	exit(0);
    	}
	memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);
	serv_addr.sin_port = htons(atoi(argv[2]));

	//connect
	if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");
	char buffer_read[300] = {0};
	int n = read(sockfd,buffer_read,20);
	int cache_size = atoi(buffer_read);//接受cache size
	char buffer_write[300] = {0};
	int i;int j; int exit = 0;

	while(1){
		for(i = 0; i < cache_size; i++){ //write command
			bzero(buffer_write,300);
			fgets(buffer_write,300,stdin);
	   		n = write(sockfd,buffer_write,300);//向server写  为debug 将 strlen(buffer_write) 改为 300
	    		if (n < 0) error("Error wrie");
			if(buffer_write[0] == 'Q' || buffer_write[0] == 'q') {exit = 1; break;}
			if(buffer_write[0] == 'I' || buffer_write[0] == 'i'){ // I type command must be execute immediately
 				bzero(buffer_read,300); 
				n = read(sockfd,buffer_read,300);
		    		if (n < 0)  error("Error read");
		    		printf("%s\n",buffer_read);
				bzero(buffer_write,300);
				fgets(buffer_write,300,stdin);
	   			n = write(sockfd,buffer_write,300);
				if (n < 0) error("Error wrie");
			}	
		}
		for(j = 0; j < i; j++){  // when send i command ,client must read n times
		    	bzero(buffer_read,300); 
			n = read(sockfd,buffer_read,300);
		    	if (n < 0)  error("Error read");
		    	printf("%s\n",buffer_read);
		}
	if (exit) break;
	}
	close(sockfd);
	return 0;
}


