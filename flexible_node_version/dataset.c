#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include "MQTTClient.h"
#include "MQTTAsync.h"

//when execute this file, you must put server serial number and dataset label(0 -> close, 1 -> open)
//ex) ./dataset Arise 0

char url[] = "ws://broker.hivemq.com:8000"; //mqtt hivemq broker url <socket://broker url:socket number>
MQTTClient_connectOptions conn_opts_sync = MQTTClient_connectOptions_initializer;
MQTTClient_deliveryToken token;
MQTTClient client_sync;

MQTTAsync client;
MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;

pthread_mutex_t mutex_write;

typedef struct TOPIC{
	char *topic_name;
	int type; // 0 -> th 1 -> co2
	double temp[6];
	double humid[6];
	int co2[6];
	struct TOPIC *next;
}topic_list;

topic_list* head;

int qos = 1;
int timer = 1;

char open[1] = {0x0};
char *stopstring;
FILE *sub, *list;
char serial[10];
char topic[20];
char id[20];

void reset_data(){
    topic_list* curr = head->next;

    while(curr != NULL){
    	if(curr->type == 0){
	    for(int i = 0; i < 6; i++){
	        (curr->temp)[i] = 0;
		(curr->humid)[i] = 0;
	    }
	}
	else
	    for(int i = 0; i < 6; i++)
		(curr->co2)[i] = 0;

	curr = curr->next;
    }
}

