"""
Camada de Acesso a Dados (DAO) com SQLite para o Fresh Fruit Backend.
Gerencia a persistência das leituras de borda, histórico e consultas.
"""

import sqlite3
import json
import os
from datetime import datetime
from typing import List, Optional, Dict, Any

DB_FILE = os.getenv("DB_PATH", os.path.join(os.path.dirname(__file__), "freshfruit.db"))

def get_connection() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    """Inicializa as tabelas do banco de dados SQLite."""
    conn = get_connection()
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            fruit TEXT NOT NULL,
            maturity_index REAL NOT NULL,
            chlorophyll_proxy REAL NOT NULL,
            soluble_solids_proxy REAL NOT NULL,
            relative_moisture_proxy REAL NOT NULL,
            estimated_shelf_life_days REAL NOT NULL,
            stage TEXT NOT NULL,
            state TEXT NOT NULL,
            spectral_channels TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    """)
    conn.commit()
    conn.close()

def insert_reading(data: Dict[str, Any]) -> Dict[str, Any]:
    """Insere uma nova leitura espectral no banco de dados."""
    conn = get_connection()
    cursor = conn.cursor()
    
    timestamp = datetime.utcnow().isoformat() + "Z"
    channels_json = json.dumps(data.get("spectral_channels", []))

    cursor.execute("""
        INSERT INTO readings (
            device_id, fruit, maturity_index, chlorophyll_proxy,
            soluble_solids_proxy, relative_moisture_proxy,
            estimated_shelf_life_days, stage, state,
            spectral_channels, timestamp
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        data["device_id"],
        data["fruit"],
        data["maturity_index"],
        data["chlorophyll_proxy"],
        data["soluble_solids_proxy"],
        data["relative_moisture_proxy"],
        data["estimated_shelf_life_days"],
        data["stage"],
        data["state"],
        channels_json,
        timestamp
    ))

    new_id = cursor.lastrowid
    conn.commit()
    conn.close()

    result = dict(data)
    result["id"] = new_id
    result["timestamp"] = timestamp
    return result

def get_latest_reading() -> Optional[Dict[str, Any]]:
    """Recupera a última leitura registrada no banco."""
    conn = get_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM readings ORDER BY id DESC LIMIT 1")
    row = cursor.fetchone()
    conn.close()

    if not row:
        return None

    return _row_to_dict(row)

def get_all_readings(limit: int = 50) -> List[Dict[str, Any]]:
    """Recupera o histórico recente de leituras ordenadas por data descendente."""
    conn = get_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM readings ORDER BY id DESC LIMIT ?", (limit,))
    rows = cursor.fetchall()
    conn.close()

    return [_row_to_dict(row) for row in rows]

def clear_readings() -> int:
    """Limpa todo o histórico de medições."""
    conn = get_connection()
    cursor = conn.cursor()
    cursor.execute("DELETE FROM readings")
    deleted_count = cursor.rowcount
    conn.commit()
    conn.close()
    return deleted_count

def _row_to_dict(row: sqlite3.Row) -> Dict[str, Any]:
    channels = []
    try:
        channels = json.loads(row["spectral_channels"])
    except Exception:
        channels = []

    return {
        "id": row["id"],
        "device_id": row["device_id"],
        "fruit": row["fruit"],
        "maturity_index": row["maturity_index"],
        "chlorophyll_proxy": row["chlorophyll_proxy"],
        "soluble_solids_proxy": row["soluble_solids_proxy"],
        "relative_moisture_proxy": row["relative_moisture_proxy"],
        "estimated_shelf_life_days": row["estimated_shelf_life_days"],
        "stage": row["stage"],
        "state": row["state"],
        "spectral_channels": channels,
        "timestamp": row["timestamp"]
    }
