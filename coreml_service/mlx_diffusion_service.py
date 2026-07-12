#!/usr/bin/env python3
"""
MLX Stable Diffusion Service
Uses Apple's MLX framework for native Metal GPU acceleration
Avoids PyTorch MPS black image bug
"""

import sys
import json
import base64
from io import BytesIO
from pathlib import Path

try:
    import mlx.core as mx
    from diffusionkit.mlx import DiffusionPipeline
    from PIL import Image
    import numpy as np
except ImportError as e:
    print(json.dumps({
        "error": f"Missing dependencies: {e}. Run: pip install mlx diffusionkit"
    }))
    sys.exit(1)


class MLXStableDiffusion:
    def __init__(self):
        """Initialize MLX Stable Diffusion pipeline"""
        self.pipe = None
        
    def load_model(self):
        """Load Stable Diffusion with MLX (pure Metal GPU)"""
        try:
            # Load SD 1.5 with MLX - uses Metal GPU directly
            # No MPS, no black images!
            self.pipe = DiffusionPipeline(
                "stabilityai/stable-diffusion-v1-5",
                low_memory_mode=False  # Use full GPU
            )
            
            return {"success": True, "message": "MLX model loaded successfully (Pure Metal GPU)"}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def generate(self, prompt, negative_prompt="", width=512, height=512, steps=20, guidance_scale=7.5, seed=-1):
        """Generate image from prompt"""
        try:
            if self.pipe is None:
                result = self.load_model()
                if not result["success"]:
                    return result
            
            # Generate with MLX (pure Metal GPU, no CPU, no MPS bugs)
            images = self.pipe.generate_image(
                prompt=prompt,
                negative_prompt=negative_prompt if negative_prompt else None,
                height=height,
                width=width,
                num_inference_steps=steps,
                guidance_scale=guidance_scale,
                seed=seed if seed >= 0 else None
            )
            
            # Get the image (MLX returns a list or single image)
            if isinstance(images, list):
                image = images[0]
            else:
                image = images
            
            # Ensure it's a PIL Image
            if not isinstance(image, Image.Image):
                # Convert numpy array to PIL Image if needed
                if isinstance(image, np.ndarray):
                    image = Image.fromarray((image * 255).astype(np.uint8))
                else:
                    raise ValueError(f"Unexpected image type: {type(image)}")
            
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
            import traceback
            error_details = traceback.format_exc()
            sys.stderr.write(f"Generation error: {error_details}\n")
            sys.stderr.flush()
            return {"success": False, "error": str(e), "details": error_details}


def main():
    """Main service loop - reads JSON commands from stdin"""
    service = MLXStableDiffusion()
    
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