int senser_arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
//save each value of temp-humid and CO2
{
    pthread_mutex_lock(&mutex_write);

    topic_list* curr = head->next;

    while(curr != NULL){
    	if(strcmp(curr->topic_name, topicName) == 0){
		if(curr->type == 0){
            	    char *ptr = strtok(message->payload, " ");
           	    (curr->temp)[0] = (double)strtof(ptr, &stopstring);
            	    ptr = strtok(NULL, " ");
            	    (curr->humid)[0] = (double)strtof(ptr, &stopstring);
		}
		else{
        	    (curr->co2)[0] = atoi((char *)message->payload);
		}

		break;
	}
	curr = curr->next;
    }

    pthread_mutex_unlock(&mutex_write);

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

void handler_MQTT_init()
{
    int rc = MQTTAsync_create(&client, url, id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    rc = MQTTAsync_setCallbacks(client, NULL, NULL, senser_arrived, NULL);
    conn_opts.context = client;
     if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
	    while(rc != MQTTCLIENT_SUCCESS){
	    	rc = MQTTAsync_connect(client, &conn_opts);
	    }
    }

    opts.context = client;

    printf("dataset connect\n");

    list = fopen("/home/borim/final_iot/final_server/flexible_node_version/node_list.txt", "r");

    pthread_mutex_lock(&mutex_write);
    topic_list* curr = head;

    while(!feof(list)){
    	char str[100];
	fgets(str, 100, list);
	
	if(feof(list))
		break;

	topic_list* node = (topic_list*)malloc(sizeof(topic_list));
	node->topic_name = (char*)malloc(sizeof(char) * strlen(str)); 
	str[strlen(str) - 1] = 0x0;
	strcpy(node->topic_name, str);

	char *ptr = strtok(str, "/");
	ptr = strtok(NULL, "/");

	if(strstr(ptr, "th") != NULL)
	    node->type = 0;
	else
	    node->type = 1;
	
	node->next = NULL;
	
	//subscribe sensor topic in node_list.txt
	if ((rc = MQTTAsync_subscribe(client, node->topic_name, qos, &opts)) != MQTTCLIENT_SUCCESS)
        {
	    while((rc = MQTTAsync_subscribe(client, node->topic_name, qos, &opts)) != MQTTCLIENT_SUCCESS){
	    }
        }

	curr->next = node;
	curr = curr->next;
    }
    
    pthread_mutex_unlock(&mutex_write);
    fclose(list);
    
    reset_data();
}

bool check_list(){
    topic_list* curr = head->next;
    
    while(curr != NULL){
	if(curr->type == 0){
	    for(int i = 5; i > 0; i--){
	    	(curr->temp)[i] = (curr->temp)[i - 1];
	    	(curr->humid)[i] = (curr->humid)[i - 1];
	    }
	}
	else{
	    for(int i = 5; i > 0; i--){
	        (curr->co2)[i] = (curr->co2)[i - 1];
	    }
	}

	curr = curr->next;
    }

    curr = head->next;

    while(curr != NULL){
    	if(curr->type == 0){
	    for(int i = 0; i < 6; i++){
	    	if((curr->temp)[i] == 0 || (curr->humid)[i] == 0)
		    return false;
	    }
	}
	else{
	    for(int i = 0; i < 6; i++){
		if((curr->co2)[i] == 0)
		    return false;
	    }
	}

	curr = curr->next;
    }

    return true;
}

void *dataWrite()
{
    char sub_write[1024];
    sub = fopen("/home/borim/final_iot/final_server/flexible_node_version/dataset_sub.txt", "a");

    bool check;

    while(timer){
	sleep(15);
        check = true;

        pthread_mutex_lock(&mutex_write);
	
	//if temp_humid array or CO2 array is not yet filled
	check = check_list();
        if(check){ //write the difference of temp, humid, CO2 value with value right before of them
	
 	    int offset = 0; 

	    topic_list* curr = head->next;

	    for(int i = 5; i > 1; i--){
		    curr = head->next;
	   	 while(curr != NULL){
	     	     if(curr->type == 0){
		         offset += sprintf(sub_write + offset, "%.2f ", fabs((curr->temp)[i - 1] - (curr->temp)[i]));
			 offset += sprintf(sub_write + offset, "%.2f ", fabs((curr->humid)[i - 1] - (curr->humid)[i]));
		     }
		     else
		         offset += sprintf(sub_write + offset, "%d ", (curr->co2)[i - 1] - (curr->co2)[i]);

		     curr = curr->next;
	   	 }
	    }

	    curr = head->next;

            for(int i = 5; i > 1; i--){
		    curr = head->next;
	   	 while(curr != NULL){
	     	     if(curr->type == 0){
		         offset += sprintf(sub_write + offset, "%.2f ", fabs((curr->temp)[1] - (curr->temp)[i]));
			 offset += sprintf(sub_write + offset, "%.2f ", fabs((curr->humid)[1] - (curr->humid)[i]));
		     }
		     else
		         offset += sprintf(sub_write + offset, "%d ", (curr->co2)[1] - (curr->co2)[i]);

		     curr = curr->next;
	   	 }
	    }
		
	    offset += sprintf(sub_write + offset, "%s\n", open);

	    fputs(sub_write, sub);
        }
        
	printf("data write\n");

        pthread_mutex_unlock(&mutex_write);
    }

    fclose(sub);
}



void stop_write(int sig)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    sprintf(id, "%s/sync", serial);
    int rc = MQTTClient_create(&client_sync, url, id, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    if ((rc = MQTTClient_connect(client_sync, &conn_opts_sync)) != MQTTCLIENT_SUCCESS)
    {
	    while(rc != MQTTCLIENT_SUCCESS){
	    	rc = MQTTClient_connect(client_sync, &conn_opts_sync);
	    }
    }

    char Payload[512] = "dataset ";
    strcat(Payload, open);
    pubmsg.payload = Payload;
    pubmsg.payloadlen = strlen(Payload); 
    
    pubmsg.qos = qos;
    pubmsg.retained = 0;
    
    //publish dataset write end
    MQTTClient_publishMessage(client_sync, topic, &pubmsg, &token);

    timer = 0;
}

int main(int argc, char *argv[])
{
	strcpy(serial, argv[1]);
	strcpy(open, argv[0]);
	sprintf(topic, "%s/start_end", serial);
	sprintf(id, "%s/dataset", serial);

	head = (topic_list*)malloc(sizeof(topic_list));
	head->next = NULL;
	pthread_t write_thread;
	conn_opts.cleansession = 1;

	struct itimerval t;

	//set the signal alarm for how long for collect data
	signal(SIGALRM, stop_write);
	t.it_value.tv_sec = 190;
	t.it_interval = t.it_value;
	setitimer(ITIMER_REAL, &t, 0x0);

	pthread_mutex_init(&mutex_write, NULL);
	handler_MQTT_init();

	pthread_create(&write_thread, NULL, dataWrite, NULL);

	pthread_join(write_thread, (void *)NULL);

	printf("dataset done\n");
	return 0;
}
