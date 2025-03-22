#!/usr/bin/env python3
import json
import random
import string
import argparse

def generate_payload(size):
    # Generate a random alphanumeric string of the specified size.
    return ''.join(random.choices(string.ascii_letters + string.digits, k=size))

def main():
    parser = argparse.ArgumentParser(
        description="Generate a JSON file with up to 2^22 items. Each item has a numeric sorting field and a variable-length payload."
    )
    parser.add_argument("--num_items", type=int, default=100,
                        help="Number of items to generate (maximum is 2^22).")
    parser.add_argument("--payload_size", type=int, default=16,
                        help="Size in bytes for the payload (default: 16).")
    parser.add_argument("--output", type=str, default="input.json",
                        help="Output JSON file name (default: input.json)")
    args = parser.parse_args()

    max_items = 2 ** 22
    if args.num_items > max_items:
        print(f"Error: num_items cannot exceed {max_items}.")
        return

    with open(args.output, "w") as outfile:
        outfile.write("[\n")
        for i in range(args.num_items):
            # Generate a 32-bit random integer for the sorting column.
            sorting_value = random.getrandbits(32)
            item = {
                "sorting": sorting_value,
                "payload": generate_payload(args.payload_size)
            }
            json_item = json.dumps(item)
            if i < args.num_items - 1:
                outfile.write("  " + json_item + ",\n")
            else:
                outfile.write("  " + json_item + "\n")
        outfile.write("]\n")
    
    print(f"Generated {args.num_items} items with payload size {args.payload_size} bytes in '{args.output}'.")

if __name__ == "__main__":
    main()
