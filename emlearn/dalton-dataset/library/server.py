import os
import scipy
import numpy as np
import pandas as pd
import ruptures as rpt
# from library import downsample_10sec,get_normalized_signals


# --
MAX_MISSING_MINUTES=15
SMOOTHING_WINDOW=60 #in sec
POLLUTANTS=['C2H5OH', 'CO', 'CO2', 'NO2','PMS1', 'PMS10', 'PMS2_5', 'VoC', 'H', 'T']

OVER_WRITE=False
PEN=25

def downsample_10sec(df_cpy):
    df_cpy['ts10sec']=df_cpy.ts.apply(lambda e:e[:-1])
    df_10sec=df_cpy.groupby('ts10sec')[POLLUTANTS].mean()
    return df_10sec

def get_normalized_signals(df_10sec):
    #computing min and max only on positive pollutants
    #corner case: when CO2 is invalid for whole day then results NaN for CO2 min and max
    #NaN min max values are repalced with 0
    gMin=df_10sec[df_10sec[POLLUTANTS]>0][POLLUTANTS].quantile(0.0015).replace(np.nan,0) #upto 3 std 99.7%
    gMax=df_10sec[df_10sec[POLLUTANTS]>0][POLLUTANTS].quantile(0.9985).replace(np.nan,0) #upto 3 std 99.7%
    
    #Normalizing all signals (stability factor eps: 1e-10)
    signal_norm=((df_10sec[POLLUTANTS]-gMin)/((gMax-gMin)+1e-10)).clip(0,1)
    return signal_norm



# --


OVER_WRITE=False

COLUMNS=['CO2','VoC','PMS2_5','PMS10','H','T']
#TARGET LEVELS
CO2_LEVEL=1000 #ppm
VOC_LEVEL= 220 #ppb
PMS2_5_LEVEL= 100 #mu g/m^3 (https://www.iqair.com/india)
PMS10_LEVEL=100 #mu g/m^3   (https://www.iqair.com/india)
H_LEVEL= 60 #% COMFORT
T_LEVEL=26 #deg COMFORT

MIN_PEAK_LVLS=dict(zip(COLUMNS,[CO2_LEVEL,VOC_LEVEL,PMS2_5_LEVEL,PMS10_LEVEL,H_LEVEL,T_LEVEL]))

DOWN_to_10=True
SEC_MULTIPLIER=60 # for 1 min 60 sec
if DOWN_to_10:
    SEC_MULTIPLIER=6 #for 1 min 6 10sec seg
    
GRAD_AVG_WIN=20 #20 sec
if DOWN_to_10:
    GRAD_AVG_WIN=2 #20sec
    
MINUTE=10
WINDOW=MINUTE*SEC_MULTIPLIER

def read_valid(f):
    df=pd.read_csv(f)
    return df[(df.Valid==1) & (df.Valid_CO2==1)].copy()

def read_data(fname,COLUMNS=None,down_to_10=False,norm=False):
    df=read_valid(fname)
    if down_to_10:df=downsample_10sec(df)
    else:
        df['ts']=df.ts.apply(pd.to_datetime)
        df.set_index('ts',drop=True,inplace=True)
    if norm:df=get_normalized_signals(df)
    return df[COLUMNS]



#Breeze (https://www.breeze-technologies.de/blog/calculating-an-actionable-indoor-air-quality-index/)
def co2_idx(co2):
    if 0<=co2<400:
        return 1
    elif 400<=co2<1000:
        return 2
    elif 1000<=co2<1500:
        return 3
    elif 1500<=co2<2000:
        return 4
    elif 2000<=co2<5000:
        return 5
    elif co2>5000:
        return 6

#https://learn.kaiterra.com/en/resources/understanding-tvoc-volatile-organic-compounds
def voc_idx(voc):
    if 0<=voc<=220:
        return 1
    elif 221<=voc<=660:
        return 2
    elif 661<=voc<=1430:
        return 3
    elif 1431<=voc<=2200:
        return 4
    elif 2201<=voc<=3300:
        return 5
    elif voc>=3301:
        return 6

