# Backend Requirements for Puzzle System

## Overview
This document outlines the API endpoints and data structures needed to support the frontend puzzle system functionality.

---

## 1. Puzzle Management Endpoints

### GET `/puzzles`
**Description**: Fetch all puzzles with optional filtering and sorting

**Query Parameters**:
- `difficulty` (optional): `easy`, `medium`, `hard`, or `all`
- `limit` (optional): Number of results (default: 50)
- `offset` (optional): Pagination offset (default: 0)
- `sort` (optional): `recent`, `oldest`, `completed`, `pending`
- `search` (optional): Search term for puzzle name/objective

**Response**:
```json
{
  "data": [
    {
      "puzzle_id": 1,
      "difficulty": "medium",
      "objective": "Create an equation that equals 15",
      "solved": false,
      "created_at": "2025-11-10T10:00:00Z",
      "created_by": "user_id",
      "attempts": 0,
      "best_time": null
    }
  ],
  "total": 50,
  "limit": 10,
  "offset": 0
}
```

**Status Codes**:
- `200 OK`: Success
- `401 Unauthorized`: Not authenticated
- `500 Internal Server Error`: Server error

---

### GET `/puzzle/:puzzleId`
**Description**: Get a single puzzle with its board setup and rack tiles

**URL Parameters**:
- `puzzleId` (required): The puzzle ID

**Response**:
```json
{
  "puzzle_id": 1,
  "difficulty": "medium",
  "objective": "Create an equation that equals 15",
  "board": [
    {"row": 6, "col": 5, "value": "3"},
    {"row": 6, "col": 6, "value": "+"},
    {"row": 6, "col": 7, "value": "12"},
    {"row": 7, "col": 7, "value": "="},
    {"row": 8, "col": 7, "value": "15"}
  ],
  "rack": ["2", "4", "1", "5", "9", "-", "=", "10", "8", "6"],
  "created_at": "2025-11-10T10:00:00Z",
  "created_by": "user_id"
}
```

**Notes**:
- Board coordinates are **1-based** (row 1-15, col 1-15)
- Initial tiles cannot be moved (locked on board)
- Rack contains tiles available for the player

**Status Codes**:
- `200 OK`: Success
- `401 Unauthorized`: Not authenticated
- `404 Not Found`: Puzzle not found
- `500 Internal Server Error`: Server error

---

### POST `/puzzle/:puzzleId/submit`
**Description**: Submit puzzle completion and save user's result

**URL Parameters**:
- `puzzleId` (required): The puzzle ID

**Request Body**:
```json
{
  "completion_time": 245,
  "final_score": 350,
  "attempts": 3
}
```

**Response**:
```json
{
  "success": true,
  "message": "Puzzle completed successfully",
  "completion_record": {
    "puzzle_id": 1,
    "user_id": "user_123",
    "completed_at": "2025-11-10T10:45:30Z",
    "completion_time": 245,
    "final_score": 350,
    "attempts": 3
  }
}
```

**Status Codes**:
- `200 OK`: Success
- `401 Unauthorized`: Not authenticated
- `400 Bad Request`: Invalid data
- `404 Not Found`: Puzzle not found
- `500 Internal Server Error`: Server error

---

## 2. Daily Puzzle Endpoints

### GET `/puzzles/daily`
**Description**: Get today's daily puzzle (one puzzle per day for all users)

**Response**:
```json
{
  "puzzle_id": 42,
  "difficulty": "easy",
  "objective": "Create an equation that equals 20",
  "completed_today": false,
  "reset_at": "2025-11-11T00:00:00Z",
  "board": [...],
  "rack": [...]
}
```

