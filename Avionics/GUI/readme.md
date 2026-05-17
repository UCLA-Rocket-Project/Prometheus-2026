Step 1 — Create the virtual environment

cd [directory_with_gui_python_file]

python3 -m venv venv

// This creates a venv/ folder with an isolated Python install.

Step 2 — Activate it

source venv/bin/activate

Your terminal prompt will change to show (venv) — this means it's active. Every time you open a new terminal, you need to re-run this.

Step 3 — Install dependencies


pip install -r requirements.txt

This installs the three packages your script needs:

pyserial — provides the serial module
requests — HTTP calls to Telegraf
python-socketio — websocket client for the HUD
Step 4 — Run the script

python noseconegui.py


Step 5 — Deactivate when done (optional)
