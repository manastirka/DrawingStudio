#!/usr/bin/env python3
"""
Create SVG icons for DrawingStudio
"""

import os

def create_svg_icon(name, content):
    """Create an SVG icon file"""
    filename = f"resources/ai_icons/{name}.svg"
    with open(filename, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Created {filename}")

# Create all SVG icons
icons = {
    "select": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <path d="M8 8 L8 20 L4 16 L6 16 L6 12 L10 12 Z" fill="none" stroke="white" stroke-width="2"/>
  <rect x="14" y="18" width="16" height="8" fill="none" stroke="white" stroke-width="1" stroke-dasharray="2,2"/>
</svg>''',

    "line": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <line x1="12" y1="32" x2="32" y2="12" stroke="white" stroke-width="3" stroke-linecap="round"/>
  <circle cx="12" cy="32" r="2" fill="white"/>
  <circle cx="32" cy="12" r="2" fill="white"/>
</svg>''',

    "curve": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <path d="M10 30 Q24 10 38 30" fill="none" stroke="white" stroke-width="3" stroke-linecap="round"/>
  <circle cx="10" cy="30" r="2" fill="white"/>
  <circle cx="24" cy="10" r="2" fill="white"/>
  <circle cx="38" cy="30" r="2" fill="white"/>
</svg>''',

    "rectangle": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <rect x="10" y="14" width="28" height="20" fill="none" stroke="white" stroke-width="3"/>
</svg>''',

    "ellipse": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <ellipse cx="24" cy="24" rx="14" ry="10" fill="none" stroke="white" stroke-width="3"/>
</svg>''',

    "eraser": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <rect x="14" y="20" width="20" height="12" fill="none" stroke="white" stroke-width="3"/>
  <line x1="14" y1="18" x2="34" y2="18" stroke="white" stroke-width="2"/>
  <line x1="14" y1="19" x2="34" y2="19" stroke="white" stroke-width="2"/>
</svg>''',

    "fill": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <polygon points="14,32 34,32 30,16 18,16" fill="none" stroke="white" stroke-width="3"/>
  <path d="M26 12 A4 4 0 0 1 34 12" fill="none" stroke="white" stroke-width="2"/>
  <circle cx="16" cy="34" r="2" fill="white"/>
  <circle cx="24" cy="36" r="1.5" fill="white"/>
</svg>''',

    "hand": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <path d="M18 30 Q16 26 18 22 Q20 18 24 20 Q26 22 28 18 Q30 16 32 18 Q32 20 30 24 Q28 28 26 30 Q24 32 20 32 Z" fill="none" stroke="white" stroke-width="2"/>
</svg>''',

    "measure": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <line x1="8" y1="24" x2="40" y2="24" stroke="white" stroke-width="2"/>
  <line x1="8" y1="20" x2="8" y2="28" stroke="white" stroke-width="2"/>
  <line x1="40" y1="20" x2="40" y2="28" stroke="white" stroke-width="2"/>
  <text x="24" y="18" text-anchor="middle" fill="white" font-size="8">100</text>
</svg>''',

    "image": '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <rect x="12" y="8" width="24" height="20" fill="none" stroke="white" stroke-width="3"/>
  <line x1="16" y1="12" x2="28" y2="12" stroke="white" stroke-width="2"/>
  <line x1="16" y1="16" x2="24" y2="16" stroke="white" stroke-width="2"/>
  <line x1="16" y1="20" x2="26" y2="20" stroke="white" stroke-width="2"/>
  <line x1="16" y1="24" x2="22" y2="24" stroke="white" stroke-width="2"/>
</svg>'''
}

# Create the directory if it doesn't exist
os.makedirs("resources/ai_icons", exist_ok=True)

# Create all icons
for name, content in icons.items():
    create_svg_icon(name, content)

print("All SVG icons created successfully!")


