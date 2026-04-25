import { NextRequest, NextResponse } from "next/server";
import { store, MAX_OCORRENCIAS } from "@/lib/store";
import { Ocorrencia, StatusEvento } from "@/types";

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type",
};

const STATUS_VALIDOS: StatusEvento[] = [
  "DISPARO_ATIVO",
  "ACESSO_NEGADO",
  "POSSE_PERDIDA",
  "SOS_EMERGENCIA",
  "ENVIO_MANUAL",
  "SESSION_TIMEOUT",
  "LOCALIZACAO_PERIODICA",
  "DESCONHECIDO",
];

// ─────────────────────────────────────────────────────────────
//  POST /api/ocorrencias  — ESP32 envia evento aqui
//  Body esperado (JSON):
//  {
//    "id":        "OCC-00001",
//    "idAgente":  "1",
//    "latitude":  -8.8188885,
//    "longitude": 13.2670763,
//    "data":      "2025-07-10",
//    "hora":      "14:33:07",
//    "vezes":     3,
//    "status":    "DISPARO_ATIVO"
//  }
// ─────────────────────────────────────────────────────────────
export async function POST(req: NextRequest) {
  try {
    const body = await req.json();

    const { id, idAgente, latitude, longitude, data, hora, vezes, status } =
      body;

    // Validação de campos obrigatórios
    if (!id || !idAgente || !status) {
      return NextResponse.json(
        { error: "Campos obrigatórios: id, idAgente, status" },
        { status: 400, headers: CORS_HEADERS }
      );
    }

    if (typeof latitude !== "number" || typeof longitude !== "number") {
      return NextResponse.json(
        { error: "latitude e longitude devem ser números" },
        { status: 400, headers: CORS_HEADERS }
      );
    }

    // Normalizar status
    const statusNorm: StatusEvento = STATUS_VALIDOS.includes(status)
      ? status
      : "DESCONHECIDO";

    const ocorrencia: Ocorrencia = {
      id: String(id),
      idAgente: String(idAgente),
      latitude,
      longitude,
      data: data ? String(data) : new Date().toISOString().split("T")[0],
      hora: hora ? String(hora) : new Date().toTimeString().split(" ")[0],
      vezes: typeof vezes === "number" ? vezes : 0,
      status: statusNorm,
      receivedAt: Date.now(),
    };

    // Guardar no store (por ID — atualiza se já existe)
    store[ocorrencia.id] = ocorrencia;

    // Limitar tamanho do store (remover os mais antigos)
    const keys = Object.keys(store);
    if (keys.length > MAX_OCORRENCIAS) {
      const sorted = keys.sort(
        (a, b) => store[a].receivedAt - store[b].receivedAt
      );
      for (let i = 0; i < keys.length - MAX_OCORRENCIAS; i++) {
        delete store[sorted[i]];
      }
    }

    console.log(
      `[ARMA] ${ocorrencia.id} | Agente:${ocorrencia.idAgente} | ${ocorrencia.status} | ${ocorrencia.latitude.toFixed(6)},${ocorrencia.longitude.toFixed(6)}`
    );

    return NextResponse.json(
      { ok: true, id: ocorrencia.id },
      { status: 200, headers: CORS_HEADERS }
    );
  } catch {
    return NextResponse.json(
      { error: "JSON inválido" },
      { status: 400, headers: CORS_HEADERS }
    );
  }
}

// ─────────────────────────────────────────────────────────────
//  GET /api/ocorrencias  — Frontend consulta todas as ocorrências
//  Query params opcionais:
//    ?status=DISPARO_ATIVO   — filtrar por status
//    ?limit=50               — limitar resultados
// ─────────────────────────────────────────────────────────────
export async function GET(req: NextRequest) {
  const { searchParams } = new URL(req.url);
  const filtroStatus = searchParams.get("status");
  const limit = parseInt(searchParams.get("limit") ?? "100", 10);

  const now = Date.now();

  let lista = Object.values(store)
    .map((o) => ({
      ...o,
      ago: Math.floor((now - o.receivedAt) / 1000),
      stale: now - o.receivedAt > 5 * 60 * 1000, // > 5 min
    }))
    .sort((a, b) => b.receivedAt - a.receivedAt); // mais recente primeiro

  if (filtroStatus) {
    lista = lista.filter((o) => o.status === filtroStatus);
  }

  lista = lista.slice(0, Math.min(limit, MAX_OCORRENCIAS));

  return NextResponse.json(lista, {
    headers: {
      ...CORS_HEADERS,
      "Cache-Control": "no-store",
    },
  });
}

// ─────────────────────────────────────────────────────────────
//  OPTIONS — CORS preflight
// ─────────────────────────────────────────────────────────────
export async function OPTIONS() {
  return new NextResponse(null, { status: 204, headers: CORS_HEADERS });
}
