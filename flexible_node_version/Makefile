gcc:
	gcc dataset.c -lpaho-mqtt3as -lpaho-mqtt3cs -lwiringPi -lpthread -o dataset
	gcc indoor_co2.c -o indoor_co2 -l paho-mqtt3cs -lwiringPi -lpthread
	gcc server_init.c -o server_init -lpaho-mqtt3cs -lpthread
	gcc server_learn.c -o server_learn -lpaho-mqtt3cs -lpthread
	gcc server_service.c -o server_service -lpaho-mqtt3as -lpaho-mqtt3cs -lpthread

