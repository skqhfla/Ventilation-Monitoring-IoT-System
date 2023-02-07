#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include "MQTTClient.h"
#include <pthread.h>

#define BUFSIZE 1000

pthread_mutex_t mutex;
char * server_serial;
char * node_serial;
char * url = "ws://broker.hivemq.com:8000"; //mqtt hivemq broker url <socket://broker url:port number>
char init_topic[1024];
char ACK_topic[1024];
char pub_topic[1024];
int ACK_arrived = 0;
unsigned char send_data[4] = {0x11, 0x01, 0x01, 0xED};
unsigned char receive_buff[8];
unsigned char recv_cnt = 0;
unsigned int PPM_value;
int serial_port;
char buf[BUFSIZE];

MQTTClient client;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;
int qos = 1;
void send_cmd() {
	for(int i = 0; i < 4; i++) {
		serialPutchar(serial_port, send_data[i]);
		delay(1);
	}
}

unsigned char checksum_cal() {
	unsigned char SUM = 0;
	for(int i = 0; i < 7; i++) {
		SUM += receive_buff[i];
	}
	return 256-SUM;
}

void get_server_serial() {
	
	strcpy(ACK_topic, server_serial);
	strcpy(pub_topic, server_serial);
	strcpy(init_topic, server_serial);

	strcat(ACK_topic, "/ACK/");
	strcat(ACK_topic, node_serial);

	strcat(pub_topic, "/");
	strcat(pub_topic, node_serial);

	strcat(init_topic, "/init");
}

int ack_arrived_callback(void * context, char * topicName, int topicLen, MQTTClient_message * message) {
	
	pthread_mutex_lock(&mutex);

	ACK_arrived = 1;
	pthread_mutex_unlock(&mutex);

	MQTTClient_freeMessage(&message);
	MQTTClient_free(topicName);
	return 1;
}

int main(int argc, char ** argv) {
	pthread_mutex_init(&mutex, NULL);

	server_serial = argv[0];
	node_serial = argv[1];
	get_server_serial();

	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	int rc = MQTTClient_create(&client, url, node_serial, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks(client, 0x0, 0x0, ack_arrived_callback, 0x0);

	if((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
		while(rc != MQTTCLIENT_SUCCESS){
			rc = MQTTClient_connect(client, &conn_opts);
		}
	}
	rc = MQTTClient_subscribe(client, ACK_topic, 1);

	while(1) {
		sleep(5);

		pthread_mutex_lock(&mutex);

		if(ACK_arrived) {
			pthread_mutex_unlock(&mutex);
			printf("%s : ack arrived from server init\n", node_serial);
			break;
		}

		pthread_mutex_unlock(&mutex);
		
		printf("%s : send init topic to server init\n", node_serial);
		pubmsg.payload = node_serial;
		pubmsg.payloadlen = strlen(node_serial);
		pubmsg.qos = qos;
		pubmsg.retained = 0;
		MQTTClient_publishMessage(client, init_topic, &pubmsg, &token);
	}

	char dat;
	if((serial_port = serialOpen("/dev/ttyS0", 9600)) < 0) {
		fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
		return 1;
	}

	if(wiringPiSetup() == -1) {
		fprintf(stderr, "Unable to start WiringPi: %s\n", strerror(errno));
		return 1;
	}

	while(1) {
		send_cmd();
		while(1) {
			if(serialDataAvail(serial_port)) {
				receive_buff[recv_cnt++] = serialGetchar(serial_port);
				if(recv_cnt == 8) {
					recv_cnt = 0;
					break;
				}
			}
		}
		
		if(checksum_cal() == receive_buff[7]) {
			PPM_value = receive_buff[3]<<8 | receive_buff[4];
			sprintf(buf, "%lu", PPM_value);
			buf[4] = 0x0;
			printf("send(%s), co2 %s\n", node_serial, buf);
			
			pubmsg.payload = buf;
			pubmsg.payloadlen = strlen(buf);
			pubmsg.qos = qos;
			pubmsg.retained = 0;
			MQTTClient_publishMessage(client, pub_topic, &pubmsg, &token);
		}
		
		delay(1000);
	}	


	return 0;
}
