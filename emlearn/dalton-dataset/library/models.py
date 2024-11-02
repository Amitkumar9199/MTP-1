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
    df['target'] = df['target'].astype('category').cat.codes

    return df

# Load and save dataset
dataset = load_dataset()
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
def build_run_classifier(ax,model, name, test_dict):
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

    # Calculate accuracy
    accuracy = accuracy_score(test[target_column], test_pred)
    print(f"Accuracy for {name}: {accuracy}")
    print(f"Prediction for {name}: {test_pred}")
    # # Save the model using pickle
    # with open(f"library/{name}_model.pkl", "wb") as f:
    #     pickle.dump(model, f)

    # plot_results(ax, model, test[feature_columns], test[target_column])
    return model, accuracy

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
    # fig, axs = plt.subplots(
    #         ncols=1,
    #         nrows=len(classifiers),
    #         figsize=(4, 4*len(classifiers)),
    #         sharex=True, sharey=True,
    # )

    # for ax, (name, cls) in zip(axs, classifiers.items()):
    #     build_run_classifier(ax, cls, name)
    #     ax.set_title(name)

    #     if ax != axs[-1]:
    #         ax.set_xlabel('')

    for name, cls in classifiers.items():
        model, accuracy = build_run_classifier(cls, name, test_dict)


# # Run the main function
# main()
