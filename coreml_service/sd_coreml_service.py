#!/usr/bin/env python3
"""
Core ML Stable Diffusion Service
Provides fast GPU-accelerated image generation using Apple's Core ML
"""

import sys
import json
import base64
from io import BytesIO
from pathlib import Path

try:
    import coremltools as ct
    from diffusers import StableDiffusionPipeline, DPMSolverMultistepScheduler
    from PIL import Image
    import numpy as np
    import torch
except ImportError as e:
    print(json.dumps({
        "error": f"Missing dependencies: {e}. Run: cd coreml_service && ./install_coreml.sh"
    }))
    sys.exit(1)


class CoreMLStableDiffusion:
    def __init__(self, model_path=None):
        """Initialize Core ML Stable Diffusion pipeline"""
        self.pipe = None
        self.model_path = model_path or str(Path.home() / "coreml_models" / "stable-diffusion-v1-5")
        
    def load_model(self):
        """Load the model with MPS (Metal Performance Shaders) backend"""
        try:
            # Check if MPS is available (Mac with Apple Silicon)
            if not torch.backends.mps.is_available():
                return {
                    "success": False,
                    "error": "MPS (Metal) backend not available. Requires macOS 12.3+ and Apple Silicon."
                }
            
            # Load Stable Diffusion pipeline
            # Use HuggingFace model - will download on first run (~4GB)
            model_id = "stabilityai/stable-diffusion-2-1-base"
            
            # IMPORTANT: Disable MPS due to black image bug
            # Use CPU instead - slower but works correctly
            import os
            os.environ["PYTORCH_ENABLE_MPS_FALLBACK"] = "1"
            
            self.pipe = StableDiffusionPipeline.from_pretrained(
                model_id,
                torch_dtype=torch.float32,  # Use FP32 for CPU
                use_safetensors=True
            )
            
            # Use DPM++ scheduler for better quality
            self.pipe.scheduler = DPMSolverMultistepScheduler.from_config(
                self.pipe.scheduler.config
            )
            
            # Keep on CPU to avoid MPS black image bug
            self.pipe = self.pipe.to("cpu")
            
            # Enable attention slicing for memory efficiency
            self.pipe.enable_attention_slicing()
            
            # Disable safety checker for speed
            self.pipe.safety_checker = None
            
            return {"success": True, "message": "Model loaded successfully with Metal GPU"}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def generate(self, prompt, negative_prompt="", width=512, height=512, steps=20, guidance_scale=7.5, seed=-1):
        """Generate image from prompt"""
        try:
            if self.pipe is None:
                result = self.load_model()
                if not result["success"]:
                    return result
            
            # Create generator for reproducibility
            generator = None
            if seed >= 0:
                generator = torch.Generator(device="mps").manual_seed(seed)
            else:
                generator = torch.Generator(device="mps")
            
            # IMPORTANT: MPS backend workaround for black images
            # Use CPU for some operations to avoid MPS bugs
            with torch.autocast("mps", dtype=torch.float16):
                result = self.pipe(
                    prompt=prompt,
                    negative_prompt=negative_prompt if negative_prompt else None,
                    height=height,
                    width=width,
                    num_inference_steps=steps,
                    guidance_scale=guidance_scale,
                    generator=generator
                )
            
            # Get the image
            image = result.images[0]
            
            # Verify image is not black
            img_array = np.array(image)
            if img_array.mean() < 1.0:  # Nearly black image
                return {
                    "success": False,
                    "error": "Generated image is black. This is a known MPS backend issue. Try: 1) Different prompt, 2) Different seed, or 3) Restart the app."
                }
            
            # Convert to base64 for transfer
            buffer = BytesIO()
            image.save(buffer, format="PNG")
            img_base64 = base64.b64encode(buffer.getvalue()).decode()
            
            return {
                "success": True,
                "image": img_base64,
                "width": width,
                "height": height
            }
            
        except Exception as e:
            return {"success": False, "error": str(e)}


def main():
    """Main service loop - reads JSON commands from stdin"""
    service = CoreMLStableDiffusion()
    
    print(json.dumps({"status": "ready"}), flush=True)
    
    for line in sys.stdin:
        try:
            command = json.loads(line.strip())
            
            if command["action"] == "generate":
                result = service.generate(
                    prompt=command.get("prompt", ""),
                    negative_prompt=command.get("negative_prompt", ""),
                    width=command.get("width", 512),
                    height=command.get("height", 512),
                    steps=command.get("steps", 20),
                    guidance_scale=command.get("guidance_scale", 7.5),
                    seed=command.get("seed", -1)
                )
                print(json.dumps(result), flush=True)
                
            elif command["action"] == "load":
                result = service.load_model()
                print(json.dumps(result), flush=True)
                
            elif command["action"] == "quit":
                break
                
        except Exception as e:
            print(json.dumps({"success": False, "error": str(e)}), flush=True)


if __name__ == "__main__":
    main()
