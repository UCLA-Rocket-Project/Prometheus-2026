import os
import pandas as pd

# =========================
# CONFIG
# =========================

_DIR = os.path.dirname(os.path.abspath(__file__))

INPUT_CSV = os.path.join(_DIR, "raw_data", "nose_cone_data.csv")
OUTPUT_CSV = os.path.join(_DIR, "processed_data", "nose_cone_data.csv")

GROUND_OFFSET_FT = 2135

# =========================
# LOAD CSV
# =========================

df = pd.read_csv(INPUT_CSV, low_memory=False)

# =========================
# CREATE NEW DATAFRAME
# =========================

new_df = pd.DataFrame()

# -------------------------
# Time
# -------------------------
# ms -> seconds
new_df["t"] = df["ms"] / 1000.0

# -------------------------
# Barometric altitude
# -------------------------
# alt1_alt_m -> feet
# subtract launch elevation offset
new_df["baro_alt"] = (df["alt1_alt_m"] * 3.28084) - GROUND_OFFSET_FT

# -------------------------
# IMU accelerometer
# -------------------------
new_df["ax"] = df["ax"]
new_df["ay"] = df["ay"]
new_df["az"] = df["az"]

# -------------------------
# IMU gyroscope
# -------------------------
new_df["gx"] = df["gx"]
new_df["gy"] = df["gy"]
new_df["gz"] = df["gz"]

# -------------------------
# Altimeter temperature
# -------------------------
# using alt1 temp
new_df["altTemp"] = df["alt1_temp_c"]

# -------------------------
# GPS
# -------------------------
# u-blox GPS stores lat/lon as integer degrees × 1e7
new_df["lat"] = df["lat"] / 1e7
new_df["lon"] = df["lon"] / 1e7

# gps altitude: original is mm, convert to meters
new_df["gps_alt"] = df["gps_alt_mm"] / 1000.0

# gps fix type
new_df["gps_fix"] = df["fix_type"]

# -------------------------
# Radio fields
# -------------------------
# not present in original CSV
# create placeholder columns
new_df["rssi"] = None
new_df["snr"] = None

# =========================
# OPTIONAL CLEANUP
# =========================

# Remove rows where time is missing
new_df = new_df.dropna(subset=["t"])

# Reset index
new_df = new_df.reset_index(drop=True)

# =========================
# SAVE
# =========================

os.makedirs(os.path.dirname(OUTPUT_CSV), exist_ok=True)
new_df.to_csv(OUTPUT_CSV, index=False)

print(f"Converted CSV saved to: {OUTPUT_CSV}")
print(new_df.head())
