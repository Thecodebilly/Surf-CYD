# Global High Score System Implementation

## Summary

Successfully implemented a global high score system for the Surf game with PostgreSQL backend and ESP32 integration.

## Changes Made

### 1. Fixed Infinite Shark Spawning ✅
- **Problem**: Sharks stopped spawning after reaching difficulty cap
- **Solution**: 
  - Increased max speed from 10 to 20
  - Reduced minimum spawn interval from 300ms to 100ms
  - Adjusted difficulty curve multiplier from 3 to 2 for more gradual increase
  - Game now continuously gets harder indefinitely

### 2. Created Database API Backend ✅
- **Location**: `/api/` folder
- **Files**:
  - `server.js` - Express.js REST API server
  - `package.json` - Node.js dependencies
  - `README.md` - Deployment and usage guide
  - `.env` - Environment variables (database credentials - not in git)
  - `.env.example` - Example environment configuration

#### Database Connection
Database credentials are now stored in `.env` file and not committed to version control.
See `api/README.md` for setup instructions.

#### API Endpoints
- `GET /highscore` - Returns current world record
- `POST /highscore` - Submit new high score (only if it beats current record)
- `GET /leaderboard` - Returns top 10 scores
- `GET /health` - Health check

#### Database Schema
**Note**: Table name updated from `high_scores` to `records` with simplified schema.
```sql
CREATE TABLE records (
  id SERIAL PRIMARY KEY,
  name VARCHAR(20) NOT NULL,
  score INTEGER NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX idx_score ON records(score DESC);
```

### 3. ESP32 Integration ✅
- **New Files**:
  - `include/Database.h` - Database function declarations
  - `src/Database.cpp` - HTTP client for API communication

#### Features Added
- Fetches global high score at game start
- Displays both local and global high scores on game over screen
- Triggers name input UI when new world record is achieved
- Submits score to database with player name and timestamp

### 4. Name Input Keyboard UI ✅
- **Function**: `getPlayerNameInput()` in `Database.cpp`
- **Features**:
  - On-screen QWERTY keyboard
  - Letters A-Z and numbers 0-9
  - Backspace key (2-button width)
  - OK/Submit key (2-button width)
  - Blinking cursor
  - Max 12 character name
  - Touch-based input
  - Clean, themed design matching app style

### 5. Game.cpp Updates ✅
- Added global high score tracking
- Modified game over screen to show:
  - Player's score
  - Local best (personal high score)
  - World record with player name
  - "NEW LOCAL RECORD!" message if applicable
  - "WORLD RECORD!" message if new global high score
- Prompts for name when world record is beaten
- Shows submission status (success/failure)

## Deployment Instructions

### Step 1: Deploy the API Server

#### Option A: Railway (Recommended)
Railway is recommended since your PostgreSQL database is already hosted there.

1. Install Railway CLI:
```bash
npm install -g @railway/cli
```

2. Navigate to API folder and deploy:
```bash
cd api
railway login
railway init
railway up
```

3. Note the deployed URL (e.g., `https://your-app.railway.app`)

#### Option B: Heroku
```bash
cd api
heroku create surf-highscore-api
git init
git add .
git commit -m "Initial commit"
git push heroku main
```

#### Option C: Vercel (Serverless)
```bash
cd api
npm install -g vercel
vercel
```

### Step 2: Update ESP32 Configuration

1. Open `src/Database.cpp`
2. Find line 12:
```cpp
const char* DB_API_URL = "https://your-api-endpoint.com";
```

3. Replace with your deployed API URL:
```cpp
const char* DB_API_URL = "https://your-app.railway.app";  // No trailing slash
```

4. Rebuild and upload:
```bash
pio run --target upload
```

### Step 3: Test the System

1. Play the surfing game
2. Try to beat the current world record
3. When prompted, enter your name using the on-screen keyboard
4. Verify submission was successful
5. Check the database or call `GET /leaderboard` to see your score

