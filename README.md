# Arma Biométrica — Dashboard de Monitorização

Dashboard Next.js 14 (TypeScript/TSX) para monitorização em tempo real de eventos  
gerados pelo ESP32 com leitor biométrico AS608 e módulo GSM/GPS A7670E.

---

## Estrutura do Projecto

```
arma-biometrica/
├── app/
│   ├── api/
│   │   └── ocorrencias/
│   │       └── route.ts        ← API REST (POST + GET)
│   ├── layout.tsx              ← Layout global + Leaflet CDN
│   └── page.tsx                ← Dashboard principal
├── components/
│   └── ArmaMap.tsx             ← Mapa Leaflet (dark theme)
├── lib/
│   └── store.ts                ← Store em memória
├── types/
│   └── index.ts                ← Tipos TypeScript
├── ESP32_ArmaBiometrica_v6_5.ino  ← Firmware ESP32 actualizado
├── package.json
├── tsconfig.json
├── next.config.js
└── vercel.json
```

---

## API — Endpoint

### POST `/api/ocorrencias`
O ESP32 envia cada evento aqui.

**Body JSON:**
```json
{
  "id":        "OCC-00001",
  "idAgente":  "1",
  "latitude":  -8.8188885,
  "longitude": 13.2670763,
  "data":      "2025-07-10",
  "hora":      "14:33:07",
  "vezes":     3,
  "status":    "DISPARO_ATIVO"
}
```

**Valores possíveis de `status`:**
- `DISPARO_ATIVO`
- `ACESSO_NEGADO`
- `POSSE_PERDIDA`
- `SOS_EMERGENCIA`
- `ENVIO_MANUAL`
- `SESSION_TIMEOUT`
- `LOCALIZACAO_PERIODICA`

### GET `/api/ocorrencias`
Devolve todas as ocorrências em memória (mais recentes primeiro).

```
GET /api/ocorrencias?limit=50
GET /api/ocorrencias?status=DISPARO_ATIVO
```

---

## Testar localmente

```bash
npm install
npm run dev
# Abrir: http://localhost:3000
```

Testar a API manualmente:
```bash
curl -X POST http://localhost:3000/api/ocorrencias \
  -H "Content-Type: application/json" \
  -d '{"id":"OCC-00001","idAgente":"1","latitude":-8.8188885,"longitude":13.2670763,"data":"2025-07-10","hora":"14:33:07","vezes":2,"status":"DISPARO_ATIVO"}'
```

---

## Deploy na Vercel — Passo a Passo

### Pré-requisitos
- Conta em [vercel.com](https://vercel.com) (gratuita)
- [Git](https://git-scm.com/) instalado
- [Node.js 18+](https://nodejs.org/) instalado

---

### Passo 1 — Criar repositório GitHub

```bash
cd arma-biometrica
git init
git add .
git commit -m "feat: arma biometrica dashboard v1.0"
```

Aceder a [github.com/new](https://github.com/new) e criar um repositório  
(ex: `arma-biometrica`). Depois:

```bash
git remote add origin https://github.com/SEU_UTILIZADOR/arma-biometrica.git
git branch -M main
git push -u origin main
```

---

### Passo 2 — Ligar à Vercel

1. Aceder a [vercel.com/new](https://vercel.com/new)
2. Clicar em **"Import Git Repository"**
3. Seleccionar o repositório `arma-biometrica`
4. Nas configurações, verificar:
   - **Framework Preset**: Next.js (detectado automaticamente)
   - **Build Command**: `npm run build`
   - **Output Directory**: `.next`
5. Clicar em **"Deploy"**

A Vercel vai construir e publicar o projecto.  
Em ~1-2 minutos receberá um URL do tipo:
```
https://arma-biometrica.vercel.app
```

---

### Passo 3 — Actualizar o URL no ESP32

No ficheiro `ESP32_ArmaBiometrica_v6_5.ino`, alterar a linha:

```cpp
#define SERVIDOR_URL  "https://arma-biometrica.vercel.app/api/ocorrencias"
```

Substituindo `arma-biometrica.vercel.app` pelo URL real atribuído pela Vercel.

Recompilar e gravar no ESP32.

---

### Passo 4 — Verificar o deploy

```bash
# Testar a API em produção
curl -X POST https://SEU-PROJECTO.vercel.app/api/ocorrencias \
  -H "Content-Type: application/json" \
  -d '{"id":"OCC-00001","idAgente":"1","latitude":-8.8188885,"longitude":13.2670763,"data":"2025-07-10","hora":"14:00:00","vezes":1,"status":"LOCALIZACAO_PERIODICA"}'
```

Abrir `https://SEU-PROJECTO.vercel.app` no browser para ver o dashboard.

---

### Passo 5 — Deploys automáticos (opcional)

Cada `git push` para a branch `main` desencadeia um novo deploy automático:

```bash
# Após qualquer alteração ao código:
git add .
git commit -m "fix: ajuste no dashboard"
git push
# → Vercel faz deploy automático em ~1 min
```

---

## Nota sobre persistência de dados

O store actual é **em memória** — os dados perdem-se quando a Vercel reinicia  
a instância (serverless). Para persistência real em produção, recomenda-se:

- **Upstash Redis** (gratuito, integração nativa na Vercel)
- **Vercel Postgres** (plano gratuito disponível)
- **PlanetScale** (MySQL serverless)

A substituição é simples: trocar as operações em `lib/store.ts` pelo  
cliente da base de dados escolhida.

---

## Variáveis de Ambiente (opcional)

Se necessário proteger o endpoint com uma chave API, criar um ficheiro `.env.local`:

```
API_SECRET=chave_secreta_aqui
```

E na Vercel: **Settings → Environment Variables** → adicionar `API_SECRET`.
