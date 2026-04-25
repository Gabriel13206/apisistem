"use client";

import { useEffect, useRef } from "react";
import type { Map as LeafletMap, Marker, Circle } from "leaflet";
import { OcorrenciaEnriquecida, StatusEvento } from "@/types";

interface ArmaMapProps {
  ocorrencias: OcorrenciaEnriquecida[];
  selectedId: string | null;
  onSelectId: (id: string | null) => void;
}

// Cores por tipo de evento
const STATUS_COLOR: Record<StatusEvento, string> = {
  DISPARO_ATIVO: "#f72585",
  ACESSO_NEGADO: "#ff6b35",
  POSSE_PERDIDA: "#fee440",
  SOS_EMERGENCIA: "#ff0000",
  ENVIO_MANUAL: "#4cc9f0",
  SESSION_TIMEOUT: "#9b5de5",
  LOCALIZACAO_PERIODICA: "#00f5d4",
  DESCONHECIDO: "#888888",
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

function waitForLeaflet(cb: () => void, tries = 0) {
  if (typeof window !== "undefined" && (window as any).L) {
    cb();
  } else if (tries < 60) {
    setTimeout(() => waitForLeaflet(cb, tries + 1), 100);
  }
}

interface MarkerEntry {
  marker: Marker;
  circle: Circle;
}

export default function ArmaMap({
  ocorrencias,
  selectedId,
  onSelectId,
}: ArmaMapProps) {
  const mapRef = useRef<LeafletMap | null>(null);
  const markersRef = useRef<Record<string, MarkerEntry>>({});
  const containerRef = useRef<HTMLDivElement>(null);

  // Inicializar mapa uma vez
  useEffect(() => {
    waitForLeaflet(() => {
      if (mapRef.current || !containerRef.current) return;
      const L = (window as any).L;

      const map: LeafletMap = L.map(containerRef.current).setView(
        [-8.8188885, 13.2670763],
        13
      );

      L.tileLayer(
        "https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png",
        { attribution: "© OpenStreetMap © CARTO", maxZoom: 19 }
      ).addTo(map);

      mapRef.current = map;
    });

    return () => {
      if (mapRef.current) {
        mapRef.current.remove();
        mapRef.current = null;
        markersRef.current = {};
      }
    };
  }, []);

  // Actualizar marcadores
  useEffect(() => {
    waitForLeaflet(() => {
      const L = (window as any).L;
      if (!mapRef.current) return;
      const map = mapRef.current;

      // Remover marcadores de IDs que já não existem
      const idsActuais = new Set(ocorrencias.map((o) => o.id));
      Object.keys(markersRef.current).forEach((id) => {
        if (!idsActuais.has(id)) {
          markersRef.current[id].marker.remove();
          markersRef.current[id].circle.remove();
          delete markersRef.current[id];
        }
      });

      ocorrencias.forEach((o) => {
        const pos: [number, number] = [o.latitude, o.longitude];
        const color = o.stale ? "#555" : STATUS_COLOR[o.status];
        const emoji = STATUS_EMOJI[o.status];
        const isSelected = o.id === selectedId;

        const icon = L.divIcon({
          className: "",
          html: `
            <div style="display:flex;flex-direction:column;align-items:center;gap:1px;cursor:pointer">
              <div style="
                background:${color};color:#000;font-size:9px;font-weight:800;
                font-family:monospace;padding:2px 5px;border-radius:4px;
                white-space:nowrap;box-shadow:0 0 8px ${color}99;
                border:${isSelected ? "2px solid #fff" : "none"};
              ">${o.id}</div>
              <div style="width:2px;height:3px;background:${color}"></div>
              <div style="
                width:${isSelected ? 42 : 34}px;height:${isSelected ? 42 : 34}px;
                border-radius:50%;background:${color}22;
                border:${isSelected ? "3px" : "2px"} solid ${color};
                display:flex;align-items:center;justify-content:center;
                box-shadow:0 0 ${isSelected ? 18 : 10}px ${color}99;
                font-size:${isSelected ? 20 : 16}px;transition:all .2s;
              ">${emoji}</div>
            </div>`,
          iconSize: [60, 60],
          iconAnchor: [30, 60],
        });

        const popupHtml = `
          <div style="font-family:monospace;color:#eee;padding:4px;min-width:180px">
            <b style="color:${color};font-size:13px">${o.id}</b><br/>
            <div style="color:#aaa;font-size:11px;line-height:1.7;margin-top:4px">
              <b style="color:#ccc">Agente:</b> ${o.idAgente}<br/>
              <b style="color:#ccc">Status:</b> <span style="color:${color}">${o.status.replace(/_/g, " ")}</span><br/>
              <b style="color:#ccc">Data:</b> ${o.data} ${o.hora}<br/>
              <b style="color:#ccc">Disparos:</b> ${o.vezes}<br/>
              <b style="color:#ccc">GPS:</b> ${o.latitude.toFixed(6)}, ${o.longitude.toFixed(6)}<br/>
              ${o.stale
                ? `<span style="color:#f44">Sem actualização</span>`
                : `<span style="color:#4f4">Recente (${o.ago}s atrás)</span>`
              }
            </div>
          </div>`;

        if (markersRef.current[o.id]) {
          const { marker, circle } = markersRef.current[o.id];
          marker.setLatLng(pos).setIcon(icon).bindPopup(popupHtml);
          circle.setLatLng(pos).setStyle({ color, fillColor: color });
        } else {
          const marker = L.marker(pos, { icon })
            .addTo(map)
            .bindPopup(popupHtml);
          marker.on("click", () => onSelectId(o.id));
          const circle = L.circle(pos, {
            radius: 15,
            color,
            fillColor: color,
            fillOpacity: 0.12,
            weight: 1,
          }).addTo(map);
          markersRef.current[o.id] = { marker, circle };
        }
      });

      if (ocorrencias.length > 0) {
        if (selectedId) {
          const sel = ocorrencias.find((o) => o.id === selectedId);
          if (sel) map.setView([sel.latitude, sel.longitude], 16);
        } else if (ocorrencias.length === 1) {
          map.setView([ocorrencias[0].latitude, ocorrencias[0].longitude], 16);
        } else {
          const latlngs = ocorrencias.map(
            (o) => [o.latitude, o.longitude] as [number, number]
          );
          map.fitBounds(latlngs, { padding: [60, 60] });
        }
      }
    });
  }, [ocorrencias, selectedId, onSelectId]);

  return <div ref={containerRef} style={{ width: "100%", height: "100%" }} />;
}
