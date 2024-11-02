#include<bits/stdc++.h> 
using namespace std;

#include "regression/decision_tree_model.h"
// #include "regression/extra_trees_model.h"
// #include "regression/random_forest_model.h"


int main(){

//     age,sex,bmi,bp,s1,s2,s3,s4,s5,s6,target
// 0.056238598688520124,0.05068011873981862,0.021817159785093684,0.056300895272529315,-0.007072771253015731,0.018101327204732443,-0.03235593223976409,-0.002592261998183278,-0.02364686309993755,0.02377494398854077,178.0
    vector<double> features(10);
    string s;
    getline(cin, s);
    // split based on ','
    stringstream ss(s);
    string token;
    int i = 0;
    while(getline(ss, token, ',')){
        features[i] = stod(token);
        i++;
    }

    for(int i = 0; i < 10; i++){
        cout << features[i] << " ";
    }
    cout << endl;

    const int16_t feat[10];
    for(int i = 0; i < 10; i++){
        feat[i] = features[i]*1000;
    }
    // DecisionTreeModel model;
    float out = model_predict(features,10);

    return 0;
}