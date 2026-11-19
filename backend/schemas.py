"""
Schemas e Modelos de Validação de Dados (Pydantic) para o Fresh Fruit Backend.
Garante tipagem estrita, conformidade dos 18 canais espectrais e integridade do payload.
"""

from typing import List, Optional
from pydantic import BaseModel, Field, field_validator
from datetime import datetime

class SpectralReadingCreate(BaseModel):
    device_id: str = Field(..., description="Identificador único do dispositivo ESP32", example="ESP32-FRESHFRUIT-01")
    fruit: str = Field(..., description="Nome da fruta analisada", example="Banana")
    maturity_index: float = Field(..., ge=0.0, le=100.0, description="Índice de maturação em porcentagem (0-100%)", example=65.4)
    chlorophyll_proxy: float = Field(..., ge=0.0, description="Proxy de clorofila estimado", example=35.2)
    soluble_solids_proxy: float = Field(..., ge=0.0, description="Proxy de sólidos solúveis (°Brix estimado)", example=14.8)
    relative_moisture_proxy: float = Field(..., ge=0.0, le=100.0, description="Proxy de umidade relativa (%)", example=72.0)
    estimated_shelf_life_days: float = Field(..., ge=0.0, description="Estimativa heurística de vida útil em dias", example=3.5)
    stage: str = Field(..., description="Estágio de maturação (Imaturo, Maduro, Muito Maduro, Passado)", example="Maduro / Ideal")
    state: str = Field(..., description="Estado do sistema (NORMAL, ATENCAO, CRITICO, FALHA)", example="NORMAL")
    spectral_channels: List[float] = Field(..., min_length=18, max_length=18, description="Leituras de reflectância dos 18 canais espectrais (AS7265x)")

    @field_validator("spectral_channels")
    @classmethod
    def validate_spectral_channels(cls, v: List[float]) -> List[float]:
        if len(v) != 18:
            raise ValueError("O sensor espectral AS7265x requer exatamente 18 canais.")
        for val in v:
            if val < 0.0 or val > 100.0:
                raise ValueError(f"Valor de canal espectral inválido ({val}). Deve estar entre 0 e 100%.")
        return v

class SpectralReadingResponse(SpectralReadingCreate):
    id: int = Field(..., description="Identificador único do registro no banco de dados")
    timestamp: str = Field(..., description="Data e hora de recebimento da medição no backend (ISO-8601)")

class FruitInfo(BaseModel):
    id: int
    name: str
    keypad_key: str
    min_brix: float
    max_brix: float
    min_chlorophyll: float
    max_chlorophyll: float
    max_shelf_life_days: float
    description: str

class HealthResponse(BaseModel):
    status: str
    service: str
    version: str
    timestamp: str
    database: str