TempHum_mat=\
pd.DataFrame([[6,5,4,3,3,3,3,4,5,5,6,6,6],
[5,4,2,2,2,2,2,3,4,5,5,5,6],
[5,4,3,2,1,1,1,2,3,4,5,5,6],
[5,4,3,2,1,1,1,2,2,3,4,5,6],
[5,5,4,3,2,1,1,1,2,3,4,5,6],
[6,5,5,4,3,3,3,2,2,3,4,5,6],
[6,5,5,4,4,4,4,4,4,4,4,5,6],
[6,5,5,5,5,5,5,5,5,5,5,5,6],
[6,6,6,6,6,6,6,6,6,6,6,6,6]],
index=[9,8,7,6,5,4,3,2,1],
columns=np.array([16,17,18,19,20,21,22,23,24,25,26,27,28])+12) #12 deg bias added for India

def IAQI_breeze(co2,voc,ht_idx):
    c1=co2_idx(co2)
    c2=voc_idx(voc)
    return max([c1,c2,ht_idx])

def TRH_idx(temp,hum):
    row=np.clip(round(hum)%10,1,9)
    col=np.clip(round(temp),16+12,28+12) #+12 deg due to india temp
    return TempHum_mat.loc[row,col]

def Ico2(Cco2):
    return (70*np.log(Cco2/(CO2_LEVEL+1e-7)))

def Ivoc(Cvoc):
    return (100*np.log(Cvoc/(VOC_LEVEL+1e-7)))

def Ipm2_5(Cpm2_5):
    return (85*np.log(Cpm2_5/(PMS2_5_LEVEL+1e-7)))

def Ipm10(Cpm10):
    return (85*np.log(Cpm10/(PMS10_LEVEL+1e-7)))

def compute_min_max_rate_of_change(d,COLUMN,pen=10): 
    # pen=10 #10 works best
    # COLUMN='VoC'
    cpd=rpt.Pelt(model='rbf',min_size=1*SEC_MULTIPLIER)
    dd=d[[COLUMN]]
    bkps=cpd.fit_predict(dd,pen)[:-1] #determine change points
    if len(bkps)==0:
        return 0,0 #no change detected
    grads=np.gradient(dd[COLUMN])
    bk_grds=[]
    for bk in bkps:
        lower=np.clip(bk-GRAD_AVG_WIN,0,grads.shape[0]) #setting lower to be 0
        upper=np.clip(bk+GRAD_AVG_WIN,0,grads.shape[0]) #setting upper to be maxlen
        bk_grds.append(grads[np.arange(lower,upper)].mean()) #get avg grad in the change points +- 15 window
    return min(bk_grds),max(bk_grds) #send min and max grad

def get_features(d):
    index=[]
    value=[]
    for c in d.columns:
        peaks=scipy.signal.find_peaks(d[c],height=MIN_PEAK_LVLS[c])[0]; peak_count=len(peaks)
        peak_dur=scipy.signal.peak_widths(d[c],peaks)[0].sum()
        long_stay=(d[c]>MIN_PEAK_LVLS[c]).sum()
        min_grad,max_grad=compute_min_max_rate_of_change(d,c)
        index.extend([f'{c}_avg',f'{c}_min',f'{c}_max',f'{c}_std',
                      f'{c}_roc_min',f'{c}_roc_max',f'{c}_pc',f'{c}_pd',f'{c}_lg_stay'])
        value.extend([d[c].mean(),d[c].min(),d[c].max(),d[c].std(),min_grad,max_grad,peak_count,peak_dur,long_stay])
    #computing IAQI like values
    index.extend(['I_co2','I_voc','I_pm2_5','I_pm10','IAQI'])
    indidual_idx=[Ico2(d['CO2'].mean()),Ivoc(d['VoC'].mean()),Ipm2_5(d['PMS2_5'].mean()),Ipm10(d['PMS10'].mean())]
    iaqi=max(indidual_idx)
    value.extend(indidual_idx+[iaqi])
    #Breeze AQIs
    ht_idx=TRH_idx(round(d['T'].mean()),round(d['H'].mean()))
    baqi=IAQI_breeze(round(d['CO2'].mean()),round(d['VoC'].mean()),ht_idx)
    index.extend(['HT_idx','BAQI'])
    value.extend([ht_idx,baqi])
    return pd.Series(value,index)

def read_feat(fname):
    df=read_data(fname,COLUMNS,down_to_10=DOWN_to_10)
    index=[]
    stats=[]
    for d in df.rolling(WINDOW):
        if d.shape[0]<WINDOW:
            continue
        stats.append(get_features(d))
        index.append(d.index[-1])
    df_feat=pd.DataFrame(stats,index=index)
    return df_feat

