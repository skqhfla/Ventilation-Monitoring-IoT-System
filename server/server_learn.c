#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"
#include <sys/wait.h>

char url[] = "ws://broker.hivemq.com:8000";
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

char * serial;

int start_arrived(void * context, char * topicName, int topicLen, MQTTClient_message * message) { //callback function : mqtt message arrived
	char * payload_command = (char *)(message->payload);

	if(*(payload_command) == 'd') { //dataset command
		pid_t pid = fork();
		if(pid < 0) {
			fprintf(stderr, "fork failed!\n");
			exit(1);
		}	
		if(pid == 0) {
			char *label = strtok(message->payload, " ");
			label = strtok(NULL, " ");
			execl("/home/borim/final_iot/final_server/flexible_node_version/dataset", label, serial, 0x0); //execute dataset program
		}
		else {
			printf("%s command arrived!\n", message->payload);
			wait(NULL);
		}
	}	
	else if(*(payload_command) == 'l') { //learning command
		pid_t pid = fork();
		if(pid < 0) {
			fprintf(stderr, "fork failed!\n");
			exit(1);
		}	
		if(pid == 0) {
			execlp("/usr/bin/python3","python3", "/home/borim/final_iot/final_server/flexible_node_version/randomforest_server_classifier_train.py", serial, 0x0); //execute rainforest machine learning program
		}
		else {
			printf("training start\n");
			wait(NULL);			

			execl("/home/borim/final_iot/final_server/flexible_node_version/server_service", serial, 0x0); //after learning, execute service program
		}
	}

	MQTTClient_freeMessage(&message);
	MQTTClient_free(topicName);
	return 1; 
}

void register_subscribe(){ //subscribe topics
	MQTTClient client_start;
	MQTTClient_create(&client_start, url, serial, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks(client_start, NULL, NULL, start_arrived, NULL);
	int rc;
	if((rc = MQTTClient_connect(client_start, &conn_opts)) != MQTTCLIENT_SUCCESS){
		while(rc != MQTTCLIENT_SUCCESS){
			rc = MQTTClient_connect(client_start, &conn_opts);
		}
	}
	char topic[255];
	sprintf(topic, "%s/%s", serial, "start");
	rc = MQTTClient_subscribe(client_start, topic, 1);
}

int main(int argc, char *argv[]){

	serial = (char *)malloc(strlen(argv[0]) + 1); //get the serial code
	strcpy(serial, argv[0]);

	conn_opts.cleansession = 1;
	conn_opts.keepAliveInterval = 900;
	printf("server learn start\n");

	register_subscribe();
	
	while(1) {}

	return 0;
}

