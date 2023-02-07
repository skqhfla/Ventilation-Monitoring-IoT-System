#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <sys/wait.h>

// ./start_node server_serial th_serial co2_serial
// Then enter the server serial number

int main(int argc, char ** argv) {
	
	char * server_serial = argv[1];
	char * th_serial = argv[2];
	char * co2_serial = argv[3];
	
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed\n"); 
		return 1;
	}
       	else if (pid == 0) { // child process 
		execl("/home/luiheid/final_iot/flexible_node/indoor_co2", server_serial, co2_serial, 0x0);
	} else {
	}

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed\n"); 
		return 1;
	}
       	else if (pid == 0) { // child process 
		execl("/home/luiheid/final_iot/flexible_node/indoor_th", server_serial, th_serial, 0x0);
	} else {
		wait(NULL);
	}

	return 0;

}

