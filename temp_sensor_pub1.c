#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <string.h>
#include "MQTTClient.h"
#include <pthread.h>

#define MAX_TIME 83
#define DHT11PIN 7
//~ #define DHT11PIN 25

char server_serial[1024];
char * node_serial = "th_3";
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
      printf("Humidity = %s %% Temperature = %s *C (%.1f *F)\n", humid, temp, farenheit);
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
	printf("Enter the server's serial number: ");
	scanf("%s", server_serial);

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
	int rc = MQTTClient_create(&client, server_serial, node_serial, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks(client, NULL, NULL, ack_arrived_callback, NULL);

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        	printf("Failed to connect, return code %d\n", rc);
		while(rc != MQTTCLIENT_SUCCESS) {
			rc = MQTTClient_connect(client, &conn_opts);
		}
   	 }
	rc = MQTTClient_subscribe(client, ACK_topic, 1);
}

void * sensor_pub(void * param) {
	
	while(1) {
	
		sleep(2);
		
		pthread_mutex_lock(&mutex);

		if(ACK_arrived == 1) {
			pthread_mutex_unlock(&mutex);
			pthread_exit(0);
		}

		pubmsg.payload = node_serial;
     		pubmsg.payloadlen = strlen(node_serial);
     		pubmsg.qos = 1;
    		pubmsg.retained = 0;
    		MQTTClient_publishMessage(client, init_topic, &pubmsg, &token);
	}

	
}


int main(void) {
	
	pthread_t tid_init;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_mutex_init(&mutex, NULL); 

	get_server_serial();
	connect_server();

        
    	if(wiringPiSetup() == -1 ) {
       		printf("return -1 error");    
     	  	return -1;
    	}

	pthread_create(&tid_init, &attr, sensor_pub, NULL);

   	printf("start dht11_read_val !!!\n");    
   	while(1) {
        	dht11_read_val();
    	   	sleep(1);
   	}
    
  	return 0;
}
