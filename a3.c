#include <sys/shm.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define CONNECT "CONNECT"
#define SHM_KEY 19141
#define CREATE_SHM "CREATE_SHM"
#define ERROR "ERROR"
#define SUCCESS "SUCCESS"
#define WRITE_TO_SHM "WRITE_TO_SHM"
#define MAP_FILE_NAME "MAP_FILE"
#define READ_FROM_FILE_OFFSET "READ_FROM_FILE_OFFSET" 
#define READ_FROM_FILE_SECTION "READ_FROM_FILE_SECTION"
#define READ_FROM_LOGICAL_SPACE_OFFSET "READ_FROM_LOGICAL_SPACE_OFFSET"

int fd1 = -1, fd2 = -2 ,fd3 = -1, shmId;
unsigned int size_shm ,value, noBytes;

unsigned int  sectionNo;
off_t  offset,  offset1, logical_offset, file_size ;

char size_error = 5;
char size_read_from_off = 21;
char size_success = 7;
char size_read_section = 22;
char size_read_logical = 29;

char filename[256];
char* mapped = NULL;
char* sharedMem = NULL;

void writeConnect(){
	//char size_connect = 8;
	char c[8];
	int j;
	c[0]= 0x07; c[1] = c[6] = 0x43; c[2] = 0x4f;
	c[3] = c[4] =  0x4E; c[5] = 0x45;
	c[7] = 0x54;

	write(fd1, &c[0], 1);
	
	for(j = 1; j < 8; j++){
		write(fd1, &c[j], 1);
	}
}


void writeSucces(){
	char c[8];
	int j;
	c[0]= 0x07; c[1] = c[7] = c[6] = 0x53;
	 c[2] = 0x55;
	c[3] = c[4] =  0x43; c[5] = 0x45;

	write(fd1, &c[0], 1);
	for(j = 1; j < 8; j++){
		write(fd1, &c[j], 1);
	}
}

void writeError(){
	char c[6];
	int j;
	c[0]= 0x05; c[1] = 0x45; 
	 c[2]= c[3] = 0x52 ;c[4] =  0x4f; c[5] = 0x52;

	write(fd1, &c[0], 1);
	for(j = 1; j < 6; j++){
		write(fd1, &c[j], 1);
	}

}
void respondToPing()
{
	char size = 4;
	int i;
	char bytes[4];	
	bytes[0] = 187;
	bytes[1] = 187;
	bytes[2] = 0;
	bytes[3] = 0;

	write(fd1, &size, 1);
	write(fd1, "PING", size);

	write(fd1, &size, 1);
	write(fd1, "PONG", size);

	for(i = 0; i < 4; i++)
	{
		write(fd1, &bytes[i], 1);
	}
}

void createSharedMem()
{

	char size_create = 10;

	read(fd2, &size_shm, 4);

	shmId = shmget(SHM_KEY, size_shm, IPC_CREAT | 0664);
	if(shmId < 0) 
	{
		writeError();
		return ;
	}
	
	sharedMem = (char*)shmat(shmId, NULL, 0);
	write(fd1, &size_create, 1);
	write(fd1, CREATE_SHM, size_create);
	
	if(sharedMem == (void*)-1)
	{
		writeError();
	}else
	{
		writeSucces();
	}
	

}


void writeToSharedMem()
{
	char size_write = 12;
	write(fd1, &size_write, 1);
	write(fd1, WRITE_TO_SHM, size_write);

	if(!(offset >= 0 && offset <= size_shm) || (offset  + value > size_shm))
	{
		writeError();
		exit(4);
	}else 
	{
		*(int *)(sharedMem + offset) = value;
				
		writeSucces();
	}

}


void mapFile()
{
	char size_map = 8;
	
	fd3 = open(filename, O_RDONLY);
	
	if( fd3 == -1 || fd3 == 0)
	{
		write(fd1, &size_map, 1);
		write(fd1, MAP_FILE_NAME, size_map);

		writeError();

		return ;
	}

	file_size = lseek(fd3, 0, SEEK_END);
	lseek(fd3, 0,SEEK_SET);

	mapped = (char*) mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd3, 0);
	write(fd1, &size_map, 1);
	write(fd1, MAP_FILE_NAME, size_map);
	if( mapped == (void*)-1)
	{
		writeError();
		exit(5);	
	}else 
	{
		writeSucces();
	}

}

