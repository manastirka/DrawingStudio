#!/bin/bash

# Remote SD Server Connection Test Script
# Override DRAWINGSTUDIO_REMOTE_SD_HOST/PORT for a server on another machine.

echo "🔍 Testing Remote SD Server Connection..."
echo "=========================================="
echo ""

SERVER_IP="${DRAWINGSTUDIO_REMOTE_SD_HOST:-127.0.0.1}"
SERVER_PORT="${DRAWINGSTUDIO_REMOTE_SD_PORT:-8000}"
SERVER_URL="http://${SERVER_IP}:${SERVER_PORT}"

# Test 1: Ping server
echo "Test 1: Ping server..."
if ping -c 3 -W 2 "$SERVER_IP" > /dev/null 2>&1; then
    echo "✅ Server is reachable via ping"
else
    echo "❌ Server is NOT reachable via ping"
    echo "   → Check if server is powered on"
    echo "   → Check network connection"
    exit 1
fi
echo ""

# Test 2: Check if port is open
echo "Test 2: Check if port $SERVER_PORT is open..."
if nc -z -w 2 "$SERVER_IP" "$SERVER_PORT" 2>/dev/null; then
    echo "✅ Port $SERVER_PORT is open"
else
    echo "❌ Port $SERVER_PORT is NOT open"
    echo "   → Check if SD WebUI is running"
    echo "   → Check firewall settings"
    exit 1
fi
echo ""

# Test 3: Test HTTP connection
echo "Test 3: Test HTTP connection..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 5 "$SERVER_URL" 2>/dev/null)
if [ "$HTTP_CODE" = "200" ] || [ "$HTTP_CODE" = "404" ]; then
    echo "✅ HTTP server is responding (code: $HTTP_CODE)"
else
    echo "⚠️  HTTP server returned code: $HTTP_CODE"
    echo "   → Server might be starting up"
fi
echo ""

# Test 4: Test SD API endpoint
echo "Test 4: Test SD API endpoint..."
API_RESPONSE=$(curl -s --connect-timeout 5 "$SERVER_URL/sdapi/v1/sd-models" 2>/dev/null)
if [ -n "$API_RESPONSE" ]; then
    echo "✅ SD API is responding"
    echo "   Response preview: ${API_RESPONSE:0:100}..."
    
    # Check if response is JSON array
    if echo "$API_RESPONSE" | jq -e '. | type == "array"' > /dev/null 2>&1; then
        echo "✅ Response is valid JSON array"
        
        # Extract model names
        MODEL_COUNT=$(echo "$API_RESPONSE" | jq '. | length' 2>/dev/null)
        echo "   Found $MODEL_COUNT model(s)"
        
        echo "$API_RESPONSE" | jq -r '.[].model_name' 2>/dev/null | while read -r model; do
            echo "   📦 $model"
        done
    else
        echo "⚠️  Response is not a JSON array"
    fi
else
    echo "❌ SD API is NOT responding"
    echo "   → Check if SD WebUI is running with --api flag"
    echo "   → Try: python webui.py --api --listen"
    exit 1
fi
echo ""

# Test 5: Check for SD 3.5 Large model
echo "Test 5: Check for SD 3.5 Large model..."
if echo "$API_RESPONSE" | jq -e '.[] | select(.model_name | contains("3.5"))' > /dev/null 2>&1; then
    MODEL_NAME=$(echo "$API_RESPONSE" | jq -r '.[] | select(.model_name | contains("3.5")) | .model_name' | head -1)
    echo "✅ Found SD 3.5 model: $MODEL_NAME"
else
    echo "⚠️  SD 3.5 Large model not found"
    echo "   Available models:"
    echo "$API_RESPONSE" | jq -r '.[].model_name' 2>/dev/null | while read -r model; do
        echo "   • $model"
    done
fi
echo ""

# Test 6: Test progress endpoint
echo "Test 6: Test progress endpoint..."
PROGRESS_RESPONSE=$(curl -s --connect-timeout 5 "$SERVER_URL/sdapi/v1/progress" 2>/dev/null)
if [ -n "$PROGRESS_RESPONSE" ]; then
    echo "✅ Progress endpoint is responding"
    PROGRESS=$(echo "$PROGRESS_RESPONSE" | jq -r '.progress' 2>/dev/null)
    echo "   Current progress: $PROGRESS"
else
    echo "⚠️  Progress endpoint not responding"
fi
echo ""

# Summary
echo "=========================================="
echo "✅ Connection Test Complete!"
echo ""
echo "Server Details:"
echo "  URL: $SERVER_URL"
echo "  Status: Online and accessible"
echo "  API: Working"
echo ""
echo "Next Steps:"
echo "1. Launch DrawingStudio"
echo "2. Go to: Assistant → Stable Diffusion Backend"
echo "3. Select: 🌐 Remote Server (SD 3.5 Large)"
echo "4. Start generating images!"
echo ""
