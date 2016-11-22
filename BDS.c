/* Program Name: BDS.c
   Writer: Tony_Chang
   Purpose:  Basic disk-storage system: Implement, as an Internet-domain socket server, a simulation of a physical disk. The simulated disk is organized by cylinder and sector.
   Usage: Server: ./BDS <DiskFileName> <#cylinder> <#sectors per cylinder> <track-to-track delay> <port=10356>
	For example: ./BDS temp 1024 1024 1000 10356
*/
#include"header.h"
#define BLOCKSIZE 256

#define CACHES 1
void error(const char *msg){
    perror(msg);
    exit(-1);
}

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

void main(int argc, char* argv[]){
	//open a file
// *********************************************************************************************************************
	int SECTORS = atoi(argv[3]);
	int CYLINDERS = atoi(argv[2]);
	long delay = atoi(argv[4]);
	if (argc != 6) error("ERROR on argument");
	int fd = open (argv[1], O_RDWR | O_CREAT, 0777);//fd：有效的文件描述词。
	if (fd < 0) {
		printf("Error: Could not open file '%s'.\n", argv[1]);
		exit(-1);
	}

	//Stretch the file size to the size of the simulated disk
	long FILESIZE = BLOCKSIZE * CYLINDERS * SECTORS; //BLOCKSIZE * SECTORS * CYLINDERS; FILESIZE以字节为单位
	int result = lseek (fd, FILESIZE-1, SEEK_SET);//SEEK_SET 将读写位置指向文件头后再增加offset(FILESIZE - 1)个位移量。
						//lseek返回目前的读写位置，也就是距离文件开头多少个字节。若有错误则返回-1
	if (result == -1) {
		perror ("Error calling lseek() to 'stretch' the file");
		close (fd);
		exit(-1);
	}

	//Write something at the end of the file to ensure the file actually have the new size.
	result = write (fd, "", 1);
	if (result != 1) {
		perror("Error writing last byte of the file");
		close(fd);
		exit(-1);
	}

	//Map the file  create a mapping between a memory space and a file
	char* diskfile;
	diskfile = (char *) mmap (0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	//start：映射区的开始地址，设置为0时表示由系统决定映射区的起始地址。
	//MAP_SHARED 与其它所有映射这个对象的进程共享映射空间。对共享区的写入，相当于输出到文件。直到msync()或者munmap()被调用，文件实际上不会被更新。成功执行时，mmap()返回被映射区的指针
	if (diskfile == MAP_FAILED){
		close(fd);
		printf("Error: Could not map file.\n");
		exit(-1);
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
	 if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
	listen(sockfd,5);//listen
	int client_sockfd;
	struct sockaddr_in client_addr;
	int len = sizeof(client_addr);
	//accept
	while(1){
		client_sockfd = accept(sockfd, (struct sockaddr*) &client_addr, &len);
		if (client_sockfd < 0) error("ERROR on accept");
		int pid = fork();//fork cmd 
		if(pid < 0) error("ERROR on fork");
		if(pid == 0){
			close(sockfd);
			int count= 0; int c= 0; int s = 0; int l = 0; int ctemp = 0; int n = 0;
			char buf[300] = {0};
			char size[20] = {0};
			sprintf(size,"%d", CACHES);
			n = write(client_sockfd,size,20); // tell the client the cachesize
			
			while(1){  // enter loop
				bzero(buf,300);
				char *command_array[4] = {0};
		     		int nread = read(client_sockfd, buf, 300);
				//printf("received cmd: %s",buf);
				if (nread < 0) error("ERROR reading from socket");
				count = parseLine(buf, command_array);//divide buf
				ctemp = c; // record current head
				switch (buf[0]){
					case 'I': 
						sprintf(buf,"%d %d", CYLINDERS,SECTORS);
						n = write(client_sockfd,buf,256);
		    				if (n < 0) error("ERROR writing to socket");
						break;
					case 'R': 
						if(count != 3 ) {
								char s[256] = "Wrong R command: ";
								strcat(s, buf);
								write(client_sockfd,buf,256); break;
								break;}
						c = atoi(command_array[1]);
						s = atoi(command_array[2]);
						if(c >= CYLINDERS || s >= SECTORS) write(client_sockfd,"No",256);
						else {
					
							char d[300] = "Yes ";
							memcpy (buf, &diskfile[BLOCKSIZE * (c * SECTORS + s)], BLOCKSIZE);
							strcat(d, buf);
							usleep(delay * abs(ctemp - c)); // simulate disk delay
							n = write(client_sockfd,d,300);
							//n = write(client_sockfd,buf,300);//debug
		     					if (n < 0) error("ERROR writing to socket");
						}
						break;
					case 'W': 
						if(count != 4 ) {write(client_sockfd,"Wrong W command",256);break;}
						c = atoi(command_array[1]);
						s = atoi(command_array[2]);
						l = atoi(command_array[3]);
						if(c >= CYLINDERS || s >= SECTORS || l > 256) write(client_sockfd,"No",256);
						else {
							usleep(delay * abs(ctemp - c));
							char buff[300] = {0};///
							memcpy (buff, command_array[4],l);

							memcpy (&diskfile[BLOCKSIZE * (c * SECTORS + s)], buff, BLOCKSIZE);
							n = write(client_sockfd,"Yes",300); 
							//n = write(client_sockfd,buff,300);//debug
		     					if (n < 0) error("ERROR writing to socket");
						}	
						break;
					case 'Q': printf("Q"); goto flag; break;
					default: {
						char s[300] = "Wrong command: ";
						//strcat(s, buf);
						write(client_sockfd,s,300); break;}
					}//switch
		  	 }//while
		exit(0);
		}//if
	else close(client_sockfd);
	}//while
	flag: 
     	close(sockfd);
	munmap(0,FILESIZE);
}


