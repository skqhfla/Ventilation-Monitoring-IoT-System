#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include "MQTTClient.h"
#include "MQTTAsync.h"
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

char url[] = "ws://broker.hivemq.com:8000";
MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
MQTTClient_connectOptions conn_opts_sync = MQTTClient_connectOptions_initializer;
MQTTClient_message pub_msg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;

MQTTAsync client;
MQTTClient client_sync;
pthread_mutex_t mutex;

typedef struct node { 
	int idx; //topic index
	char* name; //topic name
	struct node *next;
} node;
node topic = {-1, NULL, NULL}; //linked list for topics

char *serial;

node* end_node(node* cur){ //return the last node of node list
	while(cur->next){
		cur = cur->next;
	}
	return cur;
}

node* find_node(node* cur, char* topic_n){ //find the node with topic anem and return that node
	while(strcmp(cur->name, topic_n) != 0){
		cur = cur->next;
		if(cur == NULL){
			printf("topic is not initialized\n");
			cur = topic.next;
			while(cur){
				cur = cur->next;
			}
			exit(1);
		}
	}
	return cur;
}

int th_node;
int co2_node;
const int INF = 987654321;
int** co2; 
double** t;
double** h;

char inference_name[255];

int is_ready() { //check if the array is full with value
	for(int i = 0; i < th_node; i++){
		if(t[i][5] == INF || h[i][5] == INF)
			return 0;
	}
	for(int i = 0; i < co2_node; i++){
		if(co2[i][5] == INF)
			return 0;
	}

	return 1;
}

void timeout_handler(int sig){ //timeout handler for alarm

	char pub_topic[255];
	pub_msg.payload = "Open window!";
	pub_msg.payloadlen = strlen("Open window!");
	pub_msg.qos = 1;
	pub_msg.retained = 0;
	sprintf(pub_topic, "%s/alarm", serial);
	MQTTClient_publishMessage(client_sync, pub_topic, &pub_msg, &token);
	
}

void send_ack(node* head){ //send ack to all topic
	node* cur = head->next;
	char pub_topic[255];
	char* node_serial;
	while(cur != NULL){
		char temp[256];
		strcpy(temp, cur->name);
		strtok(temp, "/");
		node_serial = strtok(NULL, "/"); 
		pub_msg.payload = "ACK";
		pub_msg.payloadlen = strlen("ACK");
		pub_msg.qos = 1;
		pub_msg.retained = 0;
		sprintf(pub_topic, "%s/ACK/%s", serial, node_serial);
		MQTTClient_publishMessage(client_sync, pub_topic, &pub_msg, &token);
		cur = cur->next;
	}
}

char* stopstring;
struct itimerval timer;
int openness = -1;
int arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message){ //callback function
	pthread_mutex_lock(&mutex);

	int idx;
	if(strstr(topicName, "co2")){ //get co2 message
		idx = find_node(topic.next, topicName)->idx;
		co2[idx][0] = atoi(message->payload);
	}
	else if(strstr(topicName, "th")){ //get temperature, humid message
		idx = find_node(topic.next, topicName)->idx;
		char temp_str[5] = {0x0, };
		char humid_str[5] = {0x0, };
		char* payload = (char *)(message->payload);
		strncpy(temp_str, payload, 4);
		strncpy(humid_str, payload + 5, 4);
		
		t[idx][0] = (double)strtof(temp_str, &stopstring);
		h[idx][0] = (double)strtof(humid_str, &stopstring);
	}
	else{ //inference
		if(strcmp(inference_name, topicName) == 0 && strcmp("0", message->payload) == 0) { //inference message [ open -> closed ] : set 30minutes timer
			if(openness != 0){
				signal(SIGALRM, timeout_handler);
				timer.it_value.tv_sec = 1800;
				timer.it_interval = timer.it_value;
				setitimer(ITIMER_REAL, &timer, 0x0);
				openness = 0;
			}
		}
		else{ //inference message [ closed -> open ] : unset timer
			if(openness != 1){
				timer.it_value.tv_sec = 0;
				timer.it_interval = timer.it_value;
				openness = 1;
			}
		}

		MQTTAsync_freeMessage(&message);
		MQTTAsync_free(topicName);

	}
	pthread_mutex_unlock(&mutex);
	return 1;
}

