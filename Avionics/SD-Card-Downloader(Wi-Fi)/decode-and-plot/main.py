import argparse
from utils import (
    check_headers,
    decode_digital,
    decode_analog,
    plot_analog,
    plot_digital_v2,
    plot_digital_v1,
    get_stats,
    Boards,
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Input CSV file")
    parser.add_argument(
        "-t",
        "--type",
        help="Board type: [digitalv1 | digitalv2 | analogv1 | analogv2]",
        required=True,
        type=is_valid_board,
    )
    parser.add_argument("-o", "--output", help="Output CSV file", default="out.csv")
    args: argparse.Namespace = parser.parse_args()
    check_headers(args.input)

    if args.type.lower() == "digitalv1" or args.type.lower() == "digitalv2":
        type = decode_digital(args.input, args.output)
    else:
        type = decode_analog(args.input, args.output, args.type.lower())

    assert type != None and type != Boards.ERROR, "Board data was not recognized"

    get_stats(args.output)

    if type == Boards.DIGITAL_2:
        print("\n\nPlotting for Digital V2")
        plot_digital_v2(args.output)
    elif type == Boards.DIGITAL_1:
        print("\n\nPlotting for Digital V1")
        plot_digital_v1(args.output)
    elif type == Boards.ANALOG_1:
        print("\n\nPlotting for Analog V1")
        plot_analog(args.output, type)
    else:
        print("\n\nPlotting for Analog V2")
        plot_analog(args.output, type)


def is_valid_board(value) -> str:
    valid_boards = ["digitalv1", "digitalv2", "analogv1", "analogv2"]

    processed = value.strip().replace(" ", "").lower()
    if processed in valid_boards:
        return processed
    raise argparse.ArgumentTypeError(
        f"{value} is has to be one of the following: [digitalv1 | digitalv2 | analogv1 | analogv2]"
    )


if __name__ == "__main__":
    main()
