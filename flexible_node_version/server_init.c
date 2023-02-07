#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include "MQTTClient.h"
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdbool.h>

#define QOS 1

//when execute this file, you must put two serial number (server serial, CO2 node serial)
//ex) ./server_init Arise co2_1

char url[] = "ws://broker.hivemq.com:8000"; //mqtt hivemq broker url <socket://broker url:socket number>
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
MQTTClient_deliveryToken token;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient client_start;

typedef struct NODE{
	char *node_name;
	struct NODE *next;
}node_list;

node_list* head;
node_list* check;

pthread_mutex_t mutex;

char *msg = "ACK";
char id[20];
char topic[20];
char serial[10];
char node_serial[10];
FILE *init;

bool init_start = false;

int start_arrived(void * context, char * topicName, int topicLen, MQTTClient_message * message) {

	if(strcmp("start service", message->payload) == 0){
		execl("/home/borim/final_iot/final_server/flexible_node_version/server_service", serial, 0x0); //start server_service
	}
	else if(strcmp("done init", message->payload) == 0){
		pthread_mutex_lock(&mutex);
		//write node_list.txt about node linked list
		init = fopen("/home/borim/final_iot/final_server/flexible_node_version/node_list.txt", "w");
		node_list* write = head;

		while(write->next != NULL){
			fprintf(init, "%s/%s\n", serial, write->next->node_name);
			write = write->next;
		}
		
		write = head;

		while(write->next != NULL){
			node_list* temp = write->next;
			write->next = write->next->next;
			free(temp->node_name);
			free(temp);
		}

		pthread_mutex_unlock(&mutex);
		fclose(init);
		remove("/home/borim/final_iot/final_server/flexible_node_version/dataset_sub.txt"); // remove dataset file 
		execl("/home/borim/final_iot/final_server/flexible_node_version/server_learn", serial, 0x0); //start server_learn
	}
	else if(strcmp("start init", message->payload) == 0){ //node init start
		init_start = true;
	}
	else if(init_start){
		node_list* node = (node_list*)malloc(sizeof(node_list));
		node->node_name = (char*)malloc(strlen(message->payload) + 1);
		strcpy(node->node_name, message->payload);
		node->next = NULL;

		pthread_mutex_lock(&mutex);
		node_list* curr = check;

		while(curr->next != NULL){
			curr = curr->next;
		}

		curr->next = node;

		pthread_mutex_unlock(&mutex);

		printf("recv node serial %s\n", message->payload);
	}

	MQTTClient_freeMessage(&message);
	MQTTClient_free(topicName);
	return 1; 
}

void register_sub(){	

	MQTTClient_create(&client_start, url, id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks(client_start, NULL, NULL, start_arrived, NULL);
	int rc;
	if((rc = MQTTClient_connect(client_start, &conn_opts)) != MQTTCLIENT_SUCCESS){
		while(rc != MQTTCLIENT_SUCCESS){
			rc = MQTTClient_connect(client_start, &conn_opts);
		}
	}
	printf("connect\n");

	rc = MQTTClient_subscribe(client_start, topic, QOS);

	pid_t pid = fork();
	
	if(pid < 0){
		fprintf(stderr, "fork failed\n");
		exit(-1);
	}
	else if(pid == 0)
		execl("/home/borim/final_iot/final_server/flexible_node_version/indoor_co2", serial, node_serial, 0x0); //start CO2 sensor
	else{
	}
}

void * send_ack(){
	while(1){
		sleep(5);
		pthread_mutex_lock(&mutex);
		node_list* curr = check;
		node_list* reghead = head;

		if(curr->next != NULL){
			pubmsg.payload = msg;
			pubmsg.payloadlen = strlen(msg);
			pubmsg.qos = QOS;
			pubmsg.retained = 0;

			char dest[100];
			sprintf(dest, "%s/ACK/%s", serial, curr->next->node_name);
			MQTTClient_publishMessage(client_start, dest, &pubmsg, &token);
			printf("send ACK to %s\n", curr->next->node_name);
			
			while(reghead->next != NULL){
				if(strcmp(reghead->next->node_name, curr->next->node_name) == 0)
					break;

				reghead = reghead->next;
			}

			if(reghead->next == NULL){
				node_list* temp = curr->next;
				curr->next = curr->next->next;
				reghead->next = temp;
				temp->next = NULL;
			}
			else{
				node_list* temp = curr->next;
				curr->next = curr->next->next;
				free(temp->node_name);
				free(temp);
			}

		}
		pthread_mutex_unlock(&mutex);
	}
}

int main(int argc, char* argv[]){
	

	pthread_t publish;
	strcpy(serial, argv[1]);
	sprintf(topic, "%s/init", serial);
	sprintf(id, "%s/server_init", serial);
	strcpy(node_serial, argv[2]);
	
	pthread_mutex_init(&mutex, NULL);
	
	head = (node_list*)malloc(sizeof(node_list));
	check = (node_list*)malloc(sizeof(node_list));

	head->next = NULL;
	check->next = NULL;

	pthread_create(&publish, NULL, send_ack, NULL);

	conn_opts.cleansession = 1;
	conn_opts.keepAliveInterval = 900;

	register_sub();
	
	while(1){}

	return 0;
}

