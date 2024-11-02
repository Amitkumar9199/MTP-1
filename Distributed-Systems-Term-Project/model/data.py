from torch.utils.data import Dataset
import torch
import numpy as np


class SineWaveDataset(Dataset):
    def __init__(self, length):
        self.length = length
        self.data_x = torch.rand(length, 1, dtype=torch.float32) * 2.0 - 1.0
        self.data_y = torch.sin(self.data_x * torch.pi)

    def __len__(self):
        return self.length

    def __getitem__(self, index):
        x = self.data_x[index]
        y = self.data_y[index]
        return x, y
# END SineWaveDataset


if __name__ == "__main__":
    dataset = SineWaveDataset(10)
    for i in range(10):
        x, y = dataset[i]
        print(x.shape, y.shape)
