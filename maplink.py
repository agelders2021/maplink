#!/usr/bin/env python3
import sys

def main():
    if len(sys.argv) != 2:
        print("Usage: python maplink.py <lat,lon>")
        print("Example: python maplink.py 35.09742,-106.33096")
        sys.exit(1)

    coords = sys.argv[1].strip()

    try:
        lat, lon = coords.split(",")
        float(lat)
        float(lon)
    except ValueError:
        print("Error: Coordinates must be in the format: lat,lon")
        print("Example: 35.09742,-106.33096")
        sys.exit(1)

    encoded = f"{lat.strip()}%2C{lon.strip()}"
    url = f"https://www.google.com/maps/search/?api=1&query={encoded}"

    print(url)

if __name__ == "__main__":
    main()
