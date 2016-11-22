/* Program Name: FS.c 
   Writer: Tony_Chang
   Purpose:  File system: Implement a inode file system. The file system should provide operations such as: initialize the file-system, create a file, read the data from a file, write a file with given data, append data to a file, remove a file, create directories
   Usage: ./FS <DiskServerAddress> <BDSPort=10356> <FSPort=12356>
	For example: ./FS localhost 10356 12356
*/

#include"header.h"
#define BLOCK_SIZE 256

typedef struct inode //127 bytes
{
	int father_dir;  //上一级目录
	int size; //文件大小 ,或包含文件数
	int number;    //文件标识号码
	char file_or_dir;//区分目录还是文件  f---file; d---dir
	int index_num; //记录index的使用情况,指向当前可写的块，写满后指向下一个
	int index[27];  //索引，后三个为多级的
}inode;

//1 superblock, 1 bitmap block, 16 blockbitmap blocks, 64 inodemap blocks; 
// global variable statement
int disk_port;
int fs_port;
char buffer_read[300] = {0};
char buffer_write[300] = {0};
char command[300] = {0};
char inode_bitmap[BLOCK_SIZE*1024];
char block_bitmap[16*BLOCK_SIZE*1024];
inode inode_map[BLOCK_SIZE*1024];

int current; // current directory
int current_inode; // current inode number
int current_block; // current block number
int CYLINDER; // total cylinder
int SECTOR; //sector per cylinder
int first_build; 
int INBIT; // total inode bitmap  block numbers
int BLBIT; // total block bitmap  block numbers
int IMAP; // total inode map block number
int GUEST;

//fuction statement
int parse_line(char *, char *[], int); // divide command
void get_disk_info(int); // get cylinder and sector
void write_to_inode_bitmap(int); // save inode map
void read_from_disk(int, char[], int, int); // disk reading function
void write_to_block_bitmap(int);// save blockbit map
void write_inode_table(int);
void write_to_disk(int, char[], int, int); // disk writing function
int find_inode_by_name(int, char[], int); 
int divide(char *, char *[]);
void read_from_inode_bitmap(int );
int find_next_inode();
int find_next_block();
char write_to_file(int, int, char *, int); // write data to file
char clean_file(int); // clean the bitmap of file
void remove_name(int,int, char[]); // remove name from high dir
void remove_dir(int , char[] , int );
char* i2a(int ,char* ); // change int to char*
int a2i(char*);	// change char * to int
void format();
void read_from_inode_bitmap(int);
void read_from_block_bitmap(int);
void write_to_inode_map(int );
void read_from_inode_map(int );
void r(int orig_sock){
	read_from_inode_bitmap(orig_sock);
	read_from_block_bitmap(orig_sock);
	read_from_inode_map(orig_sock);
}
void w(int orig_sock){
	write_to_inode_bitmap(orig_sock);
	write_to_block_bitmap(orig_sock);
	write_to_inode_map(orig_sock);
}



