#!/usr/bin/env python3
"""
Test script for YOLO + SAM2 human detection
"""
import requests
import base64
import json
import sys
from PIL import Image
import io
import numpy as np


def test_human_detection(image_path, endpoint='segment_largest_human'):
    """Test human detection on an image"""
    
    # Read and encode image
    print(f"Loading image: {image_path}")
    with open(image_path, 'rb') as f:
        image_data = f.read()
    
    image_b64 = base64.b64encode(image_data).decode('utf-8')
    
    # Send request
    url = f'http://localhost:5001/{endpoint}'
    print(f"Sending request to {url}...")
    
    payload = {
        'image': image_b64,
        'confidence': 0.5
    }
    
    response = requests.post(url, json=payload)
    
    if response.status_code != 200:
        print(f"Error: {response.status_code}")
        print(response.text)
        return False
    
    result = response.json()
    
    if not result.get('success'):
        print(f"Detection failed: {result.get('error', 'Unknown error')}")
        return False
    
    # Print results
    print("\n" + "="*60)
    print("DETECTION RESULTS")
    print("="*60)
    
    if endpoint == 'segment_largest_human':
        if result.get('human') is None:
            print("No human detected")
        else:
            print(f"Human detected!")
            print(f"  Confidence: {result['confidence']:.2%}")
            print(f"  SAM2 Score: {result['sam2_score']:.2f}")
            print(f"  Area: {result['area_percent']:.1f}%")
            print(f"  Bounding Box: {result['bbox']}")
            print(f"  Contour Points: {len(result['contour'])}")
    
    elif endpoint == 'segment_humans':
        count = result.get('count', 0)
        print(f"Detected {count} human(s)")
        
        for i, human in enumerate(result.get('humans', [])):
            print(f"\nHuman {i+1}:")
            print(f"  Confidence: {human['confidence']:.2%}")
            print(f"  SAM2 Score: {human['sam2_score']:.2f}")
            print(f"  Area: {human['area_percent']:.1f}%")
            print(f"  Bounding Box: {human['bbox']}")
            print(f"  Contour Points: {len(human['contour'])}")
    
    print("="*60)
    return True


def check_service():
    """Check if service is running"""
    try:
        response = requests.get('http://localhost:5001/health')
        if response.status_code == 200:
            health = response.json()
            print("✓ Service is running")
            print(f"  SAM2 loaded: {health.get('sam2_loaded')}")
            print(f"  Device: {health.get('device')}")
            return True
        else:
            print("✗ Service returned error")
            return False
    except requests.exceptions.ConnectionError:
        print("✗ Service is not running")
        print("  Start it with: python sam2_service.py")
        return False


if __name__ == '__main__':
    print("="*60)
    print("YOLO + SAM2 Human Detection Test")
    print("="*60)
    print()
    
    # Check service
    if not check_service():
        sys.exit(1)
    
    print()
    
    # Get image path from command line or use default
    if len(sys.argv) > 1:
        image_path = sys.argv[1]
    else:
        print("Usage: python test_human_detection.py <image_path>")
        print("\nExample:")
        print("  python test_human_detection.py /path/to/photo.jpg")
        sys.exit(1)
    
    # Test largest human detection
    print("\n" + "="*60)
    print("TEST 1: Detect Largest Human")
    print("="*60)
    test_human_detection(image_path, 'segment_largest_human')
    
    # Test all humans detection
    print("\n" + "="*60)
    print("TEST 2: Detect All Humans")
    print("="*60)
    test_human_detection(image_path, 'segment_humans')
    
    print("\n✓ Tests completed!")