**Notes**:
- Returns the same puzzle for all users on a given day
- Puzzles reset at UTC midnight
- `completed_today` indicates if current user has completed it
- Should be cached on client (daily doesn't change until reset)

**Status Codes**:
- `200 OK`: Success
- `401 Unauthorized`: Not authenticated
- `500 Internal Server Error`: Server error

---

### GET `/puzzles/streak`
**Description**: Get user's daily puzzle streak information

**Response**:
```json
{
  "user_id": "user_123",
  "current_streak": 7,
  "longest_streak": 12,
  "last_completed_date": "2025-11-10",
  "daily_history": [
    "2025-11-10",
    "2025-11-09",
    "2025-11-08",
    "2025-11-07",
    "2025-11-06",
    "2025-11-05",
    "2025-11-04"
  ],
  "total_daily_completed": 42
}
```

**Notes**:
- `current_streak`: Number of consecutive days completed (resets if missed a day)
- `daily_history`: Last 30 days of completion dates
- Should verify completion only after today's puzzle is submitted

**Status Codes**:
- `200 OK`: Success
- `401 Unauthorized`: Not authenticated
- `500 Internal Server Error`: Server error

---

## 3. Validation Endpoints

### POST `/puzzle/validateMove`
**Description**: Validate a puzzle move (existing endpoint - needs minor updates)

**Request Body**:
```json
{
  "puzzle_id": 1,
  "placed_tiles": [
    {"row": 6, "col": 8, "value": "4"},
    {"row": 6, "col": 9, "value": "1"},
    {"row": 6, "col": 10, "value": "1"}
  ]
}
```

**Response (Success)**:
```json
{
  "valid": true,
  "message": "Move is valid! Puzzle solved!",
  "score": 350,
  "streak_updated": true
}
```

**Response (Failure)**:
```json
{
  "valid": false,
  "message": "Invalid equation. 3+12≠15 (got 15... wait that's correct!) - This would be valid",
  "closest_value": 15
}
```

**Status Codes**:
- `200 OK`: Valid move
- `400 Bad Request`: Invalid move (move is wrong)
- `401 Unauthorized`: Not authenticated
- `404 Not Found`: Puzzle not found
- `500 Internal Server Error`: Server error

---

## 4. User Progress Endpoints

### GET `/user/puzzles/progress`
**Description**: Get all user puzzle completion progress

**Response**:
```json
{
  "data": [
    {
      "puzzle_id": 1,
      "solved": true,
      "attempts": 2,
      "best_time": 245,
      "first_completed": "2025-11-08T10:15:30Z",
      "last_attempted": "2025-11-10T10:45:30Z"
    },
    {
      "puzzle_id": 2,
      "solved": false,
      "attempts": 3,
      "best_time": null,
      "first_completed": null,
      "last_attempted": "2025-11-10T09:20:15Z"
    }
  ],
  "total_solved": 15,
  "total_attempted": 28
}
```

**Query Parameters**:
- `limit` (optional): Results per page
- `offset` (optional): Pagination offset

**Status Codes**:
- `200 OK`: Success
- `401 Unauthorized`: Not authenticated
- `500 Internal Server Error`: Server error

---

### GET `/user/stats`
**Description**: Get user's puzzle statistics

**Response**:
```json
{
  "user_id": "user_123",
  "display_name": "John Doe",
  "total_solved": 15,
  "total_attempted": 28,
  "solve_rate": 0.536,
  "average_time": 287,
  "fastest_time": 45,
  "slowest_time": 1250,
  "longest_streak": 12,
  "current_streak": 7,
  "daily_streak_active": true,
  "difficulty_stats": {
    "easy": {"solved": 8, "attempted": 9},
    "medium": {"solved": 5, "attempted": 12},
    "hard": {"solved": 2, "attempted": 7}
  },
  "recent_completions": [
    {
      "puzzle_id": 1,
      "solved_at": "2025-11-10T10:45:30Z",
      "time": 245,
      "attempts": 3
    }
  ]
}
```

**Status Codes**:
- `200 OK`: Success
- `401 Unauthorized`: Not authenticated
- `500 Internal Server Error`: Server error

---

## 5. Database Schema Requirements

### `puzzles` Table
```sql
CREATE TABLE puzzles (
  puzzle_id INT PRIMARY KEY AUTO_INCREMENT,
  difficulty VARCHAR(20) NOT NULL,
  objective TEXT NOT NULL,
  board_setup JSON NOT NULL, -- [{row, col, value}, ...]
  rack_tiles JSON NOT NULL,  -- ["1", "2", "+", ...]
  created_by UUID NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  is_daily BOOLEAN DEFAULT FALSE,
  daily_date DATE NULL,
  FOREIGN KEY (created_by) REFERENCES users(user_id),
  INDEX (difficulty),
  INDEX (daily_date),
  INDEX (is_daily)
);
```

### `puzzle_progress` Table
```sql
CREATE TABLE puzzle_progress (
  progress_id INT PRIMARY KEY AUTO_INCREMENT,
  user_id UUID NOT NULL,
  puzzle_id INT NOT NULL,
  solved BOOLEAN DEFAULT FALSE,
  attempts INT DEFAULT 0,
  best_time INT NULL, -- seconds
  best_score INT NULL,
  first_completed_at TIMESTAMP NULL,
  last_attempted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (user_id) REFERENCES users(user_id),
  FOREIGN KEY (puzzle_id) REFERENCES puzzles(puzzle_id),
  UNIQUE KEY (user_id, puzzle_id),
  INDEX (solved),
  INDEX (user_id)
);
```

### `daily_streak` Table
```sql
CREATE TABLE daily_streak (
  streak_id INT PRIMARY KEY AUTO_INCREMENT,
  user_id UUID NOT NULL,
  current_streak INT DEFAULT 0,
  longest_streak INT DEFAULT 0,
  last_completed_date DATE NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  FOREIGN KEY (user_id) REFERENCES users(user_id),
  UNIQUE KEY (user_id),
  INDEX (last_completed_date)
);
```

### `daily_completions` Table
```sql
CREATE TABLE daily_completions (
  completion_id INT PRIMARY KEY AUTO_INCREMENT,
  user_id UUID NOT NULL,
  puzzle_id INT NOT NULL,
  completed_date DATE NOT NULL,
  completion_time INT NOT NULL, -- seconds
  final_score INT NOT NULL,
  attempts INT NOT NULL,
  completed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (user_id) REFERENCES users(user_id),
  FOREIGN KEY (puzzle_id) REFERENCES puzzles(puzzle_id),
  UNIQUE KEY (user_id, puzzle_id, completed_date),
  INDEX (user_id),
  INDEX (completed_date)
);
```

---

## 6. Business Logic Requirements

### Scoring System
- **Base Value**: Each number tile = its numeric value, operators = 1 point
- **Tile Multipliers**: 
  - Double Tile (2x): Double the tile value
  - Triple Tile (3x): Triple the tile value
- **Equation Multipliers**: 
  - Double Equation (2x): Double entire equation score
  - Triple Equation (3x): Triple entire equation score

### Streak Logic
- **Definition**: Consecutive days of completing the daily puzzle
- **Reset**: Streak resets if user misses a day
- **Verification**: Only count completion if submitted between daily reset and next reset
- **Timezone**: Should use user's timezone or UTC (consistent)

### Validation Rules
- All equations must be mathematically correct
- Operators must be properly placed
- Numbers can be multi-digit
- Must use at least one new tile (can't just confirm existing board)

---

## 7. Authentication & Authorization

- All endpoints (except public ones like GET /puzzles for browsing) require a valid Firebase JWT token
- Token passed in `Authorization: Bearer <token>` header
- Backend validates token and extracts user ID
- Each puzzle return should only include data user is authorized to see

---

## 8. Error Handling

All error responses should follow this format:
```json
{
  "error": true,
  "message": "Human-readable error message",
  "code": "ERROR_CODE",
  "details": {}
}
```

Common error codes:
- `NOT_FOUND`: Resource doesn't exist
- `UNAUTHORIZED`: Not authenticated
- `FORBIDDEN`: Not authorized
- `INVALID_DATA`: Request data is invalid
- `VALIDATION_FAILED`: Move validation failed
- `SERVER_ERROR`: Unexpected server error

---

## 9. Performance Considerations

1. **Caching**:
   - Cache `/puzzles/daily` for 1 minute (same for all users)
   - Cache individual puzzle data for 5 minutes
   - Cache user stats for 1 minute

2. **Rate Limiting**:
   - Limit validateMove to 10 requests per minute per user
   - Limit puzzle submission to 5 per minute per puzzle

3. **Pagination**:
   - Default limit: 20 items
   - Max limit: 100 items
   - Include `total`, `limit`, `offset` in responses

---

## 10. Development Checklist

- [ ] Implement GET `/puzzles` with filtering/sorting
- [ ] Implement GET `/puzzle/:puzzleId`
- [ ] Implement POST `/puzzle/:puzzleId/submit`
- [ ] Implement GET `/puzzles/daily`
- [ ] Implement GET `/puzzles/streak`
- [ ] Update POST `/puzzle/validateMove` for better error messages
- [ ] Implement GET `/user/puzzles/progress`
- [ ] Implement GET `/user/stats`
- [ ] Create database tables
- [ ] Add input validation on all endpoints
- [ ] Add error handling middleware
- [ ] Add logging
- [ ] Add tests for validation logic
- [ ] Setup caching layer
- [ ] Setup rate limiting