void main(int argc, char *argv[]){
if (argc != 4){ perror("ERROR on argument"); exit(1);}
//******************  initialization   ******************************8
// connect to disk
int orig_sock,
	len;
static struct sockaddr_in
	serv_adr;
struct hostent *host;

disk_port = atoi(argv[2]);
fs_port = atoi(argv[3]);

host = gethostbyname(argv[1]);
if(host == (struct hostent *) NULL){
	perror("ERROR on on gethostname "); exit(2);		
}
memset(&serv_adr, 0, sizeof( serv_adr));
serv_adr.sin_family = AF_INET;
memcpy(&serv_adr.sin_addr, host->h_addr, host->h_length);
serv_adr.sin_port = htons(disk_port);

if(( orig_sock = socket( AF_INET, SOCK_STREAM, 0 )) < 0 ){
	perror( "ERROR on generating" ); exit(3);
}

if( connect(orig_sock, (struct sockaddr *)&serv_adr,sizeof(serv_adr))<0){
	perror("ERROR on connecting"); exit(4);
}
bzero(buffer_read,300);
int sb = read(orig_sock,buffer_read,20);
int cache_size = atoi(buffer_read);
//connect with f_client
int sockfd, // file_system as a server
	new_sockfd,
	client_len;
static struct sockaddr_in
	client_adr;
if((sockfd = socket(AF_INET,SOCK_STREAM,0))<0){
	printf("ERROR on generating！"); exit(5);
}

memset(&client_adr,0,sizeof(client_adr));
client_adr.sin_family = AF_INET;
client_adr.sin_addr.s_addr = htonl(INADDR_ANY);
client_adr.sin_port = htons(fs_port); // file system as a client of disk system

if(bind(sockfd,(struct sockaddr *) &client_adr,sizeof(client_adr))<0){
	printf("ERROR on binding!");
	close(sockfd); exit(6);	
}

if(listen(sockfd,5)>0){
	printf("ERROR on listening"); exit(7);
}
//***************************************************************************
do{
	client_len = sizeof(client_adr);
	if((new_sockfd = accept(sockfd,(struct sockaddr *)&client_adr, &client_len))<0){
		printf("ERROR on accepting!");
		close(sockfd);
		exit(8);
	}
	get_disk_info(orig_sock);
	INBIT = (SECTOR*CYLINDER/512+1); //num of inode_birmap
	BLBIT = (INBIT/2*3);	// num of block_bitmap
	IMAP = (INBIT*256);  // num of inode /2
	int len; int flag = 1;
	while(flag == 1){
		len = read(new_sockfd,command,8196);
	if(len > 0){
		command[len-1] = '\0'; int k;
		char *command_array[5] = {0};
		for(k=3;k<5;k++)command_array[k] = (char *) malloc(8196);
		
		int count = parse_line(command, command_array, 4);
		if (count == 0) {write(new_sockfd,"\n",strlen("\n")); continue;}
		if(strcmp(command,"exit") == 0){
			//保存当前状态，存入磁盘系统，退出循环
			write_to_inode_bitmap(orig_sock);
			write_to_block_bitmap(orig_sock);
			write_to_inode_map(orig_sock);
			write(new_sockfd,"Goodbye!\n",strlen("Goodbye!\n"));
			flag = 0;
		}
		else if(command[0] == 'f' && command[1] == '\0'){
			//初始化文件系统
			current = 0;// !!!!!
			current_inode = 0;
			current_block = 1 + INBIT + BLBIT + IMAP; //first avaliable block
			first_build = 1;
			int i;
			printf("f begin \n");		

			for(i = 0; i < INBIT *256;i++)
				inode_bitmap[i] = 'y';  // y stands for not used

			for(i = 0;i < BLBIT*256; i++)
				block_bitmap[i] = 'y';
			inode root_node;
			root_node.father_dir = 0; 
			root_node.size = 0; 
			root_node.number = current_inode;    
			root_node.file_or_dir = 'd';
			root_node.index_num = 0;//start from 82 1+1+16+64
			root_node.index[0] = current_block;
			for(i=1;i<27;i++)
				root_node.index[i] = -1;  
			inode_map[0] = root_node;
			inode_bitmap[current_inode] = 'n';
			block_bitmap[current_block] = 'n'; 
			//w( orig_sock);
			write(new_sockfd,"Initialized!\n",strlen("Initialized!\n"));
			
		}
	//inode_map[current].index[0] = current_block
		else if(strcmp(command_array[0], "mk") == 0 && count == 2){
			//创建文件 command_array[1]
			if(command_array[1] == (char*)0 || command_array[2] != (char*)0){
				write(new_sockfd,"Wrong command!\n",strlen("Wrong command!\n"));
				continue;
			}
			//r( orig_sock);
			int num = find_inode_by_name(orig_sock, command_array[1], current);
			if(strlen(command_array[1])>11){			
				write(new_sockfd,"Error: Too long file name!\n",strlen("Error: Too long file name!\n"));
			}
			
			else if(num != -1 ){			
				write(new_sockfd,"Error: Wrong file name!\n",strlen("Error: Wrong file name!\n"));
			}
			else{
			int i;
			inode temp;
			temp.father_dir = current;
			temp.size = 0; 
			temp.number = find_next_inode();
			temp.file_or_dir = 'f';
				temp.index_num = 0;//start from 82 1+1+16+64
			temp.index[0] = find_next_block();
			for(i=1;i<27;i++)
				temp.index[i] = -1;  
			inode_map[current_inode] = temp;
			inode_bitmap[current_inode] = 'n';
			block_bitmap[current_block] = 'n'; 

			inode_map[current].size += 1;
			//在father_dir里写入文件名 根据inode号码找到上级目录  read_from_disk(int sockfd, char *buff, int c, int s)
			char buff[261] = {0};
			int j = 0;
			if (first_build == 0){  // copy information from high directroy
				for (j = 0; j < 27; j++){
					if (inode_map[current].index[j] != -1 ){
						read_from_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
						if(strlen(buff) >= 239) {
							bzero(buff, 261);
							
						}
						else break;
					}
					else if (inode_map[current].index[j] == -1 ){
						inode_map[current].index[j] = find_next_block(); bzero(buff, 261); break;}
				}
			}
			if (first_build == 1) first_build = 0;
			// append new file
			char a[16] = {0};
			strcat(a, command_array[1]);
			strcat(a, " ");
			char node_num[10];
			sprintf(node_num,"%d",temp.number);
			strcat(a, node_num);
			strcat(a, " ");
			while(strlen(a) < 16) strcat(a," ");
			strcat(buff , a);
			//w( orig_sock);
			write_to_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
			if(write(new_sockfd,"Yes!\n",strlen("Yes!\n")) < 0) perror("ERROR on writing to client");
			}
		}

		else if(strcmp(command_array[0], "mkdir") == 0 && count == 2){
			//创建路径 command_array[1]
			if(command_array[1] == (char*)0 || command_array[2] != (char*)0){
				write(new_sockfd,"Wrong command!\n",strlen("Wrong command!\n"));
				continue;
			}
			//r( orig_sock);
			int num = find_inode_by_name(orig_sock, command_array[1], current);
			if(strlen(command_array[1])>11){			
				write(new_sockfd,"Error: Too long dir name!\n",strlen("Error: Too long dir name!\n"));
			continue;
			}
			
			if(num != -1 ){			
				write(new_sockfd,"Error: Wrong dir name!\n",strlen("Error: Wrong dir name!\n"));
				continue;
			}
			else{
			int i;
			inode temp;
			temp.father_dir = current;
			temp.size = 0; 
			temp.number = find_next_inode();
			temp.file_or_dir = 'd';
				temp.index_num = 0;
			temp.index[0] = find_next_block();
			for(i=1;i<27;i++)
				temp.index[i] = -1;  
			inode_map[current_inode] = temp;
			inode_bitmap[current_inode] = 'n';
			block_bitmap[current_block] = 'n'; 
			inode_map[current].size += 1;
			//在father_dir里写入文件名 根据inode号码找到上级目录  read_from_disk(int sockfd, char *buff, int c, int s)
			char buff[261] = {0};
			int j = 0;
			if (first_build == 0){
				for (j = 0; j < 27; j++){
					if (inode_map[current].index[j] != -1 ){
						read_from_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
						if(strlen(buff) >= 239) {
							bzero(buff, 261);
							
						}
						else break;
					}
					else if (inode_map[current].index[j] == -1 ){
						inode_map[current].index[j] = find_next_block(); bzero(buff, 261); break;}
				}
			}
			if (first_build == 1) first_build = 0;
			char a[16] = {0};
			strcat(a, command_array[1]);
			strcat(a, " ");
			char node_num[10];
			sprintf(node_num,"%d",temp.number);
			strcat(a, node_num);
			strcat(a, " ");
			while(strlen(a) < 16) strcat(a," ");
			strcat(buff , a);
			
			write_to_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
			//w( orig_sock);
			if(write(new_sockfd,"Yes!\n",strlen("Yes!\n")) < 0) perror("ERROR on writing to client");
			}
		}

		else if(strcmp(command_array[0], "rm") == 0 && count == 2){
			//删除文件 command_array[1] 并清除inode
			//在当前目录下搜索文件名，对应inode号，清除Inode信息和inodebit信息 ,到上级目录删除文件名和信息
			if(command_array[1] == (char*)0 || command_array[2] != (char*)0){
				write(new_sockfd,"Wrong command!\n",strlen("Wrong command!\n"));
				continue;
			}
			//r( orig_sock);
			if (inode_map[current].size == 0 ) write(new_sockfd,"Error: No such file!\n",strlen("Error: No such file!\n"));
			else {
				int num = find_inode_by_name(orig_sock, command_array[1], current);
				if(num == -1)		
					write(new_sockfd,"Error: No such file!\n",strlen("Error: No such file!\n"));
				else {
					clean_file(num);
					remove_name(current, orig_sock, command_array[1]);
					//w( orig_sock);
					if(write(new_sockfd,"Yes!\n",strlen("Yes!\n")) < 0) perror("ERROR on writing to client");
				}	
			}
		}

		else if(strcmp(command_array[0], "rmdir") == 0 && count == 2){
			//删除路径 command_array[1] 并清除inode
			//清除每一个block，并将block中每一个下层文件进行clean_file
			if(command_array[1] == (char*)0 || command_array[2] != (char*)0){
				write(new_sockfd,"Wrong command!\n",strlen("Wrong command!\n"));
				continue;
			}
			//r( orig_sock);
			if (inode_map[current].size == 0 ) write(new_sockfd,"Error: No such file!\n",strlen("Error: No such file!\n"));
			else {
				int num = find_inode_by_name(orig_sock, command_array[1], current);
				if(num == -1)		
					write(new_sockfd,"Error: No such file!\n",strlen("Error: No such file!\n"));
				else{

					remove_dir( current, command_array[1],  orig_sock); int i;
					for(i=0;i < 27;i++){ // first pointer
							if (inode_map[num].index[i] != -1)
								block_bitmap[inode_map[num].index[i]] = 'y';
						}
					remove_name(current, orig_sock, command_array[1]);
					//w( orig_sock);
					if(write(new_sockfd,"Yes!\n",strlen("Yes!\n")) < 0) perror("ERROR on writing to client");
					
				}
			}

		}

		else if(strcmp(command_array[0], "cd") == 0 && count == 2){
			//移动到路径 command_array[1] 
			int temp_dir = current; int a = 1;
			if(command_array[1] == (char*)0 || command_array[2] != (char*)0){
				write(new_sockfd,"Wrong command!\n",strlen("Wrong command!\n")); a = 0;
			}
			//r( orig_sock);
			if(strcmp(command_array[1],".") == 0)	write(new_sockfd, "Yes\n", strlen("Yes\n")) , a = 0;	//do nothing
			else if(strcmp(command_array[1],"..") == 0)	// move to up dir
				current = inode_map[current].father_dir;
			else if(strcmp(command_array[1],"/")==0)
				current = 0; // move to top dir
			else{
				
				char *temp_name;			
				temp_name = strtok(command_array[1], "/");
				int dir_num;
				if(command_array[1][0] == '/')
					current = 0;
				do{
					dir_num = find_inode_by_name(orig_sock, temp_name, current);
					if(inode_map[dir_num].file_or_dir != 'd'){
						write(new_sockfd,"Error: not a directory !\n",strlen("Error: not a directory !\n"));
						current = temp_dir; a = 0;
						break;
					}
					if(dir_num == -1){					
						write(new_sockfd,"Error: Wrong path!\n",strlen("Error: Wrong path!\n"));
						current = temp_dir; a = 0;
						break;
					}
					temp_name = strtok(NULL, " ");
					current = dir_num;
				}while(temp_name != (char *)0);
				
			}
			if (a == 1) {//w( orig_sock); 
				write(new_sockfd, "Yes\n", strlen("Yes\n"));}

		}

		else if(strcmp(command_array[0], "ls") == 0 && count == 2 ){
			printf("ls begin\n");
				//r( orig_sock);
			 if (inode_map[current].size == 0) {
				write(new_sockfd,"\n",strlen("\n"));
			}
			else{
				int type = atoi(command_array[1]);
				if(type == 1){

				int j; char print[8192] = {0};
				for(j = 0; j < 27; j++){  // find file name in current dir
					if (inode_map[current].index[j] != -1){
						char buff[256] = {0};
						char *temp[60] = {0};
						read_from_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
						divide(buff,temp);
						int i;
						for (i = 0; temp[i] != (char *)0 ; i++){
							strcat(print, temp[i++]);
							strcat(print,"     ");
						}
					}	
					else break;
				}
				strcat(print,"\n");
				write(new_sockfd,print,strlen(print));
				}
				if (type == 0){

					int j; char print[8192] = {0};
					for(j = 0; j < 27; j++){
						if (inode_map[current].index[j] != -1){
								char buff[300] = {0};
								char *temp[60] = {0};
								char c[2] = {0};
								char size[10] = {0};
							read_from_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
								if (buff !=  (char *)0){
									//printf("before divide buff ls %s\n",buff);
									divide(buff,temp);
									//printf("after divide in ls\n");
								// list filename and inode number and size 
									int i;
									for (i = 0; temp[i] != (char *)0 ; i++){
										strcat(print, temp[i]);
										strcat(print," -----");
										strcat(print, temp[++i]);	
										strcat(print," -----");
										sprintf(c, "%c", inode_map[ atoi(temp[i]) ].file_or_dir);
										strcat(print,c);
										strcat(print," -----");
										sprintf(size, "%d", inode_map[ atoi(temp[i]) ].size);
										strcat(print,size);
										strcat(print,"\n");
									}
							}
						}	
						else break;
					}
				
				printf("copying...\n");
				//printf("print = %s\n", print);
				//w( orig_sock);
				write(new_sockfd,print,strlen(print));
				}// list the file and display all the information
			}
			
		}

		else if(strcmp(command_array[0], "cat") == 0 && count == 2 ){
			//显示文件 command_array[1] 的内容 
			printf("cat...\n");
			if(command_array[1] == (char*)0 || command_array[2] != (char*)0){
				write(new_sockfd,"Wrong command!\n",strlen("Wrong command!\n"));
				continue;
			}
			//r( orig_sock);
			int num = find_inode_by_name(orig_sock, command_array[1], current);
			if(num == -1)		
				write(new_sockfd,"Error: No such file!\n",strlen("Error: No such file!\n"));
			else {
				int j; char print[20000] = {0};
				for(j = 0; j < 20; j++){  // directory point
					if (inode_map[num].index[j] != -1){
						char buff[256] = {0};
						read_from_disk(orig_sock, buff, inode_map[num].index[j] / SECTOR, inode_map[num].index[j] % SECTOR);
						strcat(print, buff);
						}
					else break;
				}
				
				for(j = 20; j < 26; j++){  // single dir
					if (inode_map[num].index[j] != -1){
						int i;
						for(i = 0; i < 27; ++i){
							int block_num = inode_map[ inode_map[num].index[j] ].index[i];
							if (block_num != -1){
								char buff[256] = {0};
								read_from_disk(orig_sock, buff, block_num / SECTOR, block_num % SECTOR);
								strcat(print, buff);  
							}
						}
					}	
					else break;
				}
				if (inode_map[num].index[26] != -1){ // double dir
					int inode_n = inode_map[num].index[26];
					for(j = 0; j < 27; j++){ //  每个对应一个inode
						if (inode_map[inode_n].index[j] != -1){
								int i;
							for(i = 0; i < 27; ++i){
								int block_num = inode_map[ inode_map[inode_n].index[j] ].index[i];
								if (block_num != -1){
									char buff[256] = {0};
									read_from_disk(orig_sock, buff, block_num / SECTOR, block_num % SECTOR);
									strcat(print, buff);  
								}
							}
						}	
						else break;
					}

				}
				strcat(print, "\n");
				//w( orig_sock);
				write(new_sockfd,print,strlen(print));
			}
		}

		else if(strcmp(command_array[0], "w") == 0 && count >= 3){
			printf("w ...\n");
			int l = atoi(command_array[2]); int i;
			//r( orig_sock);
			if (0 < l < 20000) {
				// write to block
				int num = find_inode_by_name(orig_sock, command_array[1], current);
				if(num == -1)		
					write(new_sockfd,"Error: No such file!\n",strlen("Error: No such file!\n"));
				else {
					write_to_file(num, l , command_array[3], orig_sock);
					//w( orig_sock);
					write(new_sockfd,"Yes!\n",strlen("Yes!\n"));
				}
			}
			else write(new_sockfd,"Size is too big!\n",strlen("Size is too big!\n"));
		}

		else if(strcmp(command_array[0], "a") == 0 && count >= 3){
			char *filename = command_array[1];
			int l = atoi(command_array[2]);
			//r( orig_sock);
			//****************************************************************************************************
			printf("append...\n");

			// append = cat + write
			int num = find_inode_by_name(orig_sock, command_array[1], current);
			if(num == -1)		
				write(new_sockfd,"Error: No such file!\n",strlen("Error: No such file!\n"));
			else {
				int j; char print[8196] = {0};
				for(j = 0; j < 20; j++){
					if (inode_map[num].index[j] != -1){
						char buff[256] = {0};
						read_from_disk(orig_sock, buff, inode_map[num].index[j] / SECTOR, inode_map[num].index[j] % SECTOR);
						strcat(print, buff);
						}
					else break;
				}
				
				for(j = 20; j < 26; j++){
					if (inode_map[num].index[j] != -1){
						int i;
						for(i = 0; i < 27; ++i){
							int block_num = inode_map[ inode_map[num].index[j] ].index[i];
							if (block_num != -1){
								char buff[256] = {0};
								read_from_disk(orig_sock, buff, block_num / SECTOR, block_num % SECTOR);
								strcat(print, buff); 
							}
						}
					}	
					else break;
				}
				if (inode_map[num].index[26] != -1){
					int inode_n = inode_map[num].index[10];
					for(j = 0; j < 27; j++){ // 每个对应一个inode
						if (inode_map[inode_n].index[j] != -1){
								int i;
							for(i = 0; i < 27; ++i){
								int block_num = inode_map[ inode_map[inode_n].index[j] ].index[i];
								if (block_num != -1){
									char buff[256] = {0};
									read_from_disk(orig_sock, buff, block_num / SECTOR, block_num % SECTOR);
									strcat(print, buff);  
								}
							}
						}	
						else break;
					}

				}
				strcat(print, command_array[3]);
				//write to file
				write_to_file(num, l + inode_map[num].size , print, orig_sock);
				//w( orig_sock);
				write(new_sockfd,"Yes!\n",strlen("Yes!\n"));
			}
		}
		else write(new_sockfd,"Wrong command!\n",strlen("Wrong command!\n"));

	}// main if
	}// main while

close(orig_sock);
exit(0);
}while(1);

}//End of main


