import torch
from matplotlib import pyplot as plt

from model import Net


def evaluate(model, num):
    vals = torch.linspace(-1, 1, num).view(-1, 1)
    reals = torch.sin(vals * torch.pi)
    with torch.inference_mode():
        preds = model(vals)
    return vals.numpy(), preds.numpy(), reals.numpy()


if __name__ == "__main__":
    model = Net()
    model.load_state_dict(torch.load("model.pth")) 
    model.eval()

    vals, preds, reals = evaluate(model, 100)
    for val, pred, real in zip(vals, preds, reals):
        plt.plot(val, pred, "ro")
        plt.plot(val, real, "bx")
    
    plt.legend(["Prediction", "Ground Truth"])
    plt.grid(True)
    plt.show()
