#!/bin/bash
# Test script for EcoDex Pl@ntNet API

echo "🌿 Testing EcoDex API with Pl@ntNet"
echo "===================================="

# Check if API key is set
if [ -z "$PLANTNET_API_KEY" ]; then
    echo "❌ Error: PLANTNET_API_KEY not set"
    echo "Run: export PLANTNET_API_KEY=your_key_here"
    exit 1
fi

# Test 1: Direct Pl@ntNet API call with sample image URL
echo ""
echo "Test 1: Direct Pl@ntNet API call"
echo "--------------------------------"

SAMPLE_IMAGE="https://bs.plantnet.org/image/o/1a2b3c4d5e6f7g8h9i0j"

curl -s "https://my-api.plantnet.org/v2/identify/useful?api-key=$PLANTNET_API_KEY&images=$SAMPLE_IMAGE&organs=leaf" \
  | python -m json.tool | head -n 30

# Test 2: Check remaining identifications
echo ""
echo "Test 2: Check API usage"
echo "----------------------"

curl -s "https://my-api.plantnet.org/v2/identify/useful?api-key=$PLANTNET_API_KEY&images=$SAMPLE_IMAGE&organs=leaf" \
  | grep -o '"remainingIdentificationRequests":[0-9]*'

# Test 3: Test your local server (if running)
echo ""
echo "Test 3: Test local EcoDex server"
echo "--------------------------------"

if curl -s http://localhost:3000/health > /dev/null 2>&1; then
    echo "✅ Server is running"
    
    # Test with a sample image (you need to have a test image)
    if [ -f "test-plant.jpg" ]; then
        echo "Testing plant identification..."
        curl -X POST http://localhost:3000/api/identify \
          -F "image=@test-plant.jpg" \
          -F "organ=leaf" \
          | python -m json.tool
    else
        echo "⚠️  No test-plant.jpg found. Create one to test image upload."
    fi
else
    echo "⚠️  Server not running. Start with: npm start"
fi

echo ""
echo "===================================="
echo "✅ Tests complete!"