int parse_line(char *line, char *command_array[] , int total) {
	char *p;
	int count = 0;
	p = strtok(line, " "); // divide string into different pieces
	while (p !=0 && count < 3) {
		//if(*p == ' ') p++;
		command_array[count] = p;
		count++;
		p = strtok(NULL," ");// continue to divide the string if not empty
	}
	if(count == 3){
		char *x;
		for(x = p;*x != 0; x++); *x = ' ';//将最后一个\0变为空格		
		command_array[count] = p;
	}

	return count;
}

void write_to_disk(int sockfd, char data[], int c, int s){
	char cmd[300] = {0};
	sprintf(cmd, "W %d %d %d", c, s, 256); 
	strcat(cmd," ");
	strcat(cmd,data);
	write(sockfd,cmd,strlen(cmd));
	if(read(sockfd, cmd, 300) < 0) perror("ERROR on reading disk"), exit(12);
}

void read_from_disk(int sockfd, char *buff, int c, int s){
	char cmd[300] = {0};
	sprintf(cmd, "R %d %d ", c, s);
	write(sockfd,cmd,strlen(cmd));
	if(read(sockfd, buff, 300) < 0) perror("ERROR on reading disk"), exit(12);
	//remove the yes in the buff
	int i = 0;
	if (strlen(buff) <= 4) buff[0] = '\0';
	else {
		for(i;i < strlen(buff) - 4;i++) buff[i] = buff[i+4];
		buff[i] = '\0';
		}
}

