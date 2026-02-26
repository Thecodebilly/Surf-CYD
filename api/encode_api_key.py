#!/usr/bin/env python3
"""
API Key Encoder for Surf Board High Score System
This generates the obfuscated API key array for Database.cpp
"""


def encode_api_key(api_key):
    """Encode an API key by XORing each character with 0xAA"""
    if not api_key:
        return "const uint8_t encoded[] = {}; // Empty = no API key"

    encoded_values = [f"0x{ord(c) ^ 0xAA:02X}" for c in api_key]
    encoded_str = ", ".join(encoded_values)

    return f"const uint8_t encoded[] = {{{encoded_str}}};"


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python3 encode_api_key.py <your_api_key>")
        print("Example: python3 encode_api_key.py mysecretkey123")
        print("\nThis will generate the obfuscated array to paste into Database.cpp")
        sys.exit(1)

    api_key = sys.argv[1]
    encoded = encode_api_key(api_key)

    print("\n" + "=" * 70)
    print("API Key Encoder for Surf Board")
    print("=" * 70)
    print(f"\nOriginal API Key: {api_key}")
    print(f"\nPaste this into Database.cpp getApiKey() function:")
    print("-" * 70)
    print(f"  {encoded}")
    print("-" * 70)
    print("\nReplace the line:")
    print("  const uint8_t encoded[] = {}; // Empty = no API key")
    print("\nWith the generated line above.")
    print("=" * 70 + "\n")
