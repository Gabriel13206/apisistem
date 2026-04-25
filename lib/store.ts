import { OcorrenciaStore } from "@/types";

// ============================================================
//  Store global em memória — persiste entre requests no mesmo processo
//  Para produção com múltiplas instâncias: substituir por Redis / Postgres
// ============================================================
declare global {
  // eslint-disable-next-line no-var
  var armaStore: OcorrenciaStore | undefined;
}

if (!global.armaStore) {
  global.armaStore = {};
}

export const store: OcorrenciaStore = global.armaStore;

// Máximo de ocorrências a manter em memória
export const MAX_OCORRENCIAS = 500;
