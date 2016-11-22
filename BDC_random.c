/* Program Name: BDC_rand.c 
   Writer: Tony_Chang
   Purpose:  This client should query the disk for its size (using the I command),
and then generate a series of N random requests to the disk. Each request should
randomly be W or R, and the cylinder number and sector number should be randomly
chosen from the valid range for that disk. Write requests should all write 256 bytes of
data. The value of N should be specified on the command line. 
   Usage: Client ./BDC_rand <DiskServerAddress> <port=10356> N
  (basic disk client, random data )
	For example: ./BDC_rand localhost 10356 400
*/
#include"header.h"
void error(const char *msg){
    perror(msg);
    exit(0);
}
int random_num(int r){ // get rand num small than r
	int n=rand()%r;
	return n;
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
	if(argc != 4) error("ERROR on argument");
	srand((unsigned)time(NULL));
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
	int N = atoi(argv[3]) - 1;
	char buffer_read[300] = {0};
	int n = read(sockfd,buffer_read,20);
	int cache_size = atoi(buffer_read);//receive cache size

	char buffer_write[300] = "I";// send Itype command first
	char *divide[3] = {0};
	n = write(sockfd,buffer_write,strlen(buffer_write));
		if (n < 0)  error("Error wirting to server");
	bzero(buffer_write,300);
	n = read(sockfd,buffer_read,300);///
	if (n < 0)  error("E");
	printf("%s\n",buffer_read);//debug
	parseLine(buffer_read, divide);
	
	int Cylinders = atoi(divide[0]);
	int Sectors = atoi(divide[1]);
	int i;
	while(N > 0){
		for(i = 0; i < cache_size; i++){ //write command
			bzero(buffer_write,300);
			int c = random_num(Cylinders);
			int s = random_num(Sectors);
			int t = random_num(2);
			switch (t){
			case 0:
				sprintf(buffer_write,"R %d %d",c,s);
				printf(">R %d %d\n",c,s);//debug
				//printf("buffer_write : %s\n", buffer_write);
				break;
			case 1:
				{
				char data[257] = {0}; int k;
				for(k = 0; k< 256; k++) data[k] = rand()%95+32;
				data[256] = '\0';
				sprintf(buffer_write,"W %d %d 256 %s",c,s,data);
				printf(">W %d %d\n",c,s);//debug
			//printf("buffer_write : %s\n\n", buffer_write);
	 			break;
				}
			}
	   		n = write(sockfd,buffer_write,300);
	    		if (n < 0) error("Error wrie");
			if (N-- == 1){
				sprintf(buffer_write,"Q");
				n = write(sockfd,buffer_write,300);
				break;
			}
		}
		for(i = 0; i < cache_size; i++){ 
		    	bzero(buffer_read,300); 
			n = read(sockfd,buffer_read,300);
		    	if (n < 0)  error("Error read");
		    	//printf("%s\n",buffer_read); read from server
		}

	}
	close(sockfd);
	return 0;
}


