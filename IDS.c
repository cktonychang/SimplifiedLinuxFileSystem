/* Program Name: IDS.c 
   Writer: Tony_Chang
   Purpose:   Intelligent disk-storage system : n this part the disk server will be made slightly more intelligent. It should obey exactly the same protocol as in the previous part. However,the disk server will gather (cache) a series of up to n requests and schedule/re-order them in
such a way that the blocks can be read/written in an optimal way so that time spent seeking between cylinders is reduced.
   Usage: Server: ./IDS <DiskFileName> <#cylinder> <#sectors per cylinder> <track-to-track delay> <port=11356> <n>

	For example: ./IDS temp 40 80 1000 11356 20
*/
#include"header.h"
#define BLOCKSIZE 256

void error(const char *msg){
    perror(msg);
    exit(-1);
}
char cache[20][300]; char* diskfile;
int SECTORS; int CYLINDERS; long DELAY;  int ctemp; int count; int head;int CACHES = 0;
int parseLine(char *line, char *command_array[]) {
	char *p;
	int count=0;
	p = strtok(line, " "); // divide string into different pieces
	while (p !=0 && count <= 3) {
		command_array[count] = p;
		count++;
		p = strtok(NULL," ");// continue to divide the string if not empty
	}
	if(count == 4){
		char *x;
		for(x = p;*x != 0; x++);*x = ' ';//将最后一个\0变为空格		
		command_array[count] = p;
	}
	return count;
}
int extract(char s[]){ // extract the cylinder num
	char tmp[5] ; char *p; int i = 0;
	for(p = s; *p != ' '; p++);
	p++;
	for(p; *p != ' '; p++)
		tmp[i++] = *p;
	int n = atoi(tmp);
	return n;
}
void SSTF(){
// caculate the shortest move time and take it as the next command
	char temp[CACHES][300];
	int flag[CACHES];
	int queue[CACHES];
	int i; int j; int k;
	for(i = 0; i < CACHES; i++) memcpy(temp[i], cache[i], 300), flag[i] = 0 , queue[i] = extract(cache[i]);
	for(i = 0; i < CACHES; i++){
		int seektime[CACHES]; 
		for(j = 0; j < CACHES; j++) seektime[j] = abs(queue[j] - head);// caculate seektime
		j = 0;
		while(flag[j] != 0 && j < CACHES) j++;
		int min = seektime[j]; k = j;
		for(j = 0; j < CACHES; j++){
			if (seektime[j] <= min && flag[j] == 0 ) min = seektime[j], k = j;}
		flag[k] = 1;
		bzero(cache[i],300);
		memcpy(cache[i], temp[k], 300);
		head = queue[k]; 
	}
}
void C_LOOK(){
//sort the cache and move the head to the first place 
	char temp[CACHES][300];
	int order[CACHES]; int queue[CACHES];
	int i;int j; int temp1; int temp2; int k;
	for(i = 0; i < CACHES; i++) memcpy(temp[i], cache[i], 300), queue[i] = extract(temp[i]), order[i] = i;
	for(j = 0; j < CACHES - 1; j++){  // buble sort
		for(i = 0; i < CACHES - 1 - j; i++){
		    if(queue[i] > queue[i+1]){
		        temp1=queue[i]; temp2 = order[i];
		        queue[i]=queue[i+1]; order[i] = order[i+1];
		        queue[i+1]=temp1; order[i+1] = temp2;
		    }
		}
	}
	k = CACHES;
	for(i = 0; i < CACHES; i++)
		if(queue[i] >= head){
			 k = i;
			 break;
		}
	for(i = 0; i < CACHES; i++){
		memcpy(cache[i], temp[order[k%CACHES]], 300);
		head = extract(cache[i]); // find the head
		k++;
	}
}
int WriteToServer(char type, int client_sockfd, char *command_array[]){
	int single_delay = 0; 
	int c , s, l, n;
	char buf[300] = {0};
	
	if(type == 'R' || type == 'r'){
		if(count != 3 )  write(client_sockfd,"Wrong W command",300); 
		else{
			c = atoi(command_array[1]);
			s = atoi(command_array[2]);
			if(c >= CYLINDERS || s >= SECTORS) write(client_sockfd,"No",256); 
			else {
				char d[300] = "Yes ";
				printf(">R %d %d\n",c,s);
				memcpy (buf, &diskfile[BLOCKSIZE * (c * SECTORS + s)], BLOCKSIZE);
				strcat(d, buf);
				single_delay = DELAY * abs(ctemp - c);
				usleep(single_delay);
				n = write(client_sockfd,d,300);
				//n = write(client_sockfd,buf,300);//debug
				if (n < 0) error("ERROR writing to socket");
			}
		}
	}
	else if(type == 'W' || type == 'w'){
		if(count != 4 ) write(client_sockfd,"Wrong W command",256);
		else{
			c = atoi(command_array[1]);
			s = atoi(command_array[2]);
			l = atoi(command_array[3]);
			if(c >= CYLINDERS || s >= SECTORS || l > 256) write(client_sockfd,"No",256);
			else {
				single_delay = DELAY * abs(ctemp - c);
				usleep(single_delay);
				char buff[300] = {0};
				printf(">W %d %d\n",c,s);
				memcpy (buff, command_array[4],l);

				memcpy (&diskfile[BLOCKSIZE * (c * SECTORS + s)], buff, BLOCKSIZE);
				n = write(client_sockfd,"Yes",300); 
				//n = write(client_sockfd,buff,300);//debug
				if (n < 0) error("ERROR writing to socket");
			}
		}	
	}	
	else if(type == 'Q' || type == 'q') {}
	else {
		char s[300] = "Wrong command ";
		write(client_sockfd,s,300); 
	}
	ctemp = c;
	return single_delay;
}

