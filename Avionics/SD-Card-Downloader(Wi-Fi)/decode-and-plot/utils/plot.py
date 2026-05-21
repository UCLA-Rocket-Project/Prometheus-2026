import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import matplotlib.axes as gridaxes
from typing import Any, Literal

from .types import Boards

FORMATTER_START = "\n=================================================================================\n"
FORMATTER_END = "\n===================================================================================\n"

# Rolling z-score window size (number of samples). Larger = smoother reference.
ROLLING_WINDOW = 250
# How many local std-devs away from the rolling mean counts as an outlier.
ROLLING_THRESHOLD = 5


# TODO: review this code
def filter_rolling_zscore(
    df: pd.DataFrame,
    cols: list[str],
    window: int = ROLLING_WINDOW,
    threshold: float = ROLLING_THRESHOLD,
) -> pd.DataFrame:
    """
    For each column in `cols`, replace values that deviate more than
    `threshold` * rolling_std from the rolling_mean with NaN.
    Uses min_periods=1 so edges are still filtered even with sparse data.
    """
    df = df.copy()
    for col in cols:
        if col not in df.columns:
            continue
        rolling = df[col].rolling(window=window, center=True, min_periods=1)
        mean = rolling.mean()
        std = rolling.std(ddof=0).fillna(0)
        # Where std is 0 (flat signal), any non-zero deviation is an outlier
        std_safe = std.replace(0, float("nan"))
        z = (df[col] - mean).abs() / std_safe
        df.loc[z > threshold, col] = float("nan")
    return df


def plot_digital_v2(out_path: str) -> None:
    df = pd.read_csv(out_path)

    # clear all the rows with the bad time readings
    df = df[df["timestamp"] > 0]
    df["timestamp"] /= 1e6
    df["lat"] /= 1e7
    df["long"] /= 1e7

    ADXL_RANGE = 200 * 9.81
    df = df[
        df["acc_x"].isna()
        | (
            (df["acc_x"].abs() < ADXL_RANGE)
            & (df["acc_y"].abs() < ADXL_RANGE)
            & (df["acc_z"].abs() < ADXL_RANGE)
        )
    ]

    shock1 = df[df["source"] == "ADXL 1"].copy()
    shock1 = filter_rolling_zscore(shock1, ["acc_x", "acc_y", "acc_z"])
    print_stats(shock1.describe())

    shock2 = df[df["source"] == "ADXL 2"].copy()
    shock2 = filter_rolling_zscore(shock2, ["acc_x", "acc_y", "acc_z"])
    print_stats(shock2.describe())

    secondary_v2 = df[df["source"] == "SECONDARY V2"].copy()
    print_stats(secondary_v2.describe())

    fig = plt.figure(figsize=(12, 17))
    gs = gridspec.GridSpec(5, 2, figure=fig)

    # First 4: full-width rows
    ax1 = fig.add_subplot(gs[0, :])
    ax2 = fig.add_subplot(gs[1, :])
    ax3 = fig.add_subplot(gs[2, :])
    ax4 = fig.add_subplot(gs[3, :])
    # Last 2: side by side in the bottom row
    ax5 = fig.add_subplot(gs[4, 0])
    ax6 = fig.add_subplot(gs[4, 1])

    shock1.plot(x="timestamp", y=["acc_x", "acc_y", "acc_z"], ax=ax1, title="ADXL 1")
    shock2.plot(x="timestamp", y=["acc_x", "acc_y", "acc_z"], ax=ax2, title="ADXL 2")
    secondary_v2.plot(
        x="timestamp",
        y=["imu_acc_x", "imu_acc_y", "imu_acc_z"],
        ax=ax3,
        title="IMU Acc",
    )
    secondary_v2.plot(
        x="timestamp",
        y=["gyr_x", "gyr_y", "gyr_z"],
        ax=ax4,
        title="IMU Gyr",
    )
    secondary_v2.plot(
        x="timestamp",
        y=["pressure"],
        ax=ax5,
        title="Alt Pressure",
    )
    secondary_v2.plot(
        x="timestamp",
        y=["lat", "long"],
        ax=ax6,
        title="GPS",
    )

    fig.tight_layout()
    plt.show()


def plot_digital_v1(out_path: str) -> None:
    df = pd.read_csv(out_path)

    # clear all the rows with the bad time readings
    df = df[df["timestamp"] > 0]
    df["timestamp"] /= 1e6

    ADXL_RANGE = 200 * 9.81
    df = df[
        df["acc_x"].isna()
        | (
            (df["acc_x"].abs() < ADXL_RANGE)
            & (df["acc_y"].abs() < ADXL_RANGE)
            & (df["acc_z"].abs() < ADXL_RANGE)
        )
    ]

    # only has the 400Hz accelerometer
    shock2 = df[df["source"] == "ADXL 2"].copy()
    shock2 = filter_rolling_zscore(shock2, ["acc_x", "acc_y", "acc_z"])
    print_stats(shock2.describe())

    secondary_v1 = df[df["source"] == "SECONDARY V1"].copy()
    print_stats(secondary_v1.describe())

    fig = plt.figure(figsize=(12, 17))
    gs = gridspec.GridSpec(4, 2, figure=fig)

    # First 4: full-width rows
    ax1 = fig.add_subplot(gs[0, :])
    ax2 = fig.add_subplot(gs[1, :])
    ax3 = fig.add_subplot(gs[2, :])
    ax4 = fig.add_subplot(gs[3, :])

    shock2.plot(x="timestamp", y=["acc_x", "acc_y", "acc_z"], ax=ax1, title="ADXL")
    secondary_v1.plot(
        x="timestamp",
        y=["imu_acc_x", "imu_acc_y", "imu_acc_z"],
        ax=ax2,
        title="IMU Acc",
    )
    secondary_v1.plot(
        x="timestamp",
        y=["gyr_x", "gyr_y", "gyr_z"],
        ax=ax3,
        title="IMU Gyr",
    )
    secondary_v1.plot(
        x="timestamp",
        y=["pressure"],
        ax=ax4,
        title="Alt Pressure",
    )

    fig.tight_layout()
    plt.show()


def plot_analog(out_path: str, board: Boards) -> None:
    df = pd.read_csv(out_path)

    # clear all the rows with the bad time readings
    df = df[df["timestamp"] > 0]
    df["timestamp"] /= 1e6

    fig = plt.figure(figsize=(12, 14))
    gs = gridspec.GridSpec(3, 1, figure=fig)

    ax1 = fig.add_subplot(gs[0, :])
    ax2 = fig.add_subplot(gs[1, :])
    ax3 = fig.add_subplot(gs[2, :])

    df.plot(x="timestamp", y="pt1", ax=ax1, title=f"{board} Channel 1")
    df.plot(x="timestamp", y="pt2", ax=ax2, title=f"{board} Channel 2")
    df.plot(x="timestamp", y="pt3", ax=ax3, title=f"{board} Channel 3")

    # ax1.set_ylim([2.558, 2.565])  
    # ax2.set_ylim([2.558, 2.565])  
    # ax3.set_ylim([2.558, 2.565])  


    fig.tight_layout()
    plt.show()


def print_stats(stats: Any) -> None:
    print(FORMATTER_START, stats, FORMATTER_END)
