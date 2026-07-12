#!/usr/bin/env python3
"""
AI Icon Generator for DrawingStudio
This script helps generate SVG icons using AI tools like DALL-E, Midjourney, or Stable Diffusion.

Usage:
    python generate_ai_icons.py --tool select --style "minimal white outline"
    python generate_ai_icons.py --all --style "clean vector icons"
"""

import argparse
import json
import os
import requests
from pathlib import Path

class AIIconGenerator:
    def __init__(self):
        self.output_dir = Path("resources/ai_icons")
        self.output_dir.mkdir(exist_ok=True)
        
        # Icon specifications for each tool
        self.icon_specs = {
            "select": {
                "description": "Selection tool with arrow cursor and dashed selection box",
                "prompt": "minimal white outlined arrow cursor with dashed selection box, 48x48px, vector style"
            },
            "line": {
                "description": "Simple diagonal line with endpoint dots",
                "prompt": "minimal white outlined diagonal line with small dots at endpoints, 48x48px, vector style"
            },
            "curve": {
                "description": "Smooth curved line with control points",
                "prompt": "minimal white outlined curved line with small control point circles, 48x48px, vector style"
            },
            "rectangle": {
                "description": "Simple outlined rectangle",
                "prompt": "minimal white outlined rectangle, clean corners, 48x48px, vector style"
            },
            "ellipse": {
                "description": "Simple outlined circle/ellipse",
                "prompt": "minimal white outlined circle, clean shape, 48x48px, vector style"
            },
            "eraser": {
                "description": "Eraser block with metal band",
                "prompt": "minimal white outlined eraser block with metal band on top, 48x48px, vector style"
            },
            "fill": {
                "description": "Paint bucket with handle and drops",
                "prompt": "minimal white outlined paint bucket with handle and paint drops, 48x48px, vector style"
            },
            "hand": {
                "description": "Simple hand outline for panning",
                "prompt": "minimal white outlined hand shape, clean fingers, 48x48px, vector style"
            },
            "measure": {
                "description": "Ruler or measuring tool",
                "prompt": "minimal white outlined ruler or measuring tool, 48x48px, vector style"
            },
            "image": {
                "description": "Image frame or picture icon",
                "prompt": "minimal white outlined image frame with picture, 48x48px, vector style"
            }
        }
    
    def generate_prompt(self, tool_name, style="minimal white outline"):
        """Generate AI prompt for icon creation"""
        if tool_name not in self.icon_specs:
            raise ValueError(f"Unknown tool: {tool_name}")
        
        spec = self.icon_specs[tool_name]
        base_prompt = spec["prompt"]
        
        return f"{base_prompt}, {style}, transparent background, professional icon design"
    
    def create_dalle_prompt(self, tool_name, style="minimal white outline"):
        """Create optimized prompt for DALL-E 3"""
        prompt = self.generate_prompt(tool_name, style)
        return f"Create a {prompt}. The icon should be suitable for a professional drawing application toolbar."
    
    def create_midjourney_prompt(self, tool_name, style="minimal white outline"):
        """Create optimized prompt for Midjourney"""
        prompt = self.generate_prompt(tool_name, style)
        return f"{prompt} --ar 1:1 --style raw --v 6"
    
    def create_stable_diffusion_prompt(self, tool_name, style="minimal white outline"):
        """Create optimized prompt for Stable Diffusion"""
        prompt = self.generate_prompt(tool_name, style)
        return f"{prompt}, vector art, line art, monochrome, high contrast, clean design"
    
    def save_prompts_to_file(self, tool_name, style="minimal white outline"):
        """Save AI prompts to a file for easy copying"""
        output_file = self.output_dir / f"{tool_name}_prompts.txt"
        
        with open(output_file, 'w') as f:
            f.write(f"AI Icon Generation Prompts for '{tool_name}' Tool\n")
            f.write("=" * 50 + "\n\n")
            
            f.write("Tool Description:\n")
            f.write(f"{self.icon_specs[tool_name]['description']}\n\n")
            
            f.write("DALL-E 3 Prompt:\n")
            f.write(f"{self.create_dalle_prompt(tool_name, style)}\n\n")
            
            f.write("Midjourney Prompt:\n")
            f.write(f"{self.create_midjourney_prompt(tool_name, style)}\n\n")
            
            f.write("Stable Diffusion Prompt:\n")
            f.write(f"{self.create_stable_diffusion_prompt(tool_name, style)}\n\n")
            
            f.write("Manual SVG Creation Tips:\n")
            f.write("- Use simple white strokes (stroke='white' stroke-width='2-3')\n")
            f.write("- Keep shapes minimal and recognizable\n")
            f.write("- Use 48x48 viewBox for consistency\n")
            f.write("- Ensure transparent background\n")
            f.write("- Test at different sizes\n")
        
        print(f"Prompts saved to: {output_file}")
    
    def generate_all_prompts(self, style="minimal white outline"):
        """Generate prompts for all tools"""
        for tool_name in self.icon_specs.keys():
            self.save_prompts_to_file(tool_name, style)
    
    def create_svg_template(self, tool_name):
        """Create a basic SVG template for manual editing"""
        svg_content = f"""<?xml version="1.0" encoding="UTF-8"?>
<svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg">
  <!-- AI-generated icon for {tool_name} tool -->
  <!-- Replace this comment with your icon design -->
  <!-- Use white strokes: stroke="white" stroke-width="2" -->
  <!-- Keep it simple and recognizable -->
</svg>"""
        
        output_file = self.output_dir / f"{tool_name}.svg"
        with open(output_file, 'w') as f:
            f.write(svg_content)
        
        print(f"SVG template created: {output_file}")
        return output_file

def main():
    parser = argparse.ArgumentParser(description="Generate AI prompts for DrawingStudio icons")
    parser.add_argument("--tool", help="Specific tool to generate prompt for")
    parser.add_argument("--all", action="store_true", help="Generate prompts for all tools")
    parser.add_argument("--style", default="minimal white outline", help="Style description for icons")
    parser.add_argument("--template", action="store_true", help="Create SVG template instead of prompts")
    
    args = parser.parse_args()
    
    generator = AIIconGenerator()
    
    if args.template and args.tool:
        generator.create_svg_template(args.tool)
    elif args.tool:
        generator.save_prompts_to_file(args.tool, args.style)
    elif args.all:
        generator.generate_all_prompts(args.style)
    else:
        print("Available tools:")
        for tool in generator.icon_specs.keys():
            print(f"  - {tool}: {generator.icon_specs[tool]['description']}")
        print("\nUse --tool <name> to generate prompts for a specific tool")
        print("Use --all to generate prompts for all tools")

if __name__ == "__main__":
    main()


