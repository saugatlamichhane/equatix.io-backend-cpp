-- Migration: Add puzzle tracking columns and tables
-- This migration adds missing columns to puzzle_progress and puzzles tables,
-- and creates new tables for daily streaks and daily completions

-- 1. Add missing columns to puzzle_progress table
ALTER TABLE puzzle_progress
ADD COLUMN IF NOT EXISTS attempts INT DEFAULT 0,
ADD COLUMN IF NOT EXISTS best_time INT,
ADD COLUMN IF NOT EXISTS best_score INT,
ADD COLUMN IF NOT EXISTS first_completed_at TIMESTAMP,
ADD COLUMN IF NOT EXISTS last_attempted_at TIMESTAMP DEFAULT now();

-- 2. Add missing columns to puzzles table
ALTER TABLE puzzles
ADD COLUMN IF NOT EXISTS is_daily BOOLEAN DEFAULT FALSE,
ADD COLUMN IF NOT EXISTS daily_date DATE;

-- 3. Create daily_streak table for tracking user streaks
CREATE TABLE IF NOT EXISTS daily_streak (
    id SERIAL PRIMARY KEY,
    user_id VARCHAR(255) NOT NULL UNIQUE,
    current_streak INT DEFAULT 0,
    longest_streak INT DEFAULT 0,
    last_completed_date DATE,
    CONSTRAINT daily_streak_user_id_fkey FOREIGN KEY (user_id) REFERENCES users(uid) ON DELETE CASCADE
);

-- 4. Create daily_completions table for tracking daily puzzle completions
CREATE TABLE IF NOT EXISTS daily_completions (
    id SERIAL PRIMARY KEY,
    user_id VARCHAR(255) NOT NULL,
    puzzle_id INT NOT NULL,
    completed_date DATE NOT NULL,
    completed_at TIMESTAMP DEFAULT now(),
    CONSTRAINT daily_completions_user_id_fkey FOREIGN KEY (user_id) REFERENCES users(uid) ON DELETE CASCADE,
    CONSTRAINT daily_completions_puzzle_id_fkey FOREIGN KEY (puzzle_id) REFERENCES puzzles(puzzle_id) ON DELETE CASCADE,
    CONSTRAINT daily_completions_unique UNIQUE(user_id, puzzle_id, completed_date)
);

-- 5. Create indexes for performance
CREATE INDEX IF NOT EXISTS idx_puzzle_progress_user_id_solved 
    ON puzzle_progress(user_id, solved);

CREATE INDEX IF NOT EXISTS idx_puzzle_progress_user_id_last_attempted 
    ON puzzle_progress(user_id, last_attempted_at);

CREATE INDEX IF NOT EXISTS idx_daily_completions_user_id_date 
    ON daily_completions(user_id, completed_date);

CREATE INDEX IF NOT EXISTS idx_daily_completions_puzzle_id 
    ON daily_completions(puzzle_id);

CREATE INDEX IF NOT EXISTS idx_puzzles_is_daily_date 
    ON puzzles(is_daily, daily_date);