def process_feat_for(fname,parent_dir):
    name='_'.join(fname.split("/")[-2:])
    if (not os.path.exists(f"{parent_dir}/{name}")) or OVER_WRITE:
        df=read_feat(fname)
        df.to_csv(f"{parent_dir}/{name}")
    else:
        print("Skipping",fname)


######################

import os
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score
from sklearn.preprocessing import StandardScaler
import sklearn.ensemble
import sklearn.tree
import sklearn.neural_network
import sklearn.naive_bayes
import matplotlib.pyplot as plt

import pickle


# Define dataset loading function
def load_dataset():
    data = pd.read_csv('classifiers/lab_activity.csv')
    df = data.copy()
    df.columns = data.columns
    
    # Exclude columns based on specified criteria
    df = df.drop(columns=['ts'])
    # df = df.loc[:, ~df.columns.str.startswith('PMS10')]
    # df = df.loc[:, ~df.columns.str.startswith('VoC')]
    df = df.rename(columns={'label': 'target'})
    # df['target'] = df['target'].astype('category').cat.codes
    
    # Get the original categories (reverse mapping)
    categories = df['target'].astype('category').cat.categories
    df['target'] = df['target'].astype('category').cat.codes
    reverse_mapping = {code: category for code, category in enumerate(categories)}


    return df,reverse_mapping

# Load and save dataset
dataset,reverse_mapping = load_dataset()
# dataset.to_csv('library/new_lab_activity.csv', index=False)

#
# Plots the decision boundaries score landscape
def plot_results(ax, model, X, y):
    from sklearn.inspection import DecisionBoundaryDisplay

    # show classification boundaries
    DecisionBoundaryDisplay.from_estimator(
        model, X, alpha=0.4, ax=ax, response_method="auto",
    )

    # show datapoints
    ax.scatter(X.iloc[:, 0], X.iloc[:, 1], c=y, s=20, edgecolor="k")


# Function to train and evaluate classifiers
def build_run_classifier(model, name, test_dict):
    target_column = 'target'
    train = dataset
    feature_columns = list(set(dataset.columns) - set([target_column]))

    # convert dictionary to dataframe
    values = [test_dict[name] for name in feature_columns]

    test = pd.DataFrame([values], columns=feature_columns)

    # # Apply scaling for MLP classifier
    # if name == 'sklearn_mlp':
    #     scaler = StandardScaler()
    #     train[feature_columns] = scaler.fit_transform(train[feature_columns])
    #     test[feature_columns] = scaler.transform(test[feature_columns])

    model.fit(train[feature_columns], train[target_column])
    test_pred = model.predict(test[feature_columns])

    # print(f"Prediction for {name}: {test_pred}")
    # print(f"Prediction for {name}: {reverse_mapping[test_pred[0]]}")
    return test_pred[0], reverse_mapping[test_pred[0]]

# Define classifiers
classifiers = {
    'decision_tree': sklearn.tree.DecisionTreeClassifier(),
    # 'random_forest': sklearn.ensemble.RandomForestClassifier(n_estimators=10, random_state=1),
    # 'extra_trees': sklearn.ensemble.ExtraTreesClassifier(n_estimators=10, random_state=1),
    # 'gaussian_naive_bayes': sklearn.naive_bayes.GaussianNB(),
    # 'sklearn_mlp': sklearn.neural_network.MLPClassifier(learning_rate_init=0.01, solver='adam', hidden_layer_sizes=(10, 10), max_iter=1000, random_state=1),
}

# Train and evaluate each classifier
def get_predict(test_dict):

    for name, cls in classifiers.items():
        val,pred = build_run_classifier(cls, name, test_dict)
        return val,pred

#################################### ------------------

import socket
import json
import pandas as pd
from collections import deque

# Server configurations
UDP_IP = "0.0.0.0"      # Listen on all network interfaces
UDP_PORT = 12345         # Port to listen on

# Get the hostname
hostname = socket.gethostname()
# Get the local IP address
local_ip = socket.gethostbyname(hostname)

print(f"Local IP Address: {local_ip}")

# Column names expected by get_features
COLUMNS = ['CO2','VoC','PMS2_5','PMS10','H','T']
# COLUMNS = ["T", "H", "FMHDS", "PMS1", "PMS2_5", "PMS10", "NO2", "C2H5OH", "VoC", "CO", "CO2"]


