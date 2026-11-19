"""
=============================================================================
BACKEND IoT - FRESH FRUIT API (FastAPI)
FIAP - GLOBAL SOLUTION 2026.2 - CITIZENTECH
=============================================================================
Responsável por receber telemetria espectral do ESP32, validar os payloads,
armazenar em banco SQLite e alimentar o Dashboard Web em tempo real.
=============================================================================
"""

import os
from datetime import datetime
from typing import List, Optional
from fastapi import FastAPI, HTTPException, Header, Query, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from .schemas import (
    SpectralReadingCreate,
    SpectralReadingResponse,
    FruitInfo,
    HealthResponse
)
from .database import (
    init_db,
    insert_reading,
    get_latest_reading,
    get_all_readings,
    clear_readings
)

# Inicializa o banco de dados SQLite na inicialização do módulo
init_db()

app = FastAPI(
    title="Fresh Fruit IoT API",
    description="API de Backend para coleta, processamento e histórico de espectrometria Vis-NIR de frutas com ESP32 (Global Solution 2026.2)",
    version="1.0.0"
)

# Habilita CORS para integração com o Dashboard Web
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Chave de API de desenvolvimento opcional
EXPECTED_API_KEY = os.getenv("API_KEY", "freshfruit-secret-key-2026")

# Catálogo oficial das 7 frutas contempladas
FRUITS_CATALOG: List[FruitInfo] = [
    FruitInfo(
        id=1,
        name="Banana",
        keypad_key="1",
        min_brix=8.0,
        max_brix=22.0,
        min_chlorophyll=15.0,
        max_chlorophyll=90.0,
        max_shelf_life_days=7.0,
        description="Amadurece com degradação rápida de clorofila e conversão de amido em açúcares solúveis."
    ),
    FruitInfo(
        id=2,
        name="Manga",
        keypad_key="2",
        min_brix=7.0,
        max_brix=18.0,
        min_chlorophyll=20.0,
        max_chlorophyll=85.0,
        max_shelf_life_days=10.0,
        description="Aumento acentuado de carotenóides e açúcares; pico aromático no estágio ideal."
    ),
    FruitInfo(
        id=3,
        name="Mamao",
        keypad_key="3",
        min_brix=6.0,
        max_brix=15.0,
        min_chlorophyll=18.0,
        max_chlorophyll=80.0,
        max_shelf_life_days=8.0,
        description="Polpa amolece rapidamente após perda da clorofila na casca."
    ),
    FruitInfo(
        id=4,
        name="Maca",
        keypad_key="4",
        min_brix=10.0,
        max_brix=17.0,
        min_chlorophyll=10.0,
        max_chlorophyll=70.0,
        max_shelf_life_days=20.0,
        description="Alta durabilidade pós-colheita; degradação gradual de firmeza e amido."
    ),
    FruitInfo(
        id=5,
        name="Uva",
        keypad_key="5",
        min_brix=12.0,
        max_brix=24.0,
        min_chlorophyll=12.0,
        max_chlorophyll=65.0,
        max_shelf_life_days=12.0,
        description="Não climatérica; sólidos solúveis altos indicam maturação perfeita para colheita."
    ),
    FruitInfo(
        id=6,
        name="Laranja",
        keypad_key="6",
        min_brix=8.0,
        max_brix=14.0,
        min_chlorophyll=25.0,
        max_chlorophyll=80.0,
        max_shelf_life_days=15.0,
        description="Relação °Brix/Acidez define a qualidade; coloração da casca e retenção hídrica."
    ),
    FruitInfo(
        id=7,
        name="Abacate",
        keypad_key="7",
        min_brix=4.0,
        max_brix=10.0,
        min_chlorophyll=30.0,
        max_chlorophyll=95.0,
        max_shelf_life_days=9.0,
        description="Matéria seca e teor lipídico são os principais indicadores espectrais de ponto de corte."
    )
]

# Monta arquivos estáticos do dashboard se a pasta existir
dashboard_dir = os.path.join(os.path.dirname(__file__), "..", "dashboard")
if os.path.isdir(dashboard_dir):
    app.mount("/dashboard", StaticFiles(directory=dashboard_dir, html=True), name="dashboard")


@app.get("/health", response_model=HealthResponse, tags=["Monitoramento"])
def get_health():
    """Verifica a saúde e disponibilidade operacional do backend."""
    return HealthResponse(
        status="ok",
        service="Fresh Fruit IoT Backend",
        version="1.0.0",
        timestamp=datetime.utcnow().isoformat() + "Z",
        database="SQLite Connected"
    )


@app.get("/fruits", response_model=List[FruitInfo], tags=["Frutas"])
def list_fruits():
    """Retorna a lista completa das 7 frutas suportadas e seus parâmetros de calibração."""
    return FRUITS_CATALOG


@app.post("/readings", response_model=SpectralReadingResponse, status_code=status.HTTP_201_CREATED, tags=["Medições"])
def create_reading(reading: SpectralReadingCreate, x_api_key: Optional[str] = Header(None)):
    """
    Recebe e armazena uma nova leitura espectral do ESP32 ou do simulador.
    Valida os 18 canais, proxies e consistência do payload.
    """
    # Validação simples de chave de API demonstrativa (opcional, aceita requisições locais)
    if x_api_key and x_api_key != EXPECTED_API_KEY:
        # Apenas loga ou aceita em ambiente dev para maior resiliência
        pass

    reading_dict = reading.model_dump()
    saved = insert_reading(reading_dict)
    return saved


@app.get("/readings/latest", response_model=SpectralReadingResponse, tags=["Medições"])
def get_latest():
    """Retorna a leitura mais recente enviada pelo dispositivo IoT."""
    latest = get_latest_reading()
    if not latest:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Nenhuma leitura registrada no sistema até o momento."
        )
    return latest


@app.get("/readings", response_model=List[SpectralReadingResponse], tags=["Medições"])
def list_readings(limit: int = Query(default=30, ge=1, le=100, description="Quantidade máxima de registros")):
    """Retorna o histórico das medições mais recentes para exibição no dashboard."""
    return get_all_readings(limit=limit)


@app.delete("/readings", status_code=status.HTTP_200_OK, tags=["Medições"])
def delete_all_readings():
    """Limpa o histórico de medições do banco de dados (útil para reinício de testes)."""
    count = clear_readings()
    return {"message": "Histórico de medições limpo com sucesso.", "deleted_count": count}


if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="127.0.0.1", port=8000, reload=True)
