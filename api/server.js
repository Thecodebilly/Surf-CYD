// Surf High Score API Server
// This server connects to PostgreSQL and provides REST API for ESP32
// Deploy this to a cloud service (Heroku, Railway, Vercel, etc.)

require('dotenv').config();
const express = require('express');
const { Pool } = require('pg');
const cors = require('cors');

const app = express();
const port = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());

// Simple API key authentication middleware (optional but recommended)
const API_KEY = process.env.API_KEY || null;

function requireApiKey(req, res, next) {
  if (!API_KEY) {
    // If no API key is configured, skip authentication
    return next();
  }
  
  const providedKey = req.headers['x-api-key'] || req.query.apikey;
  
  if (providedKey === API_KEY) {
    next();
  } else {
    res.status(401).json({ error: 'Unauthorized - Invalid or missing API key' });
  }
}

// PostgreSQL connection - use DATABASE_URL environment variable
if (!process.env.DATABASE_URL) {
  console.error('ERROR: DATABASE_URL environment variable is not set');
  console.error('Please create a .env file with DATABASE_URL=your_connection_string');
  process.exit(1);
}

const pool = new Pool({
  connectionString: process.env.DATABASE_URL,
  ssl: {
    rejectUnauthorized: false  // Required for Railway and similar services
  }
});

// Test database connection on startup
async function testConnection() {
  const client = await pool.connect();
  try {
    const result = await client.query('SELECT NOW()');
    console.log('Database connected successfully at:', result.rows[0].now);
    
    // Verify records table exists
    const tableCheck = await client.query(`
      SELECT EXISTS (
        SELECT FROM information_schema.tables 
        WHERE table_name = 'records'
      )
    `);
    
    if (!tableCheck.rows[0].exists) {
      console.error('ERROR: records table does not exist in database');
      console.error('Please create the table manually with:');
      console.error(`
        CREATE TABLE records (
          id SERIAL PRIMARY KEY,
          name VARCHAR(20) NOT NULL,
          score INTEGER NOT NULL,
          created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX idx_score ON records(score DESC);
      `);
      process.exit(1);
    }
    
    console.log('Records table verified');
  } catch (err) {
    console.error('Error connecting to database:', err);
    process.exit(1);
  } finally {
    client.release();
  }
}

// Get global high score
app.get('/highscore', requireApiKey, async (req, res) => {
  try {
    const result = await pool.query(
      'SELECT name, score FROM records ORDER BY score DESC LIMIT 1'
    );
    
    if (result.rows.length > 0) {
      res.json({
        playerName: result.rows[0].name,
        score: result.rows[0].score
      });
    } else {
      res.json({
        playerName: '---',
        score: 0
      });
    }
  } catch (err) {
    console.error('Error fetching high score:', err);
    res.status(500).json({ error: 'Failed to fetch high score' });
  }
});

// Submit new high score
app.post('/highscore', requireApiKey, async (req, res) => {
  const { playerName, score, timestamp } = req.body;
  
  if (!playerName || score === undefined || score === null) {
    return res.status(400).json({ error: 'Missing required fields' });
  }
  
  try {
    // Check if this is actually a new high score
    const currentHigh = await pool.query(
      'SELECT score FROM records ORDER BY score DESC LIMIT 1'
    );
    
    const currentHighScore = currentHigh.rows.length > 0 ? currentHigh.rows[0].score : 0;
    
    if (score > currentHighScore) {
      // Insert the new high score
      await pool.query(
        'INSERT INTO records (name, score) VALUES ($1, $2)',
        [playerName.substring(0, 20), score]
      );
      
      console.log(`New world record! ${playerName}: ${score}`);
      res.status(201).json({ 
        success: true, 
        message: 'New world record!',
        playerName,
        score
      });
    } else {
      res.status(200).json({ 
        success: false, 
        message: 'Not a world record',
        currentHighScore
      });
    }
  } catch (err) {
    console.error('Error submitting high score:', err);
    res.status(500).json({ error: 'Failed to submit high score' });
  }
});

// Get top 10 high scores (bonus endpoint)
app.get('/leaderboard', requireApiKey, async (req, res) => {
  try {
    const result = await pool.query(
      'SELECT name, score, created_at FROM records ORDER BY score DESC LIMIT 10'
    );
    
    res.json({
      leaderboard: result.rows.map((row, index) => ({
        rank: index + 1,
        playerName: row.name,
        score: row.score,
        date: row.created_at
      }))
    });
  } catch (err) {
    console.error('Error fetching leaderboard:', err);
    res.status(500).json({ error: 'Failed to fetch leaderboard' });
  }
});

// Health check endpoint
app.get('/health', (req, res) => {
  res.json({ status: 'ok', service: 'surf-highscore-api' });
});

// Start server
app.listen(port, async () => {
  console.log(`Surf High Score API listening on port ${port}`);
  await testConnection();
});

// Graceful shutdown
process.on('SIGTERM', () => {
  console.log('SIGTERM signal received: closing HTTP server');
  pool.end();
  process.exit(0);
});
