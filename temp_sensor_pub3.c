#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <wiringPiSPI.h>
#include <unistd.h>
#include <string.h>
#include "MQTTClient.h"

#define MAX_TIME 83
#define DHT11PIN 13
//~ #define DHT11PIN 25

int dht11_val[5] = {0,0,0,0,0};
int dht11_temp[5] = {0,0,0,0,0};
float farenheit_temp;
MQTTClient client;
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token;

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
      sprintf(humid, "%d.%d", dht11_val[0], dht11_val[1]);
      sprintf(temp, "%d.%d", dht11_val[2], dht11_val[3]);
      printf("Humidity = %s %% Temperature = %s *C (%.1f *F)\n", humid, temp, farenheit);
      for(i=0 ; i<5; i++) {
          dht11_temp[i] = dht11_val[i];
      }
      farenheit_temp = farenheit;
      pubmsg.payload = temp;
      pubmsg.payloadlen = strlen(temp);
      pubmsg.qos = 0;
      pubmsg.retained = 0;
      MQTTClient_publishMessage(client, "Arise/C/h3", &pubmsg, &token);
  }
}


int main(void) {
	MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
	int rc = MQTTClient_create(&client, "ws://broker.hivemq.com:8000", "areaC_temp3", MQTTCLIENT_PERSISTENCE_NONE, NULL);

	if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }
   
        
    if(wiringPiSetup() == -1 ) {
        printf("return -1 error");    
        return -1;
    }

    printf("start dht11_read_val !!!\n");    
    while(1) {

        dht11_read_val();
        sleep(1);
    }
    
    return 0;
}
