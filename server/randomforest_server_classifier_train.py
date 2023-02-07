import numpy
import pandas as pd
import pickle
import joblib
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, precision_score, recall_score, confusion_matrix
import sys

import paho.mqtt.client as mqtt
from sklearn.ensemble import RandomForestClassifier
import warnings
warnings.filterwarnings('ignore')

serial = sys.argv[1]

dataset = pd.read_table('/home/borim/final_iot/final_server/flexible_node_version/dataset_sub.txt', sep = ' ')

col_list = []

for i in range(1, dataset.shape[1]):
    col_list.append(str(i))

col_list.append('label')

dataset.columns = col_list

target = dataset['label']
ftr = dataset.drop(columns='label')

X_train, X_test, y_train, y_test = train_test_split(ftr, target, test_size = 0.2, random_state=156)

rf_clf = RandomForestClassifier(random_state = 0)
rf_clf.fit(X_train, y_train)
pred = rf_clf.predict(X_test)
accuracy = accuracy_score(y_test, pred)
print('랜덤 포레스트 정확도 : {0:.4f}'.format(accuracy))

joblib.dump(rf_clf, '/home/borim/final_iot/final_server/flexible_node_version/randomforest_classifier_weight.joblib')

mqttc = mqtt.Client(serial + "/server_learn") # Enter the topics
mqttc.connect("broker.hivemq.com", 1883) # Enter the broker URL
mqttc.publish(serial + "/" + "start_end", "learning " + str(round(100*accuracy))) # Enter the topic and payload


