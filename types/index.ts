// ============================================================
//  ARMA BIOMETRICA — Types
// ============================================================

export type StatusEvento =
  | "DISPARO_ATIVO"
  | "ACESSO_NEGADO"
  | "POSSE_PERDIDA"
  | "SOS_EMERGENCIA"
  | "ENVIO_MANUAL"
  | "SESSION_TIMEOUT"
  | "LOCALIZACAO_PERIODICA"
  | "DESCONHECIDO";

export interface Ocorrencia {
  id: string;           // OCC-00001
  idAgente: string;     // "1" | "2" | etc.
  latitude: number;
  longitude: number;
  data: string;         // "2025-07-10"
  hora: string;         // "14:33:07"
  vezes: number;        // número de disparos
  status: StatusEvento;
  receivedAt: number;   // timestamp server-side (ms)
}

export interface OcorrenciaStore {
  [id: string]: Ocorrencia;
}

// Enriquecida com campos calculados para o frontend
export interface OcorrenciaEnriquecida extends Ocorrencia {
  ago: number;          // segundos desde recebimento
  stale: boolean;       // sem update > 5 min
}
