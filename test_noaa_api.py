#!/usr/bin/env python3
"""Test NOAA Tides and Currents API to verify it's working correctly"""

import requests
import json
from datetime import datetime, timedelta


def test_station_list():
    """Test fetching the station list"""
    print("=" * 60)
    print("Testing NOAA Station List API")
    print("=" * 60)

    url = "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=tidepredictions"

    try:
        response = requests.get(url, timeout=10)
        print(f"Status Code: {response.status_code}")

        if response.status_code == 200:
            data = response.json()
            stations = data.get("stations", [])
            print(f"Total stations: {len(stations)}")

            if stations:
                # Show first 3 stations as examples
                print("\nFirst 3 stations:")
                for station in stations[:3]:
                    print(
                        f"  ID: {station.get('id')}, Name: {station.get('name')}, Lat: {station.get('lat')}, Lon: {station.get('lng')}"
                    )
                return True
        else:
            print(f"Error: {response.text}")
            return False

    except Exception as e:
        print(f"Exception: {e}")
        return False


def find_nearest_station(lat, lon):
    """Find nearest station to given coordinates (e.g., San Diego area)"""
    print("\n" + "=" * 60)
    print(f"Finding Nearest Station to ({lat}, {lon})")
    print("=" * 60)

    url = "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations.json?type=tidepredictions"

    try:
        response = requests.get(url, timeout=10)

        if response.status_code == 200:
            data = response.json()
            stations = data.get("stations", [])

            min_distance = float("inf")
            nearest_station = None

            for station in stations:
                station_lat = float(station.get("lat", 0))
                station_lon = float(station.get("lng", 0))

                # Simple distance calculation
                lat_diff = lat - station_lat
                lon_diff = lon - station_lon
                distance = (lat_diff**2 + lon_diff**2) ** 0.5

                if distance < min_distance:
                    min_distance = distance
                    nearest_station = station

            if nearest_station:
                print(f"Nearest Station: {nearest_station.get('name')}")
                print(f"  ID: {nearest_station.get('id')}")
                print(
                    f"  Lat: {nearest_station.get('lat')}, Lon: {nearest_station.get('lng')}"
                )
                print(f"  Distance: {min_distance:.4f} degrees")
                return nearest_station.get("id")

    except Exception as e:
        print(f"Exception: {e}")

    return None


def test_tide_predictions(station_id):
    """Test fetching tide predictions for a station"""
    print("\n" + "=" * 60)
    print(f"Testing Tide Predictions for Station {station_id}")
    print("=" * 60)

    today = datetime.now().strftime("%Y%m%d")
    tomorrow = (datetime.now() + timedelta(days=1)).strftime("%Y%m%d")

    url = f"https://api.tidesandcurrents.noaa.gov/api/prod/datagetter?product=predictions&station={station_id}&datum=MLLW&time_zone=lst_ldt&units=english&interval=h&begin_date={today}&end_date={tomorrow}&format=json"

    print(f"URL: {url[:100]}...")

    try:
        response = requests.get(url, timeout=10)
        print(f"Status Code: {response.status_code}")

        if response.status_code == 200:
            data = response.json()
            predictions = data.get("predictions", [])

            if predictions:
                print(f"Total predictions: {len(predictions)}")
                print("\nFirst 5 predictions:")
                for pred in predictions[:5]:
                    time_str = pred.get("t", "N/A")
                    height_ft = float(pred.get("v", 0))
                    height_m = height_ft * 0.3048
                    print(
                        f"  Time: {time_str}, Height: {height_ft:.2f} ft ({height_m:.2f} m)"
                    )

                # Test specifically what the ESP32 would get (first prediction)
                first_pred = predictions[0]
                tide_height_ft = float(first_pred.get("v", 0))
                tide_height_m = tide_height_ft * 0.3048
                print(f"\nCurrent tide (what ESP32 would read):")
                print(f"  Time: {first_pred.get('t')}")
                print(f"  Height: {tide_height_ft:.2f} ft ({tide_height_m:.2f} m)")
                return True
            else:
                print("No predictions found")
                return False
        else:
            print(f"Error: {response.text}")
            return False

    except Exception as e:
        print(f"Exception: {e}")
        return False


def main():
    print("\nNOAA Tides and Currents API Test")
    print("=" * 60)

    # Test 1: Station list
    if not test_station_list():
        print("\n❌ Station list test FAILED")
        return
    print("\n✅ Station list test PASSED")

    # Test 2: Find nearest station (using San Diego coordinates as example)
    test_lat = 32.7157  # San Diego area
    test_lon = -117.1611
    station_id = find_nearest_station(test_lat, test_lon)

    if not station_id:
        print("\n❌ Nearest station test FAILED")
        return
    print("\n✅ Nearest station test PASSED")

    # Test 3: Fetch tide predictions
    if not test_tide_predictions(station_id):
        print("\n❌ Tide predictions test FAILED")
        return
    print("\n✅ Tide predictions test PASSED")

    print("\n" + "=" * 60)
    print("ALL TESTS PASSED ✅")
    print("=" * 60)
    print("\nConclusion: NOAA API is working correctly!")
    print("The ESP32 implementation should be reading tide data properly.")


if __name__ == "__main__":
    main()