void sub_sensor() { //subscribe topics (Async)
	int rc;
	char client_name[255];
	sprintf(client_name, "%s/service_sync", serial);

	MQTTClient_create(&client_sync, url, client_name, MQTTCLIENT_PERSISTENCE_NONE, NULL);

	if((rc = MQTTClient_connect(client_sync, &conn_opts_sync)) != MQTTCLIENT_SUCCESS){
		printf("Sync Could not connect to Broker with return code %d\n", rc);
		pthread_exit((void *)-1);
	}
	send_ack(&topic);
	sprintf(client_name, "%s/service_async", serial);

	MQTTAsync_create(&client, url, client_name, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_setCallbacks(client, NULL, NULL, arrived, NULL);
	conn_opts.context = client;
	opts.context = client;

	if((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS){
		printf("Async Could not connect to Broker with return code %d\n", rc);
		pthread_exit((void *)-1);
	}

	node* cur = topic.next;
	while(cur){
		while(MQTTAsync_subscribe(client, cur->name, 1, &opts) != MQTTCLIENT_SUCCESS){}
		cur = cur->next;
	}
	while(MQTTAsync_subscribe(client, inference_name, 1, &opts) != MQTTCLIENT_SUCCESS){}


	while(1){}
}

void* array_write() { 

	while(1){
		sleep(15);
		
		for(int i = 5; i > 0; i--){ //every 15seconds, move the values in array to next
			for(int j = 0; j < th_node; j++){
				t[j][i] = t[j][i - 1];
				h[j][i] = h[j][i - 1];
			}
			for(int j = 0; j < co2_node; j++){
				co2[j][i] = co2[j][i - 1];
			}
		}

		if(is_ready() == 0) { //check if data array is full with sensor value
			continue;
		}

		char value[1000] = "";
		pthread_mutex_lock(&mutex);

		node* cur = topic.next;
		while(cur != NULL){ //for all nodes in topic list, get the diffrence with adjacent value and make them one sentence
			if(strstr(cur->name, "co2") == NULL){
				for(int i = 3; i > -1; i--){
					if(strlen(value) == 0){
						sprintf(value, "%.2f", fabs(t[cur->idx][i + 2] - t[cur->idx][i + 1]));
					}
					else{
						sprintf(value, "%s %.2f", value, fabs(t[cur->idx][i + 2] - t[cur->idx][i + 1]));
						sprintf(value, "%s %.2f", value, fabs(h[cur->idx][i + 2] - h[cur->idx][i + 1]));
					}
				}
			}
			else{
				for(int i = 3; i > -1; i--){
					if(strlen(value) == 0){
						sprintf(value, "%d", co2[cur->idx][i + 2] - co2[cur->idx][i + 1]); 
					}
					else{
						sprintf(value, "%s %d", value, co2[cur->idx][i + 2] - co2[cur->idx][i + 1]); 
					}
				}
			}
			cur = cur->next;
		}
		cur = topic.next;
		while(cur != NULL){ //for all nodes in topic list, get the diffrence with current value and add it to above sentence
			if(strstr(cur->name, "co2") == NULL){
				for(int i = 0; i < 4; i++){
					sprintf(value, "%s %.2f", value, fabs(t[cur->idx][1] - t[cur->idx][i + 2]));
					sprintf(value, "%s %.2f", value, fabs(h[cur->idx][1] - h[cur->idx][i + 2]));
				}
			}
			else{
				for(int i = 0; i < 4; i++){
					sprintf(value, "%s %d", value, co2[cur->idx][1] - co2[cur->idx][i + 2]); 
				}
			}
			cur = cur->next;
		}

		pthread_mutex_unlock(&mutex);
		
		pid_t pid = fork();

		if(pid < 0) {
			fprintf(stderr, "fork failed\n");
		}
		else if(pid == 0) {
			execlp("/usr/bin/python3","python3", "/home/borim/final_iot/final_server/flexible_node_version/randomforest_server_classifier_inference.py", value, serial, NULL); //execute inference program
		}
		else {
			wait(NULL);
			printf("inference executed\n");
		}

	}
}



int main(int argc, char* argv[]){
	
	serial = (char *)malloc(strlen(argv[0]) + 1); //get the serial
	strcpy(serial, argv[0]);
	sprintf(inference_name, "%s/inference", serial);

	char topic_name[255];
	FILE *fp = fopen("node_list.txt", "r");

	while(1){ //get the topics and make topic list
		fgets(topic_name, sizeof(topic_name), fp);
		topic_name[strlen(topic_name) - 1] = 0;
		if(feof(fp)){
			break;
		}
		node* n = (node *)malloc(sizeof(node));
		n->next = NULL;
		n->name = (char *)malloc(strlen(topic_name));
		strcpy(n->name, topic_name);
		if(strstr(n->name, "co2")){
			n->idx = co2_node++;
		}
		else{
			n->idx = th_node++;
		}
		end_node(&topic)->next = n;
	}


	co2 = (int **)malloc(sizeof(int *) * co2_node);
	for(int i = 0; i <co2_node; i++) {
		co2[i] = (int *)malloc(sizeof(int) * 6);
	}
	t = (double **)malloc(sizeof(double *) * th_node);
	h = (double **)malloc(sizeof(double *) * th_node);
	for(int i = 0; i<th_node; i++) {
		t[i] = (double *)malloc(sizeof(double) * 6);
		h[i] = (double *)malloc(sizeof(double) * 6);
	}
	
	for(int i = 0; i<th_node; i++) {
		for(int j=0; j<6; j++){
			h[i][j] = INF;
			t[i][j] = INF;
		}
	}
	for(int i = 0; i<co2_node; i++) {
		for(int j=0; j<6; j++){
			co2[i][j] = INF;
		}
	}
	
	printf("service start\n");
	conn_opts.cleansession = 1;

	pthread_t array_write_thread;
	pthread_create(&array_write_thread, NULL, array_write, NULL);

	pthread_mutex_init(&mutex, NULL);
	sub_sensor();

	pthread_join(array_write_thread, (void *)NULL);

	pthread_mutex_destroy(&mutex);

	free(serial);
	node* cur = topic.next;
	node* temp;
	while(cur != NULL){
		temp = cur;
		cur = cur->next;
		free(temp->name);
		free(temp);
	}
	for(int i = 0; i<co2_node; i++) {
		free(co2[i]);
	}
	free(co2);
	for(int i = 0; i<th_node; i++) {
		free(t[i]);
		free(h[i]);
	}
	free(t);
	free(h);

	return 0;
}