void get_disk_info(int sockfd){
	bzero(buffer_write,300);
	bzero(buffer_read,300);
	sprintf(buffer_write,"I"); // I type
	if (write(sockfd,buffer_write,300) < 0) perror("Error wrie"),exit(9);
	
	if (read(sockfd,buffer_read,300) < 0) perror("Error read"),exit(10);
	char *command_array[3] = {0};
	int count = parse_line(buffer_read, command_array, 3);
	if (count == 2){
		CYLINDER = atoi(command_array[0]);
		SECTOR = atoi(command_array[1]);
	}
	else perror("Error read cylinders"),exit(10);
	if (CYLINDER * SECTOR < 82 ) perror("Disk is too small!"),exit(11);
}

void write_to_inode_bitmap(int orig_sock){
	int i,j;
	int k = 0;
	int n = 1; // start from i sector
	for(i = 0; i < INBIT; i++){
		char bit[256] = {0};
		for(j = 0;j < BLOCK_SIZE; j++){
			bit[j] = inode_bitmap[k++];
		}
		write_to_disk(orig_sock, bit, n / SECTOR, n % SECTOR);
		n++;
	}
}

void read_from_inode_bitmap(int orig_sock){ // get inode_bitmap
	int i,j; char buff[257] = {0};
	int n = 1;
	for(i = 0; i < INBIT; i++){
		read_from_disk(orig_sock, buff, n / SECTOR, n % SECTOR);
		n++;
		for(j=0;j<256;j++)
			inode_bitmap[i*256+j] = buff[j];	
	}
}