## Testing the API Locally

Before deploying, you can test locally:

```bash
cd api
npm install
npm start
```

Server will run on `http://localhost:3000`

Test endpoints:
```bash
# Get current high score
curl http://localhost:3000/highscore

# Submit new score
curl -X POST http://localhost:3000/highscore \
  -H "Content-Type: application/json" \
  -d '{"playerName":"TEST","score":5000,"timestamp":1234567890}'

# Get leaderboard
curl http://localhost:3000/leaderboard
```

## Game Mechanics

### Difficulty Progression
- **Speed**: Starts at 5, increases by 1 every 50 points, caps at 20
- **Spawn Rate**: Starts at 800ms, decreases by 2ms per point, minimum 100ms
- **Obstacles**: Up to 12 shark fins active simultaneously

### High Score System
- **Local High Score**: Saved to `/high_score.json` on ESP32
- **Global High Score**: Fetched from PostgreSQL on game start
- **World Record Trigger**: Activated when score > global high score
- **Name Entry**: On-screen keyboard with A-Z, 0-9, backspace, OK
- **Submission**: HTTP POST to API with player name, score, timestamp

## File Structure

```
Surf-CYD/
├── api/
│   ├── server.js          # Express API server
│   ├── package.json       # Node dependencies
│   └── README.md          # API documentation
├── include/
│   └── Database.h         # Database function declarations
└── src/
    ├── Database.cpp       # Database implementation
    └── Game.cpp           # Updated with global high scores
```

## Memory Usage

- **RAM**: 14.5% (47,364 bytes / 327,680 bytes)
- **Flash**: 33.8% (1,062,093 bytes / 3,145,728 bytes)

## Security Notes

⚠️ **Important for Production**:

The current implementation is basic. For production use, consider adding:

1. **API Key Authentication**: Prevent unauthorized submissions
2. **Rate Limiting**: Prevent spam/abuse
3. **Score Validation**: Verify scores are reasonable (not hacked)
4. **Input Sanitization**: Prevent SQL injection (using parameterized queries)
5. **HTTPS Only**: Enforce secure connections
6. **CORS Configuration**: Limit allowed origins

Example API key implementation:
```cpp
// In Database.cpp
http.addHeader("X-API-Key", "your-secret-api-key");
```

```javascript
// In server.js
app.use((req, res, next) => {
  const apiKey = req.headers['x-api-key'];
  if (apiKey !== process.env.API_KEY) {
    return res.status(401).json({ error: 'Unauthorized' });
  }
  next();
});
```

## Future Enhancements

Potential improvements:
- [ ] Display top 10 leaderboard in-game
- [ ] Add player profiles with stats (games played, average score, etc.)
- [ ] Regional leaderboards (continent/country based)
- [ ] Daily/weekly/all-time leaderboards
- [ ] Achievement system
- [ ] Score replay/verification system
- [ ] Social sharing (generate shareable score images)

## Troubleshooting

### ESP32 can't connect to API
- Check WiFi connection
- Verify API URL is correct (no trailing slash)
- Ensure API server is running
- Check API server logs for errors

### Scores not saving to database
- Verify PostgreSQL connection string
- Check database table was created
- Review API server logs
- Ensure WiFi is connected on ESP32

### Name input UI not appearing
- Ensure score is actually higher than global high score
- Check serial monitor for errors
- Verify touch screen is calibrated

### API deployment issues
- Check platform-specific logs (Railway/Heroku/Vercel)
- Verify environment variables if using them
- Ensure PostgreSQL connection string is correct
- Check firewall/security group settings

## Credits

- **PostgreSQL Database**: Railway (caboose.proxy.rlwy.net)
- **ESP32 Framework**: Arduino/PlatformIO
- **HTTP Client**: ESP32 HTTPClient library
- **API Framework**: Express.js
- **Database Driver**: node-postgres (pg)
