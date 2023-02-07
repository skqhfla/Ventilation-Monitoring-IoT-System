import numpy
import pandas as pd
import pickle
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, precision_score, recall_score, confusion_matrix
import joblib
import xgboost as xgb
import sys
import paho.mqtt.client as mqtt

#serial = "Arise" # Enter the serial
serial = sys.argv[2]

string_list = sys.argv[1].split(' ')
input_list = []

for i in string_list:
    input_list.append(float(i))

df_input = pd.DataFrame([input_list])

dict_data = {'label': -1}
dummy_label = pd.Series(dict_data)

clf_from_joblib = joblib.load('/home/borim/final_iot/final_server/flexible_node_version/randomforest_classifier_weight.joblib')

dtest = xgb.DMatrix(data = df_input, label = dummy_label)
pred = clf_from_joblib.predict(df_input)
print(pred.shape)
print(pred)

payload = str(str(pred)[1])

mqttc = mqtt.Client(serial + "inference") # enter the client's name
mqttc.connect("broker.hivemq.com", 1883) # enter the client's broker
mqttc.publish(serial + "/inference", payload) # enter the topic and payload