void read_from_block_bitmap(int orig_sock){ // get block_bitmap
	int i,j;
	int n = 1 + INBIT; 
	char buff[257] = {0};
	for(i = 0; i < BLBIT; i++){
		read_from_disk(orig_sock, buff, n / SECTOR, n % SECTOR); n++;
		for(j = 0; j < 256; j++)
			block_bitmap[i*256+j] = buff[j];	
	}
}

void write_to_block_bitmap(int orig_sock){
	int i,j;
	int k = 0;
	int n = 1 + INBIT;
	for(i = 0; i < BLBIT; i++){
		char bit[256] = {0};
		for(j=0;j<BLOCK_SIZE;j++){
			bit[j] = block_bitmap[k++];
		}
		write_to_disk(orig_sock, bit, n / SECTOR, n % SECTOR);
		n++;
	}
}
void write_to_inode_map(int orig_sock){
	int i,j,k;	
	int n = 1 + INBIT + BLBIT;
	for(i = 0;i < IMAP;i++){	// every block has one inode
		char buff[256] = {0};
		if(inode_bitmap[i] == 'n' ){
	    		inode inode1 = inode_map[i]; 	
			char tp1[5],tp2[5],tp3[5],tp4,tp5[5],tp6[27][5],tp7[3];
			char index[108] = {0}; 
			tp7[2] = '\0';
			for(k = 0; k < 27; k++){
				char tmp[5];
				sprintf(tmp,"%4s",i2a(inode1.index[k],tp6[k]));
				strcat(index,tmp);
			}	
	
	sprintf(buff, "%4s%4s%4s%c%4s%108s%3s",i2a(inode1.father_dir,tp1),i2a(inode1.size,tp2),i2a(inode1.number,tp3),inode1.file_or_dir,i2a(inode1.index_num,tp5),index,tp7);
			strcat(buff,"I LOVE I-NODE ~~~ TA IS HANDSOME !");		
			write_to_disk(orig_sock, buff, n / SECTOR, n % SECTOR); n++; 
		}
	}
}
void read_from_inode_map(int orig_sock){
	int i,j;
	int n =  1 + INBIT + BLBIT;
	for(i = 0; i < IMAP ; i++){
		char buf[257] = {0};
		if(inode_bitmap[i] == 'n'){
			read_from_disk(orig_sock, buf, n / SECTOR, n % SECTOR);
			char father_dir[5]; 				
			char size[5]; 					
			char number[5];					
			char file_or_dir;				
			char index_num[5];				
			char index[27][5];			
			file_or_dir = buf[12];  		
			int k;
			strncpy(father_dir,buf,4);				
			strncpy(size,buf+4, 4);				
			strncpy(number,buf+8,4);
			strncpy(index_num,buf+13,4 );	
			father_dir[4] = '\0'; size[4] = '\0'; number[4] = '\0'; index_num[4] = '\0';

			for(j=0;j<27;j++){
					strncpy(index[j],buf+17+j*4,4);
				index[j][4] = '\0';	
			}

			inode_map[i].father_dir = a2i(father_dir);
			inode_map[i].size = a2i(size);
			inode_map[i].number = a2i(number);
			inode_map[i].file_or_dir = file_or_dir;
			inode_map[i].index_num = a2i(index_num);

			for(j=0;j<27;j++)
				inode_map[i].index[j] = a2i(index[j]);
			n++;
		}
		
	}
}

