# Surf High Score API

REST API server for managing global high scores in the Surf Board game.

## Setup

### 1. Database Setup

First, create the `records` table in your PostgreSQL database:

```sql
CREATE TABLE records (
  id SERIAL PRIMARY KEY,
  name VARCHAR(20) NOT NULL,
  score INTEGER NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_score ON records(score DESC);
```

You can use the provided SQL file:

```bash
# Using psql with your database credentials
PGPASSWORD=your_password psql -h your_host -U postgres -p your_port -d your_database -f create_table.sql
```

### 2. Environment Configuration

Create a `.env` file (copy from `.env.example`):

```bash
cp .env.example .env
```

Edit `.env` and set your database connection string:

```
DATABASE_URL=postgresql://username:password@host:port/database
PORT=3000
```

**Important**: Never commit `.env` to version control!

### 3. Install Dependencies

```bash
npm install
```

### 4. Run the Server

Development mode (with auto-reload):
```bash
npm run dev
```

Production mode:
```bash
npm start
```

## API Endpoints

### GET /highscore
Returns the current world record.

Response:
```json
{
  "playerName": "JOHN",
  "score": 1234
}
```

### POST /highscore
Submit a new high score (only saves if it beats the current record).

Request:
```json
{
  "playerName": "JANE",
  "score": 1500
}
```

Response (success):
```json
{
  "success": true,
  "message": "New world record!",
  "playerName": "JANE",
  "score": 1500
}
```

Response (not a record):
```json
{
  "success": false,
  "message": "Not a world record",
  "currentHighScore": 2000
}
```

### GET /leaderboard
Returns top 10 high scores.

Response:
```json
{
  "leaderboard": [
    {
      "rank": 1,
      "playerName": "ALICE",
      "score": 2000,
      "date": "2026-02-26T12:00:00.000Z"
    }
  ]
}
```

### GET /health
Health check endpoint.

Response:
```json
{
  "status": "ok",
  "service": "surf-highscore-api"
}
```

## Deployment

### Railway

1. Create a new project on Railway
2. Add a PostgreSQL database
3. Create a new service from this repository
4. Set the `DATABASE_URL` environment variable to your Railway PostgreSQL connection string
5. Deploy!

### Heroku

1. Create a new Heroku app
2. Add Heroku Postgres addon
3. Deploy this directory
4. The `DATABASE_URL` will be automatically set by Heroku

### Other Platforms

Deploy to any Node.js hosting platform that supports:
- Node.js 16+
- PostgreSQL connection
- Environment variables

## Update ESP32 Configuration

After deploying the API, update the ESP32 code:

1. Open `src/Database.cpp`
2. Replace the `DB_API_URL` with your deployed API URL:
```cpp
const char* DB_API_URL = "https://your-deployed-api.railway.app";
```

3. Rebuild and upload to ESP32

## Security Notes

⚠️ **Important**: This is a basic implementation. For production use, consider:
- Adding authentication/API keys
- Rate limiting to prevent spam
- Input validation and sanitization
- HTTPS enforcement
- Additional verification of high scores
- Never expose database credentials in code
