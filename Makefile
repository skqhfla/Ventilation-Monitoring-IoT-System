all:
	gcc temp_sensor_pub1.c -o temp1 -l paho-mqtt3cs -lwiringPi -lpthread
