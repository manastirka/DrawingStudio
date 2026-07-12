#!/bin/bash

echo "Testing Apple Core ML Service..."
echo ""

cd "$(dirname "$0")"

# Start service
source venv/bin/activate
python3 apple_coreml_service.py &
PID=$!

sleep 2

# Send test command
echo '{"action": "generate", "prompt": "sunset", "width": 512, "height": 512, "steps": 20}' 

# Wait a bit
sleep 30

# Kill service
kill $PID 2>/dev/null

echo ""
echo "Test complete"
