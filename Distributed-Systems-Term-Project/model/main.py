from model import Net
from data import SineWaveDataset

import torch
import torch.optim as optim
import torch.nn.functional as F
from torch.utils.data import DataLoader

DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")


# Train function
def train(
    model,
    train_loader,
    optimizer,
    loss_fn,
):
    # Train the model
    curr_loss = 0.0

    model.train()
    for data, target in train_loader:
        data = data.to(device=DEVICE, non_blocking=True)
        target = target.to(device=DEVICE, non_blocking=True)

        optimizer.zero_grad()
        output = model(data)
        loss = loss_fn(output, target)
        loss.backward()
        optimizer.step()

        curr_loss += loss.item()
    # END for data, target in train_loader

    mean_loss = curr_loss / len(train_loader)

    return mean_loss
# END train


# Test function
def test(
    model,
    test_loader,
):
    total_loss = 0.0

    model.eval()
    # Test the model
    with torch.inference_mode():
        for data, target in test_loader:
            data = data.to(device=DEVICE, non_blocking=True)
            target = target.to(device=DEVICE, non_blocking=True)

            output = model(data)
            loss = loss_fn(output, target).numpy(force=True)
            total_loss += loss.item()
        # END for i in range(DATA_LEN)
    # END with torch.inference_mode()

    mean_loss = total_loss / len(test_loader)

    return mean_loss
# END test


if __name__ == "__main__":
    TRAIN_LEN = 10_000
    TEST_LEN = 1_000
    BATCH_SIZE = 64
    EPOCHS = 200

    # Create a dataset
    train_dataset = SineWaveDataset(TRAIN_LEN)
    test_dataset = SineWaveDataset(TEST_LEN)

    # Create a data loader
    train_loader = DataLoader(train_dataset,
                              batch_size=BATCH_SIZE,
                              shuffle=True)
    test_loader = DataLoader(test_dataset,
                             batch_size=BATCH_SIZE,
                             shuffle=False)

    # Create a model
    model = Net()

    # Load the model
    # model.load_state_dict(torch.load("model.pth"))

    model = model.to(device=DEVICE, non_blocking=True)

    # Create an optimizer
    optimizer = optim.SGD(model.parameters(), lr=0.01, momentum=0.5)

    # Create a loss function
    loss_fn = F.mse_loss

    best_loss = float("inf")

    for epoch in range(EPOCHS):
        train_loss = train(model, train_loader, optimizer, loss_fn)
        test_loss = test(model, test_loader)

        if test_loss < best_loss:
            best_loss = test_loss
            torch.save(model.state_dict(), "model.pth")

        print(f"Epoch: {epoch + 1:>3}, "
              f"Train Loss: {train_loss:10.6f}, "
              f"Test Loss: {test_loss:10.6f}, "
              f"Best Loss: {best_loss:10.6f}")
    # END for epoch in range(EPOCHS)

# END if __name__ == "__main__"