# Initialize a deque with 30 default values
max_size = 30
data_list = deque([{
    "T": 0.0, "H": 0.0, "FMHDS": 0, "PMS1": 0, "PMS2_5": 0, "PMS10": 0,
    "NO2": 0, "C2H5OH": 0, "VoC": 0, "CO": 0, "CO2": 0
}] * max_size, maxlen=max_size)

# Create the UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Server listening on port {UDP_PORT}...")


def process_data(data):
    """Parse incoming data, add it to the list, and calculate features."""
    try:
        # Decode JSON data
        parsed_data = json.loads(data)
        # Append to the deque; if over 30 items, oldest is removed automatically
        data_list.append(parsed_data)
        # print(len(data_list))
        # print("Updated data list:", list(data_list))
        
        # Create DataFrame from the current data list and ensure correct column order
        df = pd.DataFrame(list(data_list))
        df = df[COLUMNS]  # Ensure it matches the expected column format
        features = get_features(df)
        
        model_filename = 'library/decision_tree_model.pkl'
        with open(model_filename, 'rb') as file:
            model = pickle.load(file)

        # # Convert features to a NumPy array (if needed)
        original_feature_names=['CO2_avg_max','CO2_avg_min','CO2_min_max','CO2_min_min','CO2_max_max','CO2_max_min','CO2_std_max','CO2_std_min','CO2_roc_min_max','CO2_roc_min_min','CO2_roc_max_max','CO2_roc_max_min','CO2_pc_max','CO2_pc_min','CO2_pd_max','CO2_pd_min','CO2_lg_stay_max','CO2_lg_stay_min','VoC_avg_max','VoC_avg_min','VoC_min_max','VoC_min_min','VoC_max_max','VoC_max_min','VoC_std_max','VoC_std_min','VoC_roc_min_max','VoC_roc_min_min','VoC_roc_max_max','VoC_roc_max_min','VoC_pc_max','VoC_pc_min','VoC_pd_max','VoC_pd_min','VoC_lg_stay_max','VoC_lg_stay_min','PMS2_5_avg_max','PMS2_5_avg_min','PMS2_5_min_max','PMS2_5_min_min','PMS2_5_max_max','PMS2_5_max_min','PMS2_5_std_max','PMS2_5_std_min','PMS2_5_roc_min_max','PMS2_5_roc_min_min','PMS2_5_roc_max_max','PMS2_5_roc_max_min','PMS2_5_pc_max','PMS2_5_pc_min','PMS2_5_pd_max','PMS2_5_pd_min','PMS2_5_lg_stay_max','PMS2_5_lg_stay_min','PMS10_avg_max','PMS10_avg_min','PMS10_min_max','PMS10_min_min','PMS10_max_max','PMS10_max_min','PMS10_std_max','PMS10_std_min','PMS10_roc_min_max','PMS10_roc_min_min','PMS10_roc_max_max','PMS10_roc_max_min','PMS10_pc_max','PMS10_pc_min','PMS10_pd_max','PMS10_pd_min','PMS10_lg_stay_max','PMS10_lg_stay_min','H_avg_max','H_avg_min','H_min_max','H_min_min','H_max_max','H_max_min','H_std_max','H_std_min','H_roc_min_max','H_roc_min_min','H_roc_max_max','H_roc_max_min','H_pc_max','H_pc_min','H_pd_max','H_pd_min','H_lg_stay_max','H_lg_stay_min','T_avg_max','T_avg_min','T_min_max','T_min_min','T_max_max','T_max_min','T_std_max','T_std_min','T_roc_min_max','T_roc_min_min','T_roc_max_max','T_roc_max_min','T_pc_max','T_pc_min','T_pd_max','T_pd_min','T_lg_stay_max','T_lg_stay_min']
        # new_feature_names = ['VoC_min_max','T_std_max','T_roc_max_max','PMS2_5_pd_min','PMS10_roc_max_min','H_min_min','T_avg_max','VoC_roc_max_min','CO2_std_max','VoC_pd_max','PMS2_5_pc_max','T_roc_min_max','H_pc_min','T_min_min','CO2_lg_stay_min','VoC_pc_min','PMS10_pd_max','H_max_max','PMS10_max_max','H_pc_max','PMS10_std_min','H_std_min','PMS10_min_max','CO2_avg_max','VoC_pd_min','PMS2_5_min_min','CO2_max_max','H_avg_max','H_roc_min_max','VoC_min_min','PMS10_std_max','H_pd_min','H_avg_min','T_pd_min','CO2_pc_max','PMS10_pd_min','PMS2_5_max_min','H_max_min','CO2_min_min','PMS2_5_std_min','VoC_avg_max','PMS2_5_lg_stay_max','T_avg_min','T_max_min','CO2_std_min','VoC_pc_max','T_pd_max','PMS2_5_max_max','CO2_max_min','PMS2_5_roc_max_min','H_roc_min_min','CO2_roc_max_min','PMS2_5_lg_stay_min','CO2_roc_min_max','PMS10_max_min','PMS10_lg_stay_max','H_pd_max','PMS10_pc_max','CO2_avg_min','T_roc_max_min','PMS10_roc_min_min','PMS2_5_avg_min','PMS10_roc_max_max','T_roc_min_min','PMS2_5_pd_max','VoC_lg_stay_max','T_min_max','PMS2_5_roc_min_min','T_pc_min','PMS2_5_min_max','PMS10_pc_min','PMS2_5_roc_max_max','PMS2_5_pc_min','T_pc_max','VoC_avg_min','T_std_min','H_roc_max_max','T_lg_stay_max','H_lg_stay_min','CO2_roc_min_min','PMS2_5_roc_min_max','T_max_max','PMS10_roc_min_max','VoC_lg_stay_min','CO2_pd_max','CO2_min_max','VoC_roc_max_max','PMS10_min_min','PMS2_5_avg_max','PMS2_5_std_max','H_min_max','CO2_roc_max_max','PMS10_avg_min','VoC_max_min','VoC_roc_min_min','VoC_roc_min_max','H_lg_stay_max','VoC_max_max','VoC_std_max','CO2_pc_min','T_lg_stay_min','CO2_pd_min','PMS10_avg_max','PMS10_lg_stay_min','H_std_max','VoC_std_min','H_roc_max_min','CO2_lg_stay_max']

        features = features.to_dict()
        # features = {k: features[k] for k in cols}
        features = [392.0536378333333,376.6767495,386.215,367.57835000000006,396.71999,385.89165,7.797296560707762,3.38738724549757,-0.1912512500000005,-0.7358312499999968,-0.1912512500000005,-0.7358312499999968,0.0,0.0,0.0,0.0,0.0,0.0,405.9544438333333,152.04566816666667,404.67501,150.79499,407.49832,153.48832,2.0578176818462195,0.913440559389202,-0.0614637499999943,-0.2050012499999951,-0.0614637499999943,-0.2050012499999951,5.0,0.0,5.952799610106189,0.0,60.0,0.0,43.617889,42.91472216666667,42.49499,41.42168,44.98,43.96331,0.7223323969434877,0.3533465970417556,0.1854175000000006,0.0,0.1854175000000006,0.0,0.0,0.0,0.0,0.0,0.0,0.0,52.78019449999999,52.144222000000006,51.03167,49.77835,54.72999,53.21833,1.2444626507142398,0.6284212379617323,0.3954162499999984,0.0,0.3954162499999984,0.0,0.0,0.0,0.0,0.0,0.0,0.0,36.423862500000006,32.5492505,36.00517,32.29282,36.60917,32.78983,0.1824298339533394,0.1209448497437901,0.0247112500000001,0.0,0.0247112500000001,0.0,0.0,0.0,0.0,0.0,0.0,0.0,27.19863283333333,24.26755066666667,27.1933,24.21532,27.2,24.3,0.038756826340951,0.0019091120410573,0.0113574999999994,0.0,0.0113574999999994,0.0,5.0,0.0,39.05611659581376,0.0,60.0,0.0]
        feature_dict = dict(zip(original_feature_names, features))

        val,pred = get_predict(feature_dict)
        print(f"Prediction : {val}")
        print(f"Prediction : {pred}")


    except json.JSONDecodeError:
        print("Received malformed data:", data)
    except KeyError as e:
        print(f"Missing column in data: {e}")

while True:
    # Listen for data from the Arduino
    data, addr = sock.recvfrom(1024)  # Buffer size is 1024 bytes
    # print("Received message:", data.decode("utf-8"))
    
    # Process the data
    process_data(data.decode("utf-8"))