void readFromFileOffset()
{
	int of;
	unsigned int b = 0;
	
	if(offset1 + noBytes > file_size)
	{
		
		write(fd1, &size_read_from_off, 1);
		write(fd1, READ_FROM_FILE_OFFSET, size_read_from_off);

		writeError();
		return;
	}
	
	for( of = offset1; of <= offset1 + noBytes; of++)
	{
		
		sharedMem[b] = mapped[of];
		b++;
	
	}
	
	write(fd1, &size_read_from_off, 1);
	write(fd1, READ_FROM_FILE_OFFSET, size_read_from_off);

	writeSucces();
	
}

void readFromSectionFile()
{
	int i;
	unsigned int b = 0;
	
	int32_t sect_offset ;
	short *header_size = (short*)&mapped[file_size - 4];
	//char *sectiune = (char *)&mapped[fileSize - header_size - 1];
	unsigned int sect = 0;
	printf("%X \n", *header_size);
	for ( i = ((int32_t)file_size - (int32_t)*header_size) + 5; i < (file_size - 5); i += 25)
	{	
		//printf("iter --> %d\n", i);
		sect_offset = *(int32_t*)&mapped[i + 17];
		if( sect+1 == sectionNo )
		{
			break;
		}
		else{
			sect++;
		}
	}

	if(sect+1 != sectionNo){
		// error
		write(fd1, &size_read_section, 1);
		write(fd1, READ_FROM_FILE_SECTION, size_read_section);

		writeError();

		return;
	}

	
	for(i = (sect_offset + offset); i <= (sect_offset + offset + noBytes); i++)
	{
		sharedMem[b++] = mapped[i];
	}

	write(fd1, &size_read_section, 1);
	write(fd1, READ_FROM_FILE_SECTION, size_read_section);

	writeSucces();
	
}


int main(int argc, char **argv)
{
	//char size_connect = 7 ;
	char size1, name_size;
	unsigned int i, exit = 0;
	char buff1[75];
	memset(buff1, 0, sizeof(buff1));
	
	
	//struct stat buffer;
   // int exist = stat("RESP_PIPE_48059",&buffer);
	if(mkfifo("RESP_PIPE_48059", 0600) != 0)
	{
		printf("ERROR creating FIFO pipe\n");
		return 1;
	}
	

	fd2 = open( "REQ_PIPE_48059", O_RDONLY);
	if(fd2 < 0)
	{
		printf("could not open REQ_PIPE for reading\n");	
		return 1;
	}else 
	{
		printf("REQ_PIPE opened success\n"); 
	}

	fd1 = open("RESP_PIPE_48059", O_WRONLY);
	if(fd1 == -1)
	{
		printf("could not open RESP_PIPE for writing\n");
		return 1;
	}

	/*write(fd1, &size_connect, 1);
	write(fd1, CONNECT, size_connect);
*/
	writeConnect();
	do{
		read(fd2, &size1, 1);
		for(i = 0; i < size1; i++)
		{
			read(fd2, &buff1[i], 1);
		}

		if(strncmp(buff1, "PING", 4) == 0)
		{
			respondToPing();
		}
		else if(strncmp(CREATE_SHM, buff1, 10) == 0)
		{
			createSharedMem();	
		}
		else if(strncmp(WRITE_TO_SHM, buff1, 12) == 0)
		{
			read(fd2, &offset, 4);
			read(fd2, &value, 4);
			writeToSharedMem(sharedMem);
		}
		else if(strncmp(MAP_FILE_NAME, buff1, 8) == 0)
		{
			read(fd2, &name_size, 1);
			for(i = 0; i < name_size; i++)
			{
				read(fd2, &filename[i], 1);
				
			}
			printf("FILENAME %s\n", filename);
			strcat(filename, "\0");
			mapFile();
		}
		else if (strncmp( READ_FROM_FILE_OFFSET, buff1, size_read_from_off) == 0)
		{
			read(fd2, &offset1, 4);
			read(fd2, &noBytes, 4);
			readFromFileOffset();
		}
		else if(strncmp(READ_FROM_FILE_SECTION, buff1, size_read_section) == 0)
		{
			read(fd2, &sectionNo, 4);
			read(fd2, &offset, 4);
			read(fd2, &noBytes, 4);
			readFromSectionFile();

		}else if(strncmp(READ_FROM_LOGICAL_SPACE_OFFSET, buff1,size_read_logical) == 0)
		{
			read(fd2, &logical_offset, 4);
			read(fd2, &noBytes, 4);
			exit = 1;
			continue;
		}
		else if(strncmp("EXIT", buff1, 4) == 0)
		{
			exit = 1;
		}
	}while( exit != 1);

	close(fd1);
	close(fd2);
	close(fd3);
	munmap(mapped, file_size);
	unlink("RESP_PIPE_48059");
	unlink("REQ_PIPE_48059");

	return 0;

}