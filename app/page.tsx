"use client";

import { useEffect, useState, useCallback, useMemo } from "react";
import dynamic from "next/dynamic";
import { OcorrenciaEnriquecida, StatusEvento } from "@/types";

const ArmaMap = dynamic(() => import("@/components/ArmaMap"), { ssr: false });

// ─────────────────────────────────────────────────────
//  Constantes de estilo
// ─────────────────────────────────────────────────────
const C = {
  bg: "#080810",
  bgCard: "#0d0d1c",
  bgSidebar: "#0a0a18",
  border: "#1a1a30",
  text: "#e0e0f0",
  textMuted: "#555",
  textSub: "#888",
  accent: "#00f5d4",
  font: "'JetBrains Mono', 'Fira Code', monospace",
} as const;

const STATUS_COLOR: Record<StatusEvento, string> = {
  DISPARO_ATIVO: "#f72585",
  ACESSO_NEGADO: "#ff6b35",
  POSSE_PERDIDA: "#fee440",
  SOS_EMERGENCIA: "#ff2222",
  ENVIO_MANUAL: "#4cc9f0",
  SESSION_TIMEOUT: "#9b5de5",
  LOCALIZACAO_PERIODICA: "#00f5d4",
  DESCONHECIDO: "#666",
};

const STATUS_EMOJI: Record<StatusEvento, string> = {
  DISPARO_ATIVO: "🔴",
  ACESSO_NEGADO: "🚫",
  POSSE_PERDIDA: "⚠️",
  SOS_EMERGENCIA: "🆘",
  ENVIO_MANUAL: "📡",
  SESSION_TIMEOUT: "⏰",
  LOCALIZACAO_PERIODICA: "📍",
  DESCONHECIDO: "❓",
};

const STATUS_LABEL: Record<StatusEvento, string> = {
  DISPARO_ATIVO: "Disparo Activo",
  ACESSO_NEGADO: "Acesso Negado",
  POSSE_PERDIDA: "Posse Perdida",
  SOS_EMERGENCIA: "SOS Emergência",
  ENVIO_MANUAL: "Envio Manual",
  SESSION_TIMEOUT: "Timeout Sessão",
  LOCALIZACAO_PERIODICA: "Localização",
  DESCONHECIDO: "Desconhecido",
};

const AGENTE_LABEL: Record<string, string> = {
  "0": "Sistema",
  "1": "Prop. (Polegar Dir.)",
  "2": "Prop. (Polegar Esq.)",
};

function descAgente(id: string): string {
  return AGENTE_LABEL[id] ?? `Agente ID ${id}`;
}

function timeAgo(ago: number): string {
  if (ago < 5) return "agora";
  if (ago < 60) return `${ago}s`;
  if (ago < 3600) return `${Math.floor(ago / 60)}m`;
  return `${Math.floor(ago / 3600)}h`;
}

// ─────────────────────────────────────────────────────
//  Sub-componentes
// ─────────────────────────────────────────────────────
function StatCard({
  label,
  value,
  color,
  emoji,
}: {
  label: string;
  value: number | string;
  color: string;
  emoji: string;
}) {
  return (
    <div
      style={{
        background: C.bgCard,
        border: `1px solid ${C.border}`,
        borderRadius: 10,
        padding: "12px 16px",
        display: "flex",
        flexDirection: "column",
        gap: 4,
        minWidth: 120,
        flex: 1,
      }}
    >
      <div style={{ fontSize: 11, color: C.textMuted, letterSpacing: "0.1em" }}>
        {emoji} {label}
      </div>
      <div
        style={{
          fontSize: 26,
          fontWeight: 700,
          color,
          lineHeight: 1.1,
        }}
      >
        {value}
      </div>
    </div>
  );
}

function Badge({ status }: { status: StatusEvento }) {
  const color = STATUS_COLOR[status];
  return (
    <span
      style={{
        fontSize: 9,
        padding: "2px 7px",
        borderRadius: 10,
        background: `${color}18`,
        color,
        border: `1px solid ${color}33`,
        fontWeight: 700,
        letterSpacing: "0.08em",
        whiteSpace: "nowrap",
      }}
    >
      {STATUS_EMOJI[status]} {STATUS_LABEL[status].toUpperCase()}
    </span>
  );
}