int find_inode_by_name(int sockfd, char name[], int dir){
	// read dir information find the name of file ,its number is just behind its name
	int i; int num = -1;
	for(i = 0; i < 27; i++){
		if (inode_map[dir].index[i] != -1){
			char buff[300] = {0};
			char *temp[60] = {0};
			read_from_disk(sockfd, buff, inode_map[dir].index[0] / SECTOR, inode_map[dir].index[0] % SECTOR);
			divide(buff,temp);
			int j;
			for (j = 0; temp[j] != (char *)0 ; j++){ 
				if(strcmp(temp[j], name) == 0)  num = atoi(temp[j+1]);
				j++;
			}
		}	
		else break;
	}

	return num;
}

int divide(char *line, char *command_array[]) {
	char *p;
	int count=0;
	p = strtok(line, " "); 
	while (p && strcmp(p,"|")!=0) {
		if(*p == ' ') p++;
		command_array[count] = p;
		count++;
		p = strtok(NULL," ");
	}
	return count;
}

int find_next_inode(){ // find the next avaliable inode
	int i = -1;
	for(i = current_inode; i < INBIT * 256; i++)
		if(inode_bitmap[i] == 'y'){
			current_inode = i;
			return i;
		}
	for(i = 0; i < current_inode; i++)
		if(inode_bitmap[i] == 'y'){
			current_inode = i;
			return i;
		}
	return -1;
}

int find_next_block(){ // find the next avaliable block
	int i = -1;
	for(i = current_block; i < BLBIT * BLOCK_SIZE; i++)
		if(block_bitmap[i] == 'y'){
			current_block = i;
			return i;
		}
	for(i = 0; i < current_block; i++)
		if(block_bitmap[i] == 'y'){
			current_block = i;
			return i;
		}
	return -1;
}

