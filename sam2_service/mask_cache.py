"""
Mask and Embedding Cache
Provides instant results for repeated images
"""
import hashlib
import pickle
import os
from collections import OrderedDict
from typing import Optional, Dict, Any
import numpy as np

class MaskCache:
    """LRU cache for masks and embeddings"""
    
    def __init__(self, max_memory_items: int = 10, cache_dir: str = 'cache'):
        self.max_memory_items = max_memory_items
        self.cache_dir = cache_dir
        
        # In-memory LRU cache
        self.memory_cache: OrderedDict = OrderedDict()
        
        # Create cache directory
        os.makedirs(cache_dir, exist_ok=True)
        os.makedirs(os.path.join(cache_dir, 'embeddings'), exist_ok=True)
        os.makedirs(os.path.join(cache_dir, 'masks'), exist_ok=True)
        
    def get_image_hash(self, image: np.ndarray) -> str:
        """Generate hash for image"""
        # Use image shape and a sample of pixels for speed
        h, w = image.shape[:2]
        
        # Sample pixels from corners and center
        samples = [
            image[0, 0],
            image[0, w-1],
            image[h-1, 0],
            image[h-1, w-1],
            image[h//2, w//2],
        ]
        
        # Create hash from shape + samples + mean
        hash_input = f"{h}x{w}_{samples}_{image.mean():.2f}".encode()
        return hashlib.md5(hash_input).hexdigest()
    
    def get_embedding(self, image_hash: str) -> Optional[Any]:
        """Get cached embedding"""
        # Check memory cache first
        if image_hash in self.memory_cache:
            # Move to end (most recently used)
            self.memory_cache.move_to_end(image_hash)
            return self.memory_cache[image_hash]['embedding']
        
        # Check disk cache
        disk_path = os.path.join(self.cache_dir, 'embeddings', f'{image_hash}.pkl')
        if os.path.exists(disk_path):
            try:
                with open(disk_path, 'rb') as f:
                    embedding = pickle.load(f)
                
                # Add to memory cache
                self._add_to_memory(image_hash, {'embedding': embedding})
                return embedding
            except Exception as e:
                print(f"Cache read error: {e}")
                return None
        
        return None
    
    def set_embedding(self, image_hash: str, embedding: Any):
        """Cache embedding"""
        # Add to memory cache
        self._add_to_memory(image_hash, {'embedding': embedding})
        
        # Save to disk
        disk_path = os.path.join(self.cache_dir, 'embeddings', f'{image_hash}.pkl')
        try:
            with open(disk_path, 'wb') as f:
                pickle.dump(embedding, f)
        except Exception as e:
            print(f"Cache write error: {e}")
    
    def get_masks(self, image_hash: str) -> Optional[list]:
        """Get cached mask results"""
        # Check memory cache
        if image_hash in self.memory_cache:
            self.memory_cache.move_to_end(image_hash)
            return self.memory_cache[image_hash].get('masks')
        
        # Check disk cache
        disk_path = os.path.join(self.cache_dir, 'masks', f'{image_hash}.pkl')
        if os.path.exists(disk_path):
            try:
                with open(disk_path, 'rb') as f:
                    masks = pickle.load(f)
                
                # Add to memory cache
                if image_hash in self.memory_cache:
                    self.memory_cache[image_hash]['masks'] = masks
                else:
                    self._add_to_memory(image_hash, {'masks': masks})
                
                return masks
            except Exception as e:
                print(f"Cache read error: {e}")
                return None
        
        return None
    
    def set_masks(self, image_hash: str, masks: list):
        """Cache mask results"""
        # Add to memory cache
        if image_hash in self.memory_cache:
            self.memory_cache[image_hash]['masks'] = masks
        else:
            self._add_to_memory(image_hash, {'masks': masks})
        
        # Save to disk
        disk_path = os.path.join(self.cache_dir, 'masks', f'{image_hash}.pkl')
        try:
            with open(disk_path, 'wb') as f:
                pickle.dump(masks, f)
        except Exception as e:
            print(f"Cache write error: {e}")
    
    def _add_to_memory(self, key: str, value: Dict):
        """Add to memory cache with LRU eviction"""
        if key in self.memory_cache:
            # Update existing
            self.memory_cache[key].update(value)
            self.memory_cache.move_to_end(key)
        else:
            # Add new
            self.memory_cache[key] = value
            
            # Evict oldest if over limit
            if len(self.memory_cache) > self.max_memory_items:
                self.memory_cache.popitem(last=False)
    
    def clear(self):
        """Clear all caches"""
        self.memory_cache.clear()
        
        # Clear disk cache
        import shutil
        if os.path.exists(self.cache_dir):
            shutil.rmtree(self.cache_dir)
            os.makedirs(self.cache_dir, exist_ok=True)
            os.makedirs(os.path.join(self.cache_dir, 'embeddings'), exist_ok=True)
            os.makedirs(os.path.join(self.cache_dir, 'masks'), exist_ok=True)
    
    def get_stats(self) -> Dict:
        """Get cache statistics"""
        embedding_count = len([f for f in os.listdir(os.path.join(self.cache_dir, 'embeddings')) if f.endswith('.pkl')])
        mask_count = len([f for f in os.listdir(os.path.join(self.cache_dir, 'masks')) if f.endswith('.pkl')])
        
        return {
            'memory_items': len(self.memory_cache),
            'disk_embeddings': embedding_count,
            'disk_masks': mask_count
        }