// ─────────────────────────────────────────────────────
//  FILTROS disponíveis
// ─────────────────────────────────────────────────────
const FILTROS: Array<{ label: string; value: StatusEvento | "TODOS" }> = [
  { label: "Todos", value: "TODOS" },
  { label: "Disparo", value: "DISPARO_ATIVO" },
  { label: "Acesso Negado", value: "ACESSO_NEGADO" },
  { label: "Posse Perdida", value: "POSSE_PERDIDA" },
  { label: "SOS", value: "SOS_EMERGENCIA" },
  { label: "Timeout", value: "SESSION_TIMEOUT" },
  { label: "Localização", value: "LOCALIZACAO_PERIODICA" },
];

// ─────────────────────────────────────────────────────
//  PÁGINA PRINCIPAL
// ─────────────────────────────────────────────────────
export default function DashboardPage() {
  const [ocorrencias, setOcorrencias] = useState<OcorrenciaEnriquecida[]>([]);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [filtro, setFiltro] = useState<StatusEvento | "TODOS">("TODOS");
  const [loading, setLoading] = useState(true);
  const [lastSync, setLastSync] = useState<Date | null>(null);
  const [activeTab, setActiveTab] = useState<"lista" | "detalhe">("lista");

  // ── Fetch ──────────────────────────────────────────
  const fetchOcorrencias = useCallback(async () => {
    try {
      const url =
        filtro === "TODOS"
          ? "/api/ocorrencias?limit=100"
          : `/api/ocorrencias?status=${filtro}&limit=100`;
      const res = await fetch(url);
      if (!res.ok) return;
      const data: OcorrenciaEnriquecida[] = await res.json();
      setOcorrencias(data);
      setLastSync(new Date());
    } catch {
      // silently ignore
    } finally {
      setLoading(false);
    }
  }, [filtro]);

  useEffect(() => {
    fetchOcorrencias();
    const iv = setInterval(fetchOcorrencias, 3000);
    return () => clearInterval(iv);
  }, [fetchOcorrencias]);

  // ── Selecção ───────────────────────────────────────
  const selectedOcc = useMemo(
    () => (selectedId ? ocorrencias.find((o) => o.id === selectedId) : null),
    [ocorrencias, selectedId]
  );

  // ── Estatísticas ───────────────────────────────────
  const stats = useMemo(() => {
    const total = ocorrencias.length;
    const sos = ocorrencias.filter((o) => o.status === "SOS_EMERGENCIA").length;
    const disparos = ocorrencias.filter(
      (o) => o.status === "DISPARO_ATIVO"
    ).length;
    const tiros = ocorrencias.reduce((acc, o) => acc + (o.vezes || 0), 0);
    return { total, sos, disparos, tiros };
  }, [ocorrencias]);

  // ── Cores únicas por agente ────────────────────────
  const palette = ["#00f5d4", "#f72585", "#4cc9f0", "#fee440", "#9b5de5"];
  const agenteColors: Record<string, string> = {};
  let ci = 0;
  const getAgenteColor = (id: string) => {
    if (!agenteColors[id]) {
      agenteColors[id] = palette[ci % palette.length];
      ci++;
    }
    return agenteColors[id];
  };

  // ── Render ─────────────────────────────────────────
  return (
    <>
      <div
        style={{
          display: "flex",
          flexDirection: "column",
          height: "100vh",
          background: C.bg,
          color: C.text,
          fontFamily: C.font,
          overflow: "hidden",
        }}
      >
        {/* ══ TOP BAR ══════════════════════════════════ */}
        <header
          style={{
            display: "flex",
            alignItems: "center",
            gap: 14,
            padding: "0 20px",
            height: 52,
            background: "#060610",
            borderBottom: `1px solid ${C.border}`,
            flexShrink: 0,
            zIndex: 10,
          }}
        >
          {/* Logo */}
          <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
            <span style={{ fontSize: 20 }}>🔫</span>
            <div>
              <div
                style={{
                  fontSize: 13,
                  fontWeight: 800,
                  color: C.accent,
                  letterSpacing: "0.15em",
                  textTransform: "uppercase",
                }}
              >
                Arma Biométrica
              </div>
              <div style={{ fontSize: 9, color: C.textMuted, letterSpacing: "0.1em" }}>
                SISTEMA DE MONITORIZAÇÃO · v6.4
              </div>
            </div>
          </div>

          {/* Stats resumidas */}
          <div
            style={{
              marginLeft: 24,
              display: "flex",
              gap: 20,
              fontSize: 11,
              color: C.textSub,
            }}
          >
            <span>
              <span style={{ color: C.accent, fontWeight: 700 }}>
                {stats.total}
              </span>{" "}
              ocorrências
            </span>
            <span>
              <span style={{ color: "#f72585", fontWeight: 700 }}>
                {stats.disparos}
              </span>{" "}
              disparos
            </span>
            <span>
              <span style={{ color: "#ff2222", fontWeight: 700 }}>
                {stats.sos}
              </span>{" "}
              SOS
            </span>
            <span>
              <span style={{ color: "#fee440", fontWeight: 700 }}>
                {stats.tiros}
              </span>{" "}
              tiros
            </span>
          </div>

          {/* Direita */}
          <div
            style={{
              marginLeft: "auto",
              display: "flex",
              alignItems: "center",
              gap: 16,
            }}
          >
            {/* Live dot */}
            <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
              <span
                style={{
                  width: 8,
                  height: 8,
                  borderRadius: "50%",
                  background: loading ? "#555" : C.accent,
                  display: "inline-block",
                  boxShadow: loading ? "none" : `0 0 8px ${C.accent}`,
                  animation: loading ? "none" : "pulse 1.5s infinite",
                }}
              />
              <span style={{ fontSize: 10, color: C.textMuted }}>
                {loading ? "conectando…" : "LIVE"}
              </span>
            </div>
            {lastSync && (
              <span style={{ fontSize: 10, color: "#333" }}>
                sync{" "}
                {lastSync.toLocaleTimeString("pt", {
                  hour: "2-digit",
                  minute: "2-digit",
                  second: "2-digit",
                })}
              </span>
            )}
          </div>
        </header>

        {/* ══ STAT CARDS ═══════════════════════════════ */}
        <div
          style={{
            display: "flex",
            gap: 10,
            padding: "10px 16px",
            background: "#09091a",
            borderBottom: `1px solid ${C.border}`,
            flexShrink: 0,
          }}
        >
          <StatCard
            label="Total"
            value={stats.total}
            color={C.accent}
            emoji="📊"
          />
          <StatCard
            label="Disparos"
            value={stats.disparos}
            color="#f72585"
            emoji="🔴"
          />
          <StatCard
            label="Tiros"
            value={stats.tiros}
            color="#ff9a1f"
            emoji="💥"
          />
          <StatCard
            label="SOS"
            value={stats.sos}
            color="#ff2222"
            emoji="🆘"
          />
          <StatCard
            label="Endpoint"
            value="POST /api/ocorrencias"
            color="#4cc9f0"
            emoji="📡"
          />
        </div>

        {/* ══ CORPO PRINCIPAL ══════════════════════════ */}
        <div style={{ display: "flex", flex: 1, overflow: "hidden" }}>
          {/* ─── SIDEBAR ─────────────────────────────── */}
          <aside
            style={{
              width: 300,
              background: C.bgSidebar,
              borderRight: `1px solid ${C.border}`,
              display: "flex",
              flexDirection: "column",
              overflow: "hidden",
              flexShrink: 0,
            }}
          >
            {/* Filtros */}
            <div
              style={{
                padding: "8px 10px",
                borderBottom: `1px solid ${C.border}`,
                display: "flex",
                flexWrap: "wrap",
                gap: 4,
              }}
            >
              {FILTROS.map((f) => {
                const active = filtro === f.value;
                const cor =
                  f.value === "TODOS"
                    ? C.accent
                    : STATUS_COLOR[f.value as StatusEvento];
                return (
                  <button
                    key={f.value}
                    onClick={() => {
                      setFiltro(f.value as StatusEvento | "TODOS");
                      setSelectedId(null);
                    }}
                    style={{
                      fontSize: 9,
                      padding: "3px 8px",
                      borderRadius: 6,
                      border: `1px solid ${active ? cor : C.border}`,
                      background: active ? `${cor}22` : "transparent",
                      color: active ? cor : C.textMuted,
                      cursor: "pointer",
                      fontFamily: C.font,
                      fontWeight: active ? 700 : 400,
                      transition: "all .15s",
                    }}
                  >
                    {f.label}
                  </button>
                );
              })}
            </div>

            {/* Tabs */}
            <div
              style={{
                display: "flex",
                borderBottom: `1px solid ${C.border}`,
              }}
            >
              {(["lista", "detalhe"] as const).map((tab) => (
                <button
                  key={tab}
                  onClick={() => setActiveTab(tab)}
                  style={{
                    flex: 1,
                    padding: "8px 0",
                    fontSize: 10,
                    letterSpacing: "0.12em",
                    textTransform: "uppercase",
                    background: activeTab === tab ? "#131326" : "transparent",
                    color: activeTab === tab ? C.accent : C.textMuted,
                    border: "none",
                    borderBottom:
                      activeTab === tab
                        ? `2px solid ${C.accent}`
                        : "2px solid transparent",
                    cursor: "pointer",
                    fontFamily: C.font,
                  }}
                >
                  {tab === "lista" ? "📋 Lista" : "🔍 Detalhe"}
                </button>
              ))}
            </div>

            {/* Conteúdo da tab */}
            <div style={{ flex: 1, overflowY: "auto" }}>
              {activeTab === "lista" && (
                <>
                  {ocorrencias.length === 0 && !loading && (
                    <div
                      style={{
                        padding: 24,
                        color: C.textMuted,
                        fontSize: 12,
                        textAlign: "center",
                        lineHeight: 2,
                      }}
                    >
                      Sem ocorrências.
                      <br />
                      <span style={{ fontSize: 10, color: "#333" }}>
                        A aguardar dados do ESP32…
                        <br />
                        POST → /api/ocorrencias
                      </span>
                    </div>
                  )}

                  {ocorrencias.map((o) => {
                    const cor = o.stale
                      ? "#444"
                      : STATUS_COLOR[o.status];
                    const isSelected = selectedId === o.id;
                    return (
                      <div
                        key={o.id}
                        onClick={() =>
                          setSelectedId(isSelected ? null : o.id)
                        }
                        style={{
                          padding: "10px 14px",
                          borderBottom: `1px solid #0e0e1e`,
                          cursor: "pointer",
                          background: isSelected ? "#131326" : "transparent",
                          borderLeft: `3px solid ${isSelected ? cor : "transparent"}`,
                          transition: "all .15s",
                        }}
                      >
                        {/* Linha 1 */}
                        <div
                          style={{
                            display: "flex",
                            justifyContent: "space-between",
                            alignItems: "center",
                            marginBottom: 4,
                          }}
                        >
                          <span
                            style={{
                              fontSize: 12,
                              fontWeight: 700,
                              color: cor,
                            }}
                          >
                            {o.id}
                          </span>
                          <Badge status={o.status} />
                        </div>
                        {/* Linha 2 */}
                        <div
                          style={{
                            fontSize: 11,
                            color: C.textSub,
                            marginBottom: 3,
                          }}
                        >
                          {descAgente(o.idAgente)}
                        </div>
                        {/* Linha 3 */}
                        <div
                          style={{
                            display: "flex",
                            gap: 10,
                            fontSize: 10,
                            color: C.textMuted,
                          }}
                        >
                          <span>📅 {o.data} {o.hora}</span>
                          <span>💥 {o.vezes}</span>
                          <span>🕐 {timeAgo(o.ago)}</span>
                        </div>
                      </div>
                    );
                  })}
                </>
              )}

              {activeTab === "detalhe" && selectedOcc && (
                <div style={{ padding: "14px 16px" }}>
                  <div
                    style={{
                      fontSize: 10,
                      letterSpacing: "0.15em",
                      color: C.textMuted,
                      textTransform: "uppercase",
                      marginBottom: 12,
                    }}
                  >
                    Ocorrência Seleccionada
                  </div>
                  {[
                    ["ID", selectedOcc.id],
                    ["Status", STATUS_LABEL[selectedOcc.status]],
                    ["Agente", `${selectedOcc.idAgente} — ${descAgente(selectedOcc.idAgente)}`],
                    ["Data", selectedOcc.data],
                    ["Hora", selectedOcc.hora],
                    ["Disparos (vezes)", String(selectedOcc.vezes)],
                    ["Latitude", selectedOcc.latitude.toFixed(7)],
                    ["Longitude", selectedOcc.longitude.toFixed(7)],
                    ["Recebido", `${selectedOcc.ago}s atrás`],
                    ["GPS", selectedOcc.stale ? "Desactualizado" : "Recente"],
                    [
                      "Mapa",
                      `${selectedOcc.latitude.toFixed(4)}, ${selectedOcc.longitude.toFixed(4)}`,
                    ],
                  ].map(([label, value]) => (
                    <div
                      key={label}
                      style={{
                        display: "flex",
                        justifyContent: "space-between",
                        fontSize: 11,
                        padding: "5px 0",
                        borderBottom: `1px solid #0e0e1e`,
                        gap: 8,
                      }}
                    >
                      <span style={{ color: C.textMuted, flexShrink: 0 }}>
                        {label}
                      </span>
                      <span
                        style={{
                          color:
                            label === "Status"
                              ? STATUS_COLOR[selectedOcc.status]
                              : label === "GPS"
                              ? selectedOcc.stale
                                ? "#f44"
                                : "#4f4"
                              : "#ccc",
                          fontWeight: 500,
                          textAlign: "right",
                          wordBreak: "break-all",
                        }}
                      >
                        {value}
                      </span>
                    </div>
                  ))}

                  {/* Botão Google Maps */}
                  <a
                    href={`https://maps.google.com/?q=${selectedOcc.latitude},${selectedOcc.longitude}`}
                    target="_blank"
                    rel="noreferrer"
                    style={{
                      display: "block",
                      marginTop: 14,
                      padding: "8px 12px",
                      background: `${C.accent}18`,
                      border: `1px solid ${C.accent}44`,
                      borderRadius: 8,
                      color: C.accent,
                      fontSize: 11,
                      textAlign: "center",
                      textDecoration: "none",
                      fontWeight: 700,
                    }}
                  >
                    🗺️ Abrir no Google Maps
                  </a>
                </div>
              )}

              {activeTab === "detalhe" && !selectedOcc && (
                <div
                  style={{
                    padding: 24,
                    color: C.textMuted,
                    fontSize: 12,
                    textAlign: "center",
                  }}
                >
                  Seleccione uma ocorrência<br />na lista ou no mapa.
                </div>
              )}
            </div>
          </aside>

          {/* ─── MAPA ────────────────────────────────── */}
          <main style={{ flex: 1, position: "relative" }}>
            <ArmaMap
              ocorrencias={ocorrencias}
              selectedId={selectedId}
              onSelectId={(id) => {
                setSelectedId(id);
                setActiveTab("detalhe");
              }}
            />

            {/* Overlay quando vazio */}
            {ocorrencias.length === 0 && !loading && (
              <div
                style={{
                  position: "absolute",
                  inset: 0,
                  display: "flex",
                  alignItems: "center",
                  justifyContent: "center",
                  pointerEvents: "none",
                  zIndex: 1000,
                }}
              >
                <div
                  style={{
                    background: "#0d0d1aee",
                    border: `1px solid ${C.border}`,
                    borderRadius: 14,
                    padding: "24px 36px",
                    textAlign: "center",
                  }}
                >
                  <div style={{ fontSize: 36, marginBottom: 10 }}>📡</div>
                  <div style={{ fontSize: 14, color: "#aaa", marginBottom: 6 }}>
                    A aguardar dados do ESP32
                  </div>
                  <div style={{ fontSize: 11, color: C.textMuted }}>
                    POST →{" "}
                    <code style={{ color: C.accent }}>/api/ocorrencias</code>
                  </div>
                </div>
              </div>
            )}
          </main>
        </div>
      </div>

      <style>{`
        * { box-sizing: border-box; }
        ::-webkit-scrollbar { width: 4px; }
        ::-webkit-scrollbar-track { background: #080810; }
        ::-webkit-scrollbar-thumb { background: #1e1e3a; border-radius: 4px; }
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.2; }
        }
        .leaflet-popup-content-wrapper {
          background: #1a1a2e !important;
          border: 1px solid #2a2a4a !important;
          color: #eee !important;
          box-shadow: 0 4px 24px #00000099 !important;
        }
        .leaflet-popup-tip { background: #1a1a2e !important; }
        .leaflet-container { background: #080810 !important; }
        .leaflet-popup-content { margin: 0 !important; }
      `}</style>
    </>
  );
}
