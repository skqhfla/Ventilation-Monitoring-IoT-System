#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "MQTTClient.h"

#define MAX_TIME 83
#define DHT11PIN 7

char * server_serial;
char * node_serial;
char * url = "ws://broker.hivemq.com:8000";
char init_topic[1024];
char ACK_topic[1024];
char pub_topic[1024];
int ACK_arrived = 0;
int dht11_val[5] = {0,0,0,0,0};
int dht11_temp[5] = {0,0,0,0,0};
float farenheit_temp;
MQTTClient client;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;
pthread_mutex_t mutex;

void dht11_read_val()
{

    uint8_t lststate = HIGH;
    uint8_t counter =0;
    uint8_t j=0, i, k;
    float farenheit;
    
    for(k = 0; k < 5; k++)
	dht11_val[k] = 0;

    pinMode(DHT11PIN, OUTPUT);
    digitalWrite(DHT11PIN, 0);
    delay(18);
    digitalWrite(DHT11PIN, 1);
    delayMicroseconds(40);
    pinMode(DHT11PIN, INPUT);
    
    for( i=0 ; i<MAX_TIME ; i++) {
        counter =0;
        while(digitalRead(DHT11PIN) == lststate) {
            counter++;
            delayMicroseconds(1);
            if(counter == 255)
                break;
        }
    lststate = digitalRead(DHT11PIN);
    if(counter == 255)
        break;
    if((i>=4) && (i%2 ==0)) {
        dht11_val[j/8] <<= 1;
                            
        if(counter > 26){
            dht11_val[j/8] |= 1;
        }
        j++;
      }
    }
    
  char humid[100];
  char temp[100];
  if((j>=40)&&(dht11_val[4] == ((dht11_val[0] + dht11_val[1] + dht11_val[2] + dht11_val[3]) & 0xFF))) {	 
      farenheit = dht11_val[2]*9./5.+32;
      sprintf(temp, "%d.%d %d.%d", dht11_val[2], dht11_val[3], dht11_val[0], dht11_val[1]);
      printf("send(%s), th: %s \n", node_serial, temp);
      for(i=0 ; i<5; i++) {
          dht11_temp[i] = dht11_val[i];
      }
      farenheit_temp = farenheit;
      pubmsg.payload = temp;
      pubmsg.payloadlen = strlen(temp);
      pubmsg.qos = 1;
      pubmsg.retained = 0;
      MQTTClient_publishMessage(client, pub_topic, &pubmsg, &token);
  }

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

void connect_server() {
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

	int rc = MQTTClient_create(&client, url, node_serial, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks(client, NULL, NULL, ack_arrived_callback, NULL);

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
		while(rc != MQTTCLIENT_SUCCESS) {
			rc = MQTTClient_connect(client, &conn_opts);
		}
   	 }
	rc = MQTTClient_subscribe(client, ACK_topic, 1);
}

int main(int argc, char ** argv) {
	
	pthread_mutex_init(&mutex, NULL);

	server_serial = argv[0];
	node_serial = argv[1];	

	get_server_serial();
	connect_server();

	while(1) {	
		sleep(5);
		
		pthread_mutex_lock(&mutex);

		if(ACK_arrived == 1) {
			pthread_mutex_unlock(&mutex);
			printf("%s : ack arrived from server init\n", node_serial);
			break;
		}
		pthread_mutex_unlock(&mutex);

		printf("%s : send init topic to server init\n", node_serial);
		pubmsg.payload = node_serial;
     		pubmsg.payloadlen = strlen(node_serial);
     		pubmsg.qos = 1;
    		pubmsg.retained = 0;
    		MQTTClient_publishMessage(client, init_topic, &pubmsg, &token);
	}
        
    	if(wiringPiSetup() == -1 ) {
       		printf("return -1 error");    
     	  	return -1;
    	}

   	while(1) {
        	dht11_read_val();
    	   	sleep(1);
   	}
    
  	return 0;
}
