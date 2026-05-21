import pandas as pd
from typing import Any


def get_stats(out_path: str) -> None:
    df = pd.read_csv(out_path)
    df = df[df["timestamp"] > 0]
    df["timestamp"] /= 1e6

    total_time = df["timestamp"].max() - df["timestamp"].min()

    data = {"Sensor": df["source"].unique(), "Total Samples": [], "Sampling Rate": []}
    for entry in data["Sensor"]:
        num_samples = len(df[df["source"] == entry])
        data["Total Samples"].append(num_samples)
        data["Sampling Rate"].append(num_samples / total_time)

    stats_df = pd.DataFrame(data)

    print_sample_stats(total_time, stats_df)


FORMATTER_START = "\n=================================================================================\n"
FORMATTER_END = "\n=================================================================================\n"


def print_sample_stats(total_time_s: float, input: Any) -> None:
    print(
        FORMATTER_START,
        f"Total runtime: {total_time_s} seconds, {total_time_s / 60} minutes\n\n",
        input,
        FORMATTER_END,
    )


if __name__ == "__main__":
    get_stats("../out2.csv")
