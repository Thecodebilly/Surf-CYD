#!/bin/bash
# Test script for Surf High Score API

API_URL="${1:-http://localhost:3000}"

echo "Testing Surf High Score API at: $API_URL"
echo "================================================"

# Test 1: Health check
echo -e "\n[1] Health Check"
curl -s "$API_URL/health"

# Test 2: Get current high score
echo -e "\n\n[2] Get Current High Score"
curl -s "$API_URL/highscore"

# Test 3: Submit test score
echo -e "\n\n[3] Submit Test Score (500)"
curl -s -X POST "$API_URL/highscore" \
  -H "Content-Type: application/json" \
  -d '{"playerName":"TEST1","score":500}'

# Test 4: Submit higher score
echo -e "\n\n[4] Submit Higher Score (1000)"
curl -s -X POST "$API_URL/highscore" \
  -H "Content-Type: application/json" \
  -d '{"playerName":"TEST2","score":1000}'

# Test 5: Submit lower score
echo -e "\n\n[5] Submit Lower Score (300 - should not be record)"
curl -s -X POST "$API_URL/highscore" \
  -H "Content-Type: application/json" \
  -d '{"playerName":"TEST3","score":300}'

# Test 6: Get leaderboard
echo -e "\n\n[6] Get Leaderboard"
curl -s "$API_URL/leaderboard"

echo -e "\n\n================================================"
echo "Testing complete!"