char write_to_file(int inode_num, int len, char *data, int orig_sock){
	if(inode_bitmap[inode_num]=='y')
		return 0;
	else{
		clean_file(inode_num);  
		inode_map[inode_num].size = len;
		inode_map[inode_num].index_num = 0;
		int index_num;
		int index_len = len/256 + (len%256 != 0); // the index we need
		int next_block;
		int next_inode;
		char buf[256] = {0};
		int x = 0; 
		for(index_num = 0;index_num < 20 && index_len > 0; index_num++){  //while not empty
			next_block = find_next_block();
			if(next_block == -1)
				return 0;
			current_block = next_block;
			inode_map[inode_num].index[index_num] = next_block;
			inode_map[inode_num].index_num++;
			if(index_len > 1){
				buf[0] = '\0';
				strncpy(buf,data+x, 256);
				buf[255] = '\0';
				write_to_disk(orig_sock, buf, next_block / SECTOR, next_block % SECTOR);
				block_bitmap[next_block] = 'n';
			}
			else{
				buf[0] = '\0';
				strncpy(buf,data+x, 256);
				buf[255] = '\0';
				write_to_disk(orig_sock, buf, next_block / SECTOR, next_block % SECTOR);
				block_bitmap[next_block] = 'n';
				return 1; // empty
			}
			x += 256; // copy 256bytes once
			index_len--;
		}

		int i,j; 

		//single directory

		for(index_num;index_num < 26 && index_len > 0 ;index_num++){
			next_inode = find_next_inode();
			if(next_inode == -1)
				return 0;
			current_inode = next_inode;
			inode_bitmap[next_inode] = 'n';
			inode second_inode;
			second_inode.index_num = 0;
			for(i = 0; i < 10 && index_len > 0;i++){
				next_block = find_next_block();
				if(next_block == -1)
					return 0;
				current_block = next_block;
				second_inode.index[i] = next_block;
				second_inode.index_num++;
				if(index_len > 1){
					buf[0] = '\0';
					strncpy(buf,data+x, 256);
					//buf[255] = '\0';
					write_to_disk(orig_sock, buf, next_block / SECTOR, next_block % SECTOR);
					block_bitmap[next_block] = 'n';
				}
				else{
					buf[0] = '\0';
  					strncpy(buf,data+x, 256);
					//buf[255] = '\0';
					write_to_disk(orig_sock, buf, next_block / SECTOR, next_block % SECTOR);
					block_bitmap[next_block] = 'n';
				}
				x += 256;
				index_len--;
				inode_map[next_inode] = second_inode;
			}
		}
	
		// double directory
		if(index_len > 0){
			for(i=0;i<27;i++){ //use inode as its block
				next_inode = find_next_inode();
				if(next_inode == -1)
					return -1;
				current_inode = next_inode;
				inode_bitmap[next_inode] = 'n';
				inode third_inode_1; 
				third_inode_1.index_num = 0;
				for(j = 0; j < 27; j++){
					next_block = find_next_block();
					if(next_block == -1)
						return -1;
					current_block = next_block;
					third_inode_1.index[j] = next_block;
					third_inode_1.index_num++;
					if(index_len > 1){
						buf[0] = '\0';
						strncpy(buf,data+x, 256);
						write_to_disk(orig_sock, buf, next_block / SECTOR, next_block % SECTOR);
						block_bitmap[next_block] = 'n';
					}
					else
					{
						buf[0] = '\0';
						strncpy(buf,data+x, 256);
						write_to_disk(orig_sock, buf, next_block / SECTOR, next_block % SECTOR);
						block_bitmap[next_block] = 'n';
						return 1;
					}
					x += 256;
					index_len--;
					inode_map[next_inode] = third_inode_1;
				}			
			}

		}
	}
	return 1;
}

char clean_file(int inode_num){ // change the bit information of the file
	if(inode_bitmap[inode_num] == 'y')
		return 0;
	else{
		printf("cleaning ...\n");
		inode temp_inode = inode_map[inode_num];//extract the current inode
		if(temp_inode.index_num == 0)
			return 1;
		int i;
		for(i=0;i < 20;i++){ // first pointer
			if( inode_map[inode_num].index[i] == -1 )
				return 1;
			else
				block_bitmap[temp_inode.index[i]] = 'y';
		}
		for(i = 20;i < 26; i++){
			inode_bitmap[temp_inode.index[i]] = 'y';
			printf("cleaning double...\n");
			int j;
			int inode_num = temp_inode.index[i];
			if (inode_num != -1)
				for(j = 0; j < 27; j++){
					if(inode_map[inode_num ].index[j] != 'y')
						block_bitmap[inode_map[inode_num ].index[j]] = 'y';
				}
			}
		
		if (temp_inode.index[26] != -1){
			inode_bitmap[temp_inode.index[26]] = 'y'; //指向11个inode 
			inode second =  inode_map[temp_inode.index[10]];
			int j;
			for(j = 0; j <= 26; j++){  //各指向一个inode
				if(second.index[j] != -1){
					inode_bitmap[second.index[j]] = 'y';
					int k; printf("cleaning triple...\n");
					for(k = 0; k <= 26; k++){
						if(inode_map[second.index[j]].index[k] != -1)
							block_bitmap[inode_map[second.index[j]].index[k]] = 'y';
					}
				}
			}
		}

	}
	return 1;
}

