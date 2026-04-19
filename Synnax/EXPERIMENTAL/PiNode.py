import multiprocessing as mp
import signal
import time

from ControlsNode import run_controls_node
from DAQNode import run_daq_node


def run_controls_process(stop_event) -> None:
    try:
        run_controls_node(stop_flag=stop_event)
    except KeyboardInterrupt:
        pass


def run_daq_process(stop_event) -> None:
    try:
        run_daq_node(stop_flag=stop_event)
    except KeyboardInterrupt:
        pass


def main() -> None:
    """Run controls and DAQ nodes in parallel on the Raspberry Pi."""
    stop_event = mp.Event()
    controls_process = mp.Process(
        target=run_controls_process,
        args=(stop_event,),
        name="ControlsNode",
    )
    daq_process = mp.Process(
        target=run_daq_process,
        args=(stop_event,),
        name="DAQNode",
    )
    controls_process.start()
    daq_process.start()

    def handle_shutdown(*_):
        stop_event.set()

    signal.signal(signal.SIGINT, handle_shutdown)
    signal.signal(signal.SIGTERM, handle_shutdown)
    try:
        while controls_process.is_alive() and daq_process.is_alive():
            time.sleep(0.5)
    finally:
        stop_event.set()
        controls_process.join(timeout=3)
        daq_process.join(timeout=3)
        if controls_process.is_alive():
            controls_process.terminate()
        if daq_process.is_alive():
            daq_process.terminate()


if __name__ == "__main__":
    main()
