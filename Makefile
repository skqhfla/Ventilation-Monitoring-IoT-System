all:
	gcc temp_sensor_pub1.c -o temp1 -l paho-mqtt3cs -lwiringPi -lpthread
	gcc temp_sensor_pub2.c -o temp2 -l paho-mqtt3cs -lwiringPi -lpthread
	gcc temp_sensor_pub3.c -o temp3 -l paho-mqtt3cs -lwiringPi -lpthread
	gcc temp_sensor_pub4.c -o temp4 -l paho-mqtt3cs -lwiringPi -lpthread