int main(int argc, char* argv[]){
	if(argc != 7) error("argument error!");
// *********************************************************************************************************************
	SECTORS = atoi(argv[3]);
	CYLINDERS = atoi(argv[2]);
	DELAY = atoi(argv[4]);
	int fd = open (argv[1], O_RDWR | O_CREAT, 0777);//fd：有效的文件描述词。
	if (fd < 0) {
		printf("Error: Could not open file '%s'.\n", argv[1]);
		exit(-1);}

	//Stretch the file size to the size of the simulated disk
	long FILESIZE = BLOCKSIZE * CYLINDERS * SECTORS; 
	int result = lseek (fd, FILESIZE-1, SEEK_SET);
						
	if (result == -1) {
		close (fd);
		error ("Error calling lseek() to 'stretch' the file");
	}

	//Write something at the end of the file to ensure the file actually have the new size.
	result = write (fd, "", 1);
	if (result != 1) {
		close(fd);
		error("Error writing last byte of the file");
	}

	//Map the file  create a mapping between a memory space and a file
	diskfile = (char *) mmap (0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (diskfile == MAP_FAILED){
		close(fd);
		error("Error: Could not map file.");
	}
// *****************************************************************************************************************************	
	//socket begin 
	int sockfd;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("ERROR opening socket");
		exit(2);
	}
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[5]));
	 if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
	listen(sockfd,5);//listen
	int client_sockfd;
	struct sockaddr_in client_addr;
	int len = sizeof(client_addr);
	printf("Please choose schedule algorithm: \n1 : SSTF.\n2 : C-LOOK.\nothers : FCFS.\n");
	int sel = 0;
	scanf("%d",&sel);
	//accept
	while(1){
		CACHES = atoi(argv[6]); 
		client_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &len);
		if (client_sockfd < 0) error("ERROR on accept");
		int pid = fork();//fork cmd
		if(pid < 0) error("ERROR on fork");
		if(pid == 0){
			close(sockfd);
			ctemp = 0; head = 0;
			int flag = 0; int N = 0; int total_delay = 0; 
			//send cache_size
			char size[20] = {0};
			sprintf(size,"%d", CACHES);
			int n = write(client_sockfd,size,20);
			while(1){  // enter loop
				// collect N commands  if receive Q  ouput all the command
				for(N = 0; N < CACHES; N++){
					bzero(cache[N],300);
					int nread = read(client_sockfd, cache[N], 300); // READ
					//printf("received cmd: %s\n\n",cache[N]);
					if (nread < 0) error("ERROR reading from socket");
					while (cache[N][0] == 'I' || cache[N][0] == 'i' ){
						char buf[256] = {0};
						sprintf(buf,"%d %d", CYLINDERS,SECTORS);
						n = write(client_sockfd,buf,256);
						if (n < 0) error("ERROR writing to socket");
						bzero(cache[N],300);
						nread = read(client_sockfd, cache[N], 300);
						//printf("received cmd: %s\n\n",cache[N]);
						if (nread < 0) error("ERROR reading from socket");
						if(cache[N][0] == 'Q' || cache[N][0] == 'q') {CACHES = N; flag = 1; break;}	
					}
					if(cache[N][0] == 'Q' || cache[N][0] == 'q') {CACHES = N; flag = 1; break;}
					// while (cache[N][0] != 'W' || cache[N][0] != 'R' )  try
				}
				switch (sel){	// scheduling
					case 1 : SSTF(); break;
					case 2 : C_LOOK();break;
					default: break;
				}
				// C-LOOK() OR SSTF() 传递参数必须有CACHES
				for(N = 0; N < CACHES; N++){
					char *command_array[4] = {0};
					count = parseLine(cache[N], command_array);
					total_delay += WriteToServer(cache[N][0], client_sockfd, command_array);//WRITE
					bzero(cache[N],300);
				}
				printf("current total delay is %d\n", total_delay);
				if (flag) break;
		  	 }//while
		exit(0);
		}//if
	else close(client_sockfd);
	}//while
     	close(sockfd);
	munmap(0,FILESIZE);
	return 0;
}


