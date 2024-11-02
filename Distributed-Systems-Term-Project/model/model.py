import torch.nn as nn
import torch


class Net(nn.Module):
    """
    Simple shallow neural network with 1 hidden layer
    """
    
    def __init__(self):
        super(Net, self).__init__()
        self.layers = nn.Sequential(
            nn.Linear(1, 256),
            nn.ReLU(inplace=True),
            nn.Linear(256, 1),
        )
    # END __init__

    def forward(self, x):
        x = self.layers(x)
        return x

    # END forward
# END Net

if __name__ == "__main__":
    model = Net()
    x = torch.rand(10, 1)
    y = model(x)
    print(x.shape, y.shape)
