# Plano e Matriz de Testes — Fresh Fruit

> **FIAP · Global Solution 2026.2 · CitizenTech**  
> **Projeto:** Fresh Fruit — Monitoramento Espectral e Maturação de Frutas  

---

## 1. Matriz de Casos de Teste

| ID | Cenário / Funcionalidade | Procedimento | Resultado Esperado | Status |
|:---:|:---|:---|:---|:---:|
| **CT-01** | **Inicialização do ESP32** | Ligar simulação no Wokwi | Serial exibe banner e integrantes; LCD exibe "FRESH FRUIT GS 2026.2"; LED RGB acende em Azul (Idle). | APROVADO |
| **CT-02** | **Seleção de Fruta via Teclado** | Pressionar teclas `1` a `7` | LCD exibe o nome da fruta correspondente e solicita confirmação com `#`. | APROVADO |
| **CT-03** | **Confirmação da Fruta** | Pressionar tecla `#` após selecionar fruta | Buzzer emite bipe de confirmação; LCD entra no modo de análise contínua. | APROVADO |
| **CT-04** | **Cancelamento da Seleção** | Pressionar tecla `*` a qualquer momento | Sistema reseta para o estado inicial de aguardo (LED Azul, LCD limpo). | APROVADO |
| **CT-05** | **Estado Normal (Fruta Madura)** | Ajustar potenciômetro em nível intermediário (~60%) | LED RGB acende em Verde; LCD exibe `[NORMAL]`, índice de maturação e °Brix; Buzzer permanece desligado. | APROVADO |
| **CT-06** | **Estado de Atenção (Fruta Verde)** | Ajustar potenciômetro em nível baixo (~20%) | LED RGB acende em Amarelo (R+G); LCD exibe `[ATENCAO] Verde`; Buzzer desligado. | APROVADO |
| **CT-07** | **Estado de Atenção (Muito Madura)** | Ajustar potenciômetro em nível alto (~85%) | LED RGB acende em Amarelo; LCD exibe `[ATENCAO] MtMad`; Alerta para consumo rápido. | APROVADO |
| **CT-08** | **Estado Crítico (Passada/Deteriorada)** | Ajustar potenciômetro no máximo (~98%) | LED RGB acende em Vermelho; Buzzer dispara alarme pulsante; LCD exibe `[CRITICO] Passad`. | APROVADO |
| **CT-09** | **Suavização de Leituras** | Mudar potenciômetro bruscamente | Média móvel suaviza a transição dos 18 canais sem oscilações espúrias. | APROVADO |
| **CT-10** | **Saúde do Backend (GET /health)** | Executar `curl http://127.0.0.1:8000/health` | Retorna HTTP 200 com JSON `{"status": "ok", "service": "Fresh Fruit IoT Backend"}`. | APROVADO |
| **CT-11** | **Catálogo de Frutas (GET /fruits)** | Executar `GET /fruits` | Retorna lista completa das 7 frutas com limites de Brix e Clorofila. | APROVADO |
| **CT-12** | **Ingestão de Medição (POST /readings)** | Enviar payload JSON com 18 canais | Retorna HTTP 201; Registro persistido na tabela `readings` do SQLite. | APROVADO |
| **CT-13** | **Última Leitura (GET /readings/latest)** | Consultar endpoint | Retorna a medição mais recente salva no banco. | APROVADO |
| **CT-14** | **Dashboard Web em Tempo Real** | Abrir `dashboard/index.html` com backend rodando | Badge exibe "Backend Conectado"; Cards, gráfico Chart.js e histórico atualizam a cada 2.5s. | APROVADO |
| **CT-15** | **Autonomia Offline (Edge Computing)** | Desligar o backend ou desconectar Wi-Fi | ESP32 continua realizando 100% das leituras, cálculos, display LCD, LED RGB e buzzer sem travar. | APROVADO |

---

## 2. Instruções de Teste de Ponta a Ponta (E2E)

### Etapa 1: Validação do Backend
```powershell
# 1. Iniciar o servidor FastAPI
cd backend
python -m uvicorn main:app --host 127.0.0.1 --port 8000

# 2. Em outro terminal, testar o endpoint /health
curl -i http://127.0.0.1:8000/health
```

### Etapa 2: Validação do Dashboard
- Abra `dashboard/index.html` no navegador.
- Clique no botão **"🧪 Simulador Manual"** e selecione "Banana" e "Maduro / Ideal".
- Clique em **"🚀 Enviar Leitura ao Backend"**.
- Verifique que o cartão da banana é atualizado instantaneamente, o gráfico espectral desenha os 18 canais e a linha é inserida na tabela de histórico.

### Etapa 3: Validação da Borda no Wokwi
- Inicie a simulação no Wokwi.
- Pressione a tecla `1` no keypad e depois `#`.
- Varie o potenciômetro e confira a mudança de cores do LED RGB (Verde -> Amarelo -> Vermelho + Buzzer).