void remove_name(int current,int  orig_sock, char name[]){
	// 读取block信息，依次读，当在那个block中找到文件名时，将最后一个block中的文件名和inode信息复制过去，并删除最后一个
	char buff[261] = {0};
	
/*	if( inode_map[current].size == 1){
		printf("size == 1\n");
		inode_map[current].size--;
		buff[0] = '\0';
 		write_to_disk(orig_sock, buff, inode_map[current].index[0] / SECTOR, inode_map[current].index[0] % SECTOR);
		return ;
	} */
	printf("size--ing\n");
	inode_map[current].size--;
	int j; int i;
	char *temp[32] = {0};
	int lastn; int p = -1;
	char last[16] = {0};
	char write_back[256] = {0};
	int q = 1;
	for (j = 0; j < 27 && q == 1; j++){
		if (inode_map[current].index[j] == -1 ) break;
		bzero(buff,261);
		if  (inode_map[current].index[j+1] == -1 ){
			char *temp[32] = {0};
			read_from_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
			divide(buff,temp);

			for (i = 0; temp[i] != (char *)0 ; i++){ 
				if(temp[i+2] == (char *)0  && temp[i+1] != (char *)0) lastn = i ; //locate the last file name
			}

			strcat(last, temp[lastn]);
			strcat(last, " ");
			char node_num[4];
			sprintf(node_num,"%s",temp[lastn+1]);
			strcat(last, node_num);
			strcat(last, " ");
			while(strlen(last) < 16) strcat(last," ");
			last[15] = '\0'; 
			//copy last file name to last
	
			for(i = 0; i < lastn ; i++){
				char a[16] = {0};
				strcat(a, temp[i]);
				strcat(a, " ");
				strcat(a, temp[++i]);
				strcat(a, " ");
				while(strlen(a) < 16) strcat(a," ");
				strcat(write_back , a);
			}
			for(i = 0; i < 16; ++i) strcat(write_back , " "); // append " " write back to directory
			//printf("write_back =  %s\n", write_back);
			write_to_disk(orig_sock, write_back, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
			if(strcmp(temp[lastn], name) == 0) return ;
			q = 0;
			break;
		}
	}
	bzero(write_back,256);
		// replace the filename by last
	for (j = 0; j < 11; j++){

		bzero(buff,261);
		char *temp[32] = {0};
		read_from_disk(orig_sock, buff, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
		divide(buff,temp);
		for (i = 0; temp[i] != (char *)0 ; i++){ 
				if(strcmp(temp[i], name) == 0) { p = i; break;}
			}
		
		if(p != -1){
			for(i = 0; i < p ; i++){
					char a[16] = {0};
					strcat(a, temp[i]);
					strcat(a, " ");
					strcat(a, temp[++i]);
					strcat(a, " ");
					while(strlen(a) < 16) strcat(a," ");
					strcat(write_back , a);
				}
			strcat(write_back,last);
			for(i = p + 2; temp[i] != (char *)0 ; i++){
					char a[16] = {0};
					strcat(a, temp[i]);
					strcat(a, " ");
					strcat(a, temp[++i]);
					strcat(a, " ");
					while(strlen(a) < 16) strcat(a," ");
					strcat(write_back , a);
			//printf("temp[%d] =  %s\n",i, temp[i]);
				}
			
			write_to_disk(orig_sock, write_back, inode_map[current].index[j] / SECTOR, inode_map[current].index[j] % SECTOR);
			break;
		}

	}

}

void remove_dir(int current, char name[], int orig_sock){
	int i; int j;
	int num = find_inode_by_name(orig_sock, name, current);
	char buff[261] = {0};
	if (inode_map[num].size > 0){
	for (j = 0; j < 27 ; j++){
		if (inode_map[num].index[j] == -1 ) break;
		bzero(buff,261);
		if  (inode_map[num].index[j] != -1 ){
			char *temp[32] = {0};
			read_from_disk(orig_sock, buff, inode_map[num].index[j] / SECTOR, inode_map[num].index[j] % SECTOR);
			divide(buff,temp);
			for (i = 0; i < inode_map[num].size * 2 ; i++){ 
				int nums = find_inode_by_name(orig_sock, temp[i++], num);
				if(inode_map[nums].file_or_dir == 'f')
					clean_file(nums);
					
				else {
					 remove_dir(nums, temp[i-1], orig_sock); // recursive call 
					inode_bitmap[nums] = 'y';
					for(i=0;i < 27;i++){ // first pointer
							if (inode_map[nums].index[i] != -1)
								block_bitmap[inode_map[nums].index[i]] = 'y';
						}
					}
				}


		}
	}
}

}


char* i2a(int num,char* a){ //change int to char
	int i;
	for(i=0;i<4;++i){
		a[3-i]=num%95+32;
		num/=95;
	}
	a[4]=0;
	return a;
}


int a2i(char*a){  // change char to int
	int num=0,i;
	for(i=0;i<4;++i){
		num*=95;
		num+=a[i]-32;
	}
	return num;
}


