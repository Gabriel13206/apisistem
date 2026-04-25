/*
 * ============================================================
 *  ARMA BIOMETRICA INTELIGENTE  v6.5
 *  ESP32 Dual-Core | AS608 | A7670E GSM/GPS
 * ============================================================
 *  NOVIDADES v6.5 (relativamente à v6.4):
 *  [1] Endpoint actualizado → /api/ocorrencias (Next.js)
 *  [2] JSON alinhado com a API: id, idAgente, latitude,
 *      longitude, data, hora, vezes, status
 *  [3] Suporte a HTTPS nativo (AT+HTTPSSL=1 mantido)
 *  [4] Campo "status" corresponde a nomeEvento()
 *  [5] Campo "idAgente" como string numérica (compatível API)
 *
 *  PINAGEM:
 *   AS608  RX/TX        : GPIO 16 / 17  (HardwareSerial 2)
 *   A7670E RX/TX/PWRKEY : GPIO 26 / 27 / 5  (HardwareSerial 1)
 *   Gatilho             : GPIO 32  (INPUT_PULLUP)
 *   SOS                 : GPIO 33  (INPUT_PULLUP)
 *   Laser               : GPIO 25  (via 2N2222 + 1kΩ)
 *   Buzzer              : GPIO 23
 *   LED Amarelo (AM)    : GPIO 18
 *   LED Azul    (AZ)    : GPIO 19
 *   Touch Posse (T4)    : GPIO 13
 *
 *  DEPENDENCIAS (Library Manager):
 *   - Adafruit Fingerprint Sensor Library
 *   - ArduinoJson  (Benoit Blanchon) v6.x
 * ============================================================
 */

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// ============================================================
//  PINOS
// ============================================================
#define PIN_AS608_RX      16
#define PIN_AS608_TX      17
#define PIN_GSM_RX        26
#define PIN_GSM_TX        27
#define PIN_GSM_PWRKEY     5
#define PIN_GATILHO       32
#define PIN_SOS           33
#define PIN_LASER         25
#define PIN_BUZZER        23
#define PIN_LED_AM        18
#define PIN_LED_AZ        19
#define PIN_TOUCH         13

// ============================================================
//  TOUCH
// ============================================================
#define TOUCH_N_AMOSTRAS   16
#define TOUCH_LIMIAR_ON   150
#define TOUCH_LIMIAR_OFF  400
#define TOUCH_MS_CONFIRMA 200
#define TOUCH_MS_PERDA    300

// ============================================================
//  TIMERS
// ============================================================
#define SESSION_TIMEOUT_MS  (15UL * 60UL * 1000UL)
#define PERIODIC_MS         ( 2UL * 60UL * 60UL * 1000UL)
#define SOS_CURTO_MAX_MS   1500
#define SOS_LONGO_MIN_MS   3000

// ============================================================
//  SERVIDOR — Substitua pela URL real da Vercel ou ngrok
//  Exemplo Vercel:  https://arma-biometrica.vercel.app/api/ocorrencias
//  Exemplo ngrok:   https://xxxx.ngrok-free.app/api/ocorrencias
// ============================================================
#define SERVIDOR_URL    "https://arma-biometrica.vercel.app/api/ocorrencias"
#define TEL_SUPERVISOR  "+244937156022"
#define APN             "internet.unitel.co.ao"

// GPS fallback Luanda
#define GPS_FALLBACK_LAT  -8.8188885f
#define GPS_FALLBACK_LNG  13.2670763f
#define GPS_FALLBACK_LINK "https://maps.google.com/?q=-8.8188885,13.2670763"

// IDs proprietário
#define ID_PROP_1   1
#define ID_PROP_2   2

// NVS / filas
#define PREF_NS         "arma"
#define FILA_ALTA_SZ     8
#define FILA_NORMAL_SZ  24
#define HTTP_RETRY_MAX   3

// ============================================================
//  PEDIDOS DE LED AZUL
// ============================================================
enum LedAzPedido : uint8_t {
  LAZ_NADA = 0,
  LAZ_SOS,
  LAZ_ENVIO_MANUAL
};
volatile LedAzPedido ledAzReq = LAZ_NADA;

// ============================================================
//  TIPOS
// ============================================================
enum TipoEvento : uint8_t {
  EVT_DISPARO = 0,
  EVT_ACESSO_NEGADO,
  EVT_POSSE_PERDIDA,
  EVT_SOS_EMERGENCIA,
  EVT_ENVIO_MANUAL,
  EVT_SESSION_TIMEOUT,
  EVT_PERIODICO
};

struct Evento {
  TipoEvento tipo;
  int        idAgente;
  char       descAgente[96];
  int        vezes;
  float      latitude;
  float      longitude;
  bool       gpsReal;
  char       data[12];
  char       hora[10];
  bool       altaPrior;
};

// ============================================================
//  GLOBAIS
// ============================================================
SemaphoreHandle_t mtx;
QueueHandle_t     qAlta;
QueueHandle_t     qNormal;
Preferences       prefs;

volatile bool  posseOK     = false;
volatile bool  armaOK      = false;
volatile bool  sessaoAtiva = false;
volatile int   sessaoId    = 0;
volatile int   totalTiros  = 0;
char           sessaoDesc[96] = "";

volatile float gLat = GPS_FALLBACK_LAT;
volatile float gLng = GPS_FALLBACK_LNG;
volatile bool  gFix = false;
char gData[12] = "0000-00-00";
char gHora[10] = "00:00:00";

volatile bool diagDone = false;
uint32_t occID = 1;

HardwareSerial       gsmSer(1);
HardwareSerial       fpSer(2);
Adafruit_Fingerprint finger(&fpSer);

// ============================================================
//  PROTOTIPOS
// ============================================================
void taskCore0(void *);
void taskCore1(void *);
uint32_t touchMediana();
String atSend(const char *cmd, const char *wait, uint32_t timeout);
bool   atOK(const char *cmd, uint32_t timeout = 2000);
void   gsmFlush();
void   gsmPowerOn();
void   syncRelogio();
void   lerGPS();
bool   ativarGPRS();
bool   httpPost(const Evento &e);
bool   smsSend(const Evento &e);
void   enfileirar(const Evento &e);
void   processarFilas();
void   salvarOffline(const Evento &e);
void   tentarOffline();
String buildJSON(const Evento &e);
String buildSMS(const Evento &e);
String linkMapa(float lat, float lng, bool real);
String descID(int id);
String nomeEvento(TipoEvento t);
void ledAmSolido(bool on);
void ledAmBlink(int n, int ms);
void ledAzSolido(bool on);
void ledAzBlink(int n, int ms);
void buzzer(int ms);
void buzzerPattern(int n);

// ============================================================
//  TOUCH — MEDIANA 16
// ============================================================
uint32_t touchMediana() {
  uint32_t buf[TOUCH_N_AMOSTRAS];
  for (int i = 0; i < TOUCH_N_AMOSTRAS; i++) {
    buf[i] = touchRead(PIN_TOUCH);
    delayMicroseconds(200);
  }
  for (int i = 1; i < TOUCH_N_AMOSTRAS; i++) {
    uint32_t key = buf[i]; int j = i - 1;
    while (j >= 0 && buf[j] > key) { buf[j+1] = buf[j]; j--; }
    buf[j+1] = key;
  }
  return buf[TOUCH_N_AMOSTRAS / 2];
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_LASER,      OUTPUT); digitalWrite(PIN_LASER,      LOW);
  pinMode(PIN_LED_AM,     OUTPUT); digitalWrite(PIN_LED_AM,     LOW);
  pinMode(PIN_LED_AZ,     OUTPUT); digitalWrite(PIN_LED_AZ,     LOW);
  pinMode(PIN_BUZZER,     OUTPUT); digitalWrite(PIN_BUZZER,     LOW);
  pinMode(PIN_GATILHO,    INPUT_PULLUP);
  pinMode(PIN_SOS,        INPUT_PULLUP);
  pinMode(PIN_GSM_PWRKEY, OUTPUT); digitalWrite(PIN_GSM_PWRKEY, HIGH);

  prefs.begin(PREF_NS, false);
  occID = prefs.getUInt("occID", 1);

  mtx     = xSemaphoreCreateMutex();
  qAlta   = xQueueCreate(FILA_ALTA_SZ,   sizeof(Evento));
  qNormal = xQueueCreate(FILA_NORMAL_SZ, sizeof(Evento));

  fpSer.begin(57600, SERIAL_8N1, PIN_AS608_RX, PIN_AS608_TX);
  delay(600);
  finger.begin(57600);
  bool fpOK = false;
  for (int i = 0; i < 6 && !fpOK; i++) {
    if (finger.verifyPassword()) fpOK = true;
    else delay(400);
  }

  gsmSer.begin(115200, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);

  Serial.println(F("============================================================"));
  Serial.println(F("  ARMA BIOMETRICA INTELIGENTE  v6.5"));
  Serial.println(F("  API: Next.js /api/ocorrencias"));
  Serial.println(F("============================================================"));
  Serial.printf( "  [AS608] %s\n", fpOK ? "OK" : "FALHA");

  xTaskCreatePinnedToCore(taskCore0, "C0", 14336, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskCore1, "C1",  8192, NULL, 2, NULL, 1);
}

void loop() { vTaskDelay(portMAX_DELAY); }

// ============================================================
//  CORE 0 — GSM / GPS / HTTP / SMS / Filas
// ============================================================
void taskCore0(void *pv) {
  gsmPowerOn();

  Serial.println(F("  DIAGNOSTICO A7670E — Aguarde..."));

  bool gsmOK=false, redeOK=false, gprsOK=false;
  bool gpsOK=false, httpOK=false, smsOK=false;

  for (int i = 0; i < 10 && !gsmOK; i++) {
    ledAzBlink(1, 300);
    if (atOK("AT", 1000)) gsmOK = true;
    else vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (gsmOK) {
    atOK("ATE0", 500);
    atOK("AT+CMEE=2", 500);
    atOK("AT+CLTS=1", 1000);
    atOK("AT&W", 1000);

    for (int i = 0; i < 40 && !redeOK; i++) {
      String r = atSend("AT+CREG?", "+CREG", 2000);
      if (r.indexOf(",1") >= 0 || r.indexOf(",5") >= 0) redeOK = true;
      else vTaskDelay(pdMS_TO_TICKS(1000));
    }

    gprsOK = ativarGPRS();
    atOK("AT+CGPS=0", 1000);
    vTaskDelay(pdMS_TO_TICKS(300));
    gpsOK = atOK("AT+CGPS=1,1", 3000);
    syncRelogio();

    if (gprsOK) {
      atOK("AT+HTTPTERM", 1000);
      if (atOK("AT+HTTPINIT", 3000)) {
        atOK("AT+HTTPSSL=1", 1000);
        atOK("AT+HTTPPARA=\"CID\",1", 1000);
        String u = String("AT+HTTPPARA=\"URL\",\"") + SERVIDOR_URL + "\"";
        atOK(u.c_str(), 1000);
        String r = atSend("AT+HTTPACTION=0", "+HTTPACTION", 15000);
        httpOK = (r.indexOf(",200") >= 0 || r.indexOf(",201") >= 0);
        atOK("AT+HTTPTERM", 1000);
      }
    }
    smsOK = atOK("AT+CMGF=1", 2000);
  }

  Serial.printf("  GSM  | %s\n", gsmOK  ? "OK" : "FALHA");
  Serial.printf("  REDE | %s\n", redeOK ? "OK" : "SEM REDE");
  Serial.printf("  GPRS | %s\n", gprsOK ? "OK" : "FALHA");
  Serial.printf("  GPS  | %s\n", gpsOK  ? "OK" : "FALHA");
  Serial.printf("  HTTP | %s\n", httpOK ? "OK" : "FALHA");
  Serial.printf("  SMS  | %s\n", smsOK  ? "OK" : "FALHA");
  Serial.println(F("============================================================"));

  ledAzBlink(3, 100);
  diagDone = true;

  uint32_t tGPS      = 0;
  uint32_t tPeriodic = millis();

  for (;;) {
    uint32_t agora = millis();

    if (agora - tGPS > 30000) {
      lerGPS();
      syncRelogio();
      tGPS = agora;
    }

    LedAzPedido req = ledAzReq;
    if (req != LAZ_NADA) {
      ledAzReq = LAZ_NADA;
      if (req == LAZ_SOS)          ledAzBlink(10, 60);
      if (req == LAZ_ENVIO_MANUAL) ledAzBlink(3, 150);
    }

    tentarOffline();
    processarFilas();

    if (agora - tPeriodic > PERIODIC_MS) {
      bool sa; int sid; char sd[96];
      if (xSemaphoreTake(mtx, 5) == pdTRUE) {
        sa = sessaoAtiva; sid = sessaoId;
        memcpy(sd, sessaoDesc, 96);
        xSemaphoreGive(mtx);
      }
      Evento e{}; e.tipo = EVT_PERIODICO;
      e.idAgente = sa ? sid : 0;
      memcpy(e.descAgente, sa ? sd : "sistema", 96);
      e.latitude = gLat; e.longitude = gLng; e.gpsReal = gFix;
      memcpy(e.data, gData, 12); memcpy(e.hora, gHora, 10);
      enfileirar(e);
      tPeriodic = agora;
    }

    vTaskDelay(pdMS_TO_TICKS(80));
  }
}

// ============================================================
//  CORE 1 — Touch / Biometria / Gatilho / SOS
// ============================================================
void taskCore1(void *pv) {
  while (!diagDone) vTaskDelay(pdMS_TO_TICKS(100));
  ledAmBlink(1, 400);
  Serial.println(F("  Segure a empunhadura e valide a digital."));
  Serial.println(F("============================================================"));

  bool     posseRaw   = false;
  bool     posseConf  = false;
  uint32_t tMudanca   = millis();
  uint32_t tUltAtiv   = 0;
  bool     gatPres    = false;
  bool     sosPres    = false;
  uint32_t tSosPress  = 0;
  bool     sosLongEnv = false;

  for (;;) {
    uint32_t agora = millis();

    // 1. TOUCH
    uint32_t med = touchMediana();
    bool novoRaw;
    if      (!posseRaw && med < TOUCH_LIMIAR_ON)  novoRaw = true;
    else if  (posseRaw && med > TOUCH_LIMIAR_OFF) novoRaw = false;
    else                                           novoRaw = posseRaw;
    if (novoRaw != posseRaw) { posseRaw = novoRaw; tMudanca = agora; }

    bool posseAnterior = posseConf;
    if ( posseRaw && !posseConf && (agora - tMudanca >= TOUCH_MS_CONFIRMA)) posseConf = true;
    if (!posseRaw &&  posseConf && (agora - tMudanca >= TOUCH_MS_PERDA))   posseConf = false;
    if (xSemaphoreTake(mtx, 5) == pdTRUE) { posseOK = posseConf; xSemaphoreGive(mtx); }

    if (posseAnterior && !posseConf) {
      digitalWrite(PIN_LASER, LOW);
      ledAmSolido(false);
      bool era; int sid; char sd[96]; int st;
      if (xSemaphoreTake(mtx, 10) == pdTRUE) {
        era = sessaoAtiva; sid = sessaoId;
        memcpy(sd, sessaoDesc, 96); st = totalTiros;
        armaOK = false; sessaoAtiva = false;
        xSemaphoreGive(mtx);
      }
      Serial.println(F("  [T4] POSSE PERDIDA"));
      ledAmBlink(5, 80);
      if (era) {
        Evento e{}; e.tipo = EVT_POSSE_PERDIDA; e.altaPrior = true;
        e.idAgente = sid; memcpy(e.descAgente, sd, 96); e.vezes = st;
        e.latitude = gLat; e.longitude = gLng; e.gpsReal = gFix;
        memcpy(e.data, gData, 12); memcpy(e.hora, gHora, 10);
        enfileirar(e);
        buzzerPattern(3);
        tUltAtiv = 0;
      }
      Serial.println(F("  Segure a empunhadura e valide a digital."));
    }

    // 2. BIOMETRIA
    if (posseConf) {
      bool jaSessao;
      if (xSemaphoreTake(mtx, 5) == pdTRUE) { jaSessao = sessaoAtiva; xSemaphoreGive(mtx); }

      if (!jaSessao) {
        uint8_t p = FINGERPRINT_NOFINGER;
        bool imagemOK = false;
        for (int t = 0; t < 3 && !imagemOK; t++) {
          p = finger.getImage();
          if      (p == FINGERPRINT_OK)       imagemOK = true;
          else if (p == FINGERPRINT_NOFINGER) break;
          if (!imagemOK && t < 2) vTaskDelay(pdMS_TO_TICKS(60));
        }
        if (imagemOK) {
          p = finger.image2Tz();
          if (p == FINGERPRINT_OK) {
            p = finger.fingerFastSearch();
            int fid = -1;
            if (p == FINGERPRINT_OK && finger.confidence >= 30)
              fid = finger.fingerID;
            String dc = descID(fid);

            if (fid == ID_PROP_1 || fid == ID_PROP_2) {
              if (xSemaphoreTake(mtx, 10) == pdTRUE) {
                armaOK = true; sessaoAtiva = true;
                sessaoId = fid; totalTiros = 0;
                memcpy(sessaoDesc, dc.c_str(), min((size_t)96, dc.length()+1));
                xSemaphoreGive(mtx);
              }
              tUltAtiv = agora;
              Serial.printf("  [AUTH] AUTORIZADO: %s\n", dc.c_str());
              ledAmBlink(2, 400);
              buzzerPattern(1);
            } else if (fid > 0) {
              Serial.printf("  [AUTH] NEGADO: %s\n", dc.c_str());
              ledAmBlink(4, 100);
              buzzerPattern(4);
              Evento e{}; e.tipo = EVT_ACESSO_NEGADO; e.altaPrior = true;
              e.idAgente = fid;
              memcpy(e.descAgente, dc.c_str(), min((size_t)96, dc.length()+1));
              e.vezes = 0;
              e.latitude = gLat; e.longitude = gLng; e.gpsReal = gFix;
              memcpy(e.data, gData, 12); memcpy(e.hora, gHora, 10);
              enfileirar(e);
            }
          }
        }
      }
    }

    // 3. GATILHO
    {
      bool autorizado;
      if (xSemaphoreTake(mtx, 5) == pdTRUE) { autorizado = armaOK && sessaoAtiva; xSemaphoreGive(mtx); }
      bool gatLido = (digitalRead(PIN_GATILHO) == LOW);

      if (posseConf && autorizado) {
        if (gatLido && !gatPres) {
          gatPres = true;
          digitalWrite(PIN_LASER, HIGH);
          ledAmSolido(true);
          buzzer(20);
          if (xSemaphoreTake(mtx, 5) == pdTRUE) { totalTiros++; xSemaphoreGive(mtx); }
          tUltAtiv = agora;

          // Reportar disparo imediatamente
          int sid; char sd[96]; int st;
          if (xSemaphoreTake(mtx, 5) == pdTRUE) {
            sid = sessaoId; memcpy(sd, sessaoDesc, 96); st = totalTiros;
            xSemaphoreGive(mtx);
          }
          Evento e{}; e.tipo = EVT_DISPARO; e.altaPrior = true;
          e.idAgente = sid; memcpy(e.descAgente, sd, 96); e.vezes = st;
          e.latitude = gLat; e.longitude = gLng; e.gpsReal = gFix;
          memcpy(e.data, gData, 12); memcpy(e.hora, gHora, 10);
          enfileirar(e);

          Serial.printf("  [TIRO] Disparo #%d\n", st);
        }
        if (!gatLido && gatPres) {
          gatPres = false;
          digitalWrite(PIN_LASER, LOW);
          ledAmSolido(false);
        }
      } else {
        if (gatPres) {
          gatPres = false;
          digitalWrite(PIN_LASER, LOW);
          ledAmSolido(false);
        }
      }
    }

    // 4. SESSION TIMEOUT
    {
      bool sa;
      if (xSemaphoreTake(mtx, 5) == pdTRUE) { sa = sessaoAtiva; xSemaphoreGive(mtx); }
      if (sa && tUltAtiv > 0 && (agora - tUltAtiv > SESSION_TIMEOUT_MS)) {
        int sid; char sd[96]; int st;
        if (xSemaphoreTake(mtx, 10) == pdTRUE) {
          sid = sessaoId; memcpy(sd, sessaoDesc, 96); st = totalTiros;
          armaOK = false; sessaoAtiva = false;
          xSemaphoreGive(mtx);
        }
        digitalWrite(PIN_LASER, LOW);
        ledAmSolido(false);
        Serial.println(F("  [SESSAO] Timeout 15 min."));
        ledAmBlink(3, 200);
        buzzerPattern(2);
        Evento e{}; e.tipo = EVT_SESSION_TIMEOUT;
        e.idAgente = sid; memcpy(e.descAgente, sd, 96); e.vezes = st;
        e.latitude = gLat; e.longitude = gLng; e.gpsReal = gFix;
        memcpy(e.data, gData, 12); memcpy(e.hora, gHora, 10);
        enfileirar(e);
        tUltAtiv = 0;
        Serial.println(F("  Segure a empunhadura e valide a digital."));
      }
    }

    // 5. BOTAO SOS
    {
      bool sosLido = (digitalRead(PIN_SOS) == LOW);
      if (sosLido && !sosPres) { sosPres = true; tSosPress = agora; sosLongEnv = false; }

      if (sosPres && !sosLongEnv && (agora - tSosPress > SOS_LONGO_MIN_MS)) {
        sosLongEnv = true;
        int sid; char sd[96]; int st;
        if (xSemaphoreTake(mtx, 10) == pdTRUE) {
          sid = sessaoId; memcpy(sd, sessaoDesc, 96);
          st = totalTiros; totalTiros = 0;
          xSemaphoreGive(mtx);
        }
        if (sessaoAtiva) tUltAtiv = agora;

        Evento e{}; e.tipo = EVT_SOS_EMERGENCIA; e.altaPrior = true;
        e.idAgente = sid;
        memcpy(e.descAgente, strlen(sd) > 0 ? sd : "Sem sessao ativa", 96);
        e.vezes = st;
        e.latitude = gLat; e.longitude = gLng; e.gpsReal = gFix;
        memcpy(e.data, gData, 12); memcpy(e.hora, gHora, 10);
        enfileirar(e);

        ledAzReq = LAZ_SOS;
        Serial.println(F("  [SOS LONGO] EMERGENCIA!"));
        buzzerPattern(5);
      }

      if (!sosLido && sosPres) {
        uint32_t dur = agora - tSosPress;
        sosPres = false;
        if (!sosLongEnv && dur > 80 && dur < SOS_CURTO_MAX_MS) {
          if (posseConf) {
            int sid; char sd[96]; int st;
            if (xSemaphoreTake(mtx, 5) == pdTRUE) {
              sid = sessaoId; memcpy(sd, sessaoDesc, 96); st = totalTiros;
              xSemaphoreGive(mtx);
            }
            tUltAtiv = agora;
            Evento e{}; e.tipo = EVT_ENVIO_MANUAL;
            e.idAgente = sid; memcpy(e.descAgente, sd, 96); e.vezes = st;
            e.latitude = gLat; e.longitude = gLng; e.gpsReal = gFix;
            memcpy(e.data, gData, 12); memcpy(e.hora, gHora, 10);
            enfileirar(e);
            ledAzReq = LAZ_ENVIO_MANUAL;
            Serial.println(F("  [SOS CURTO] Envio manual."));
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ============================================================
//  ENFILEIRAR
// ============================================================
void enfileirar(const Evento &e) {
  bool alta = e.altaPrior            ||
              e.tipo == EVT_SOS_EMERGENCIA ||
              e.tipo == EVT_POSSE_PERDIDA  ||
              e.tipo == EVT_ACESSO_NEGADO  ||
              e.tipo == EVT_DISPARO;
  if (xQueueSend(alta ? qAlta : qNormal, &e, 0) != pdTRUE)
    salvarOffline(e);
}

// ============================================================
//  PROCESSAR FILAS
// ============================================================
void processarFilas() {
  Evento e;
  bool alta = (xQueueReceive(qAlta, &e, 0) == pdTRUE);
  if (!alta && xQueueReceive(qNormal, &e, 0) != pdTRUE) return;

  String json = buildJSON(e);
  String sms  = buildSMS(e);

  Serial.println(F("============================================================"));
  Serial.printf( "  EVENTO  : %s\n", nomeEvento(e.tipo).c_str());
  Serial.printf( "  Agente  : %d\n", e.idAgente);
  Serial.printf( "  Tiros   : %d\n", e.vezes);
  Serial.println(F("  JSON:"));
  Serial.println(json);

  ledAzSolido(true);
  bool okH = false;
  for (int i = 0; i < HTTP_RETRY_MAX && !okH; i++) {
    okH = httpPost(e);
    if (!okH) vTaskDelay(pdMS_TO_TICKS(2500));
  }
  ledAzSolido(false);

  if (okH) { ledAzBlink(2, 200); Serial.println(F("  [HTTP] OK 200")); }
  else      { Serial.println(F("  [HTTP] FALHA")); }

  bool okS = smsSend(e);
  if (okS) { ledAzBlink(3, 100); Serial.println(F("  [SMS] OK")); }
  else      { Serial.println(F("  [SMS] FALHA")); }

  if (!okH && !okS) salvarOffline(e);
  Serial.println(F("============================================================"));
}

// ============================================================
//  OFFLINE (NVS Flash)
// ============================================================
void salvarOffline(const Evento &e) {
  uint32_t cnt = prefs.getUInt("evtCnt", 0);
  String   key = String("evt") + cnt;
  String   js  = buildJSON(e);
  if (js.length() < 3800) {
    prefs.putString(key.c_str(), js);
    prefs.putUInt("evtCnt", cnt + 1);
    Serial.printf("  [NVS] Guardado offline: evt%u\n", cnt);
  }
}

// ============================================================
//  TENTAR OFFLINE
// ============================================================
void tentarOffline() {
  uint32_t cnt = prefs.getUInt("evtCnt", 0);
  if (cnt == 0) return;
  String pd = atSend("AT+CGACT?", "+CGACT", 2000);
  if (pd.indexOf(",1") < 0) return;

  String key  = "evt0";
  String json = prefs.getString(key.c_str(), "");
  if (json.length() < 5) {
    prefs.remove(key.c_str()); prefs.putUInt("evtCnt", cnt-1); return;
  }

  int len = json.length();
  atOK("AT+HTTPTERM", 1000);
  if (!atOK("AT+HTTPINIT", 3000)) return;
  atOK("AT+HTTPSSL=1", 1000);
  atOK("AT+HTTPPARA=\"CID\",1", 1000);
  String u = String("AT+HTTPPARA=\"URL\",\"") + SERVIDOR_URL + "\"";
  atOK(u.c_str(), 1000);
  atOK("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);
  String dc = "AT+HTTPDATA=" + String(len) + ",10000";
  if (atSend(dc.c_str(), "DOWNLOAD", 6000).indexOf("DOWNLOAD") < 0) {
    atOK("AT+HTTPTERM", 1000); return;
  }
  gsmSer.print(json);
  vTaskDelay(pdMS_TO_TICKS(1500));
  String r = atSend("AT+HTTPACTION=1", "+HTTPACTION", 20000);
  atOK("AT+HTTPTERM", 1000);

  if (r.indexOf(",200") >= 0 || r.indexOf(",201") >= 0) {
    prefs.remove(key.c_str());
    for (uint32_t i = 1; i < cnt; i++) {
      String k1 = String("evt") + i;
      String k0 = String("evt") + (i-1);
      prefs.putString(k0.c_str(), prefs.getString(k1.c_str(), ""));
      prefs.remove(k1.c_str());
    }
    prefs.putUInt("evtCnt", cnt-1);
    Serial.println(F("  [NVS] Offline reenviado."));
  }
}

// ============================================================
//  HTTP POST
// ============================================================
bool httpPost(const Evento &e) {
  String pd = atSend("AT+CGACT?", "+CGACT", 2000);
  if (pd.indexOf(",1") < 0 && !ativarGPRS()) return false;

  String payload = buildJSON(e);
  int    len     = payload.length();

  atOK("AT+HTTPTERM", 1000);
  vTaskDelay(pdMS_TO_TICKS(200));
  if (!atOK("AT+HTTPINIT", 3000)) return false;
  atOK("AT+HTTPSSL=1", 1000);
  atOK("AT+HTTPPARA=\"CID\",1", 1000);
  String u = String("AT+HTTPPARA=\"URL\",\"") + SERVIDOR_URL + "\"";
  atOK(u.c_str(), 1000);
  atOK("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);
  String dc = "AT+HTTPDATA=" + String(len) + ",10000";
  if (atSend(dc.c_str(), "DOWNLOAD", 6000).indexOf("DOWNLOAD") < 0) {
    atOK("AT+HTTPTERM", 1000); return false;
  }
  gsmSer.print(payload);
  vTaskDelay(pdMS_TO_TICKS(len < 300 ? 1000 : 2000));
  String r = atSend("AT+HTTPACTION=1", "+HTTPACTION", 25000);
  atOK("AT+HTTPTERM", 1000);
  return (r.indexOf(",200") >= 0 || r.indexOf(",201") >= 0);
}

// ============================================================
//  SMS
// ============================================================
bool smsSend(const Evento &e) {
  atOK("AT+CMGF=1", 1000);
  String cmd = String("AT+CMGS=\"") + TEL_SUPERVISOR + "\"";
  if (atSend(cmd.c_str(), ">", 6000).indexOf(">") < 0) return false;
  gsmSer.print(buildSMS(e));
  gsmSer.write(0x1A);
  return atSend("", "+CMGS", 20000).indexOf("+CMGS") >= 0;
}

// ============================================================
//  GPRS
// ============================================================
bool ativarGPRS() {
  String ap = String("AT+CGDCONT=1,\"IP\",\"") + APN + "\"";
  atOK(ap.c_str(), 2000);
  atOK("AT+CGACT=1,1", 10000);
  return atSend("AT+CGACT?", "+CGACT", 3000).indexOf(",1") >= 0;
}

// ============================================================
//  RELOGIO
// ============================================================
void syncRelogio() {
  String r   = atSend("AT+CCLK?", "+CCLK", 3000);
  int    ini = r.indexOf('"'), fim = r.lastIndexOf('"');
  if (ini < 0 || fim <= ini + 15) return;
  String dt  = r.substring(ini + 1, fim);
  if (dt.length() < 17) return;
  int yy = dt.substring(0, 2).toInt();
  if (yy < 25) return;
  char d[12], h[10];
  snprintf(d, 12, "20%02d-%s-%s", yy,
    dt.substring(3, 5).c_str(), dt.substring(6, 8).c_str());
  snprintf(h, 10, "%s:%s:%s",
    dt.substring(9, 11).c_str(),
    dt.substring(12, 14).c_str(),
    dt.substring(15, 17).c_str());
  memcpy(gData, d, 12);
  memcpy(gHora, h, 10);
}

// ============================================================
//  GPS
// ============================================================
void lerGPS() {
  String r   = atSend("AT+CGPSINFO", "+CGPSINFO", 3000);
  int    idx = r.indexOf("+CGPSINFO:");
  if (idx < 0) return;
  String d   = r.substring(idx + 10); d.trim();
  if (d.startsWith(",") || d.length() < 10) return;

  auto nextF = [](String &s) -> String {
    int c = s.indexOf(',');
    if (c < 0) { String f = s; s = ""; return f; }
    String f = s.substring(0, c); s = s.substring(c+1); return f;
  };

  String latS = nextF(d), latD = nextF(d);
  String lngS = nextF(d), lngD = nextF(d);
  if (latS.length() < 4 || lngS.length() < 4) return;

  float lr = latS.toFloat(); int ld = (int)(lr/100);
  float lat = ld + (lr - ld*100) / 60.0f;
  if (latD == "S") lat = -lat;

  float nr = lngS.toFloat(); int nd = (int)(nr/100);
  float lng = nd + (nr - nd*100) / 60.0f;
  if (lngD == "W") lng = -lng;

  if (lat != 0.0f || lng != 0.0f) { gLat = lat; gLng = lng; gFix = true; }
}

// ============================================================
//  BUILD JSON — alinhado com API /api/ocorrencias
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
// ============================================================
String buildJSON(const Evento &e) {
  char occ[16];
  snprintf(occ, sizeof(occ), "OCC-%05u", (unsigned)occID);
  prefs.putUInt("occID", ++occID);

  float lat = e.gpsReal ? e.latitude  : GPS_FALLBACK_LAT;
  float lng = e.gpsReal ? e.longitude : GPS_FALLBACK_LNG;

  StaticJsonDocument<512> doc;
  doc["id"]        = String(occ);
  doc["idAgente"]  = String(e.idAgente);
  doc["latitude"]  = serialized(String(lat, 7));
  doc["longitude"] = serialized(String(lng, 7));
  doc["data"]      = String(e.data);
  doc["hora"]      = String(e.hora);
  doc["vezes"]     = e.vezes;
  doc["status"]    = nomeEvento(e.tipo);

  String out;
  serializeJson(doc, out);   // JSON compacto (menos bytes via GSM)
  return out;
}

// ============================================================
//  BUILD SMS
// ============================================================
String buildSMS(const Evento &e) {
  float lat = e.gpsReal ? e.latitude  : GPS_FALLBACK_LAT;
  float lng = e.gpsReal ? e.longitude : GPS_FALLBACK_LNG;
  String s;
  s  = "[" + nomeEvento(e.tipo) + "]\n";
  s += "Agente:" + String(e.idAgente) + "\n";
  s += "Tiros :" + String(e.vezes)    + "\n";
  s += "Data  :" + String(e.data) + " " + String(e.hora) + "\n";
  s += "GPS   :" + String(lat, 6) + "," + String(lng, 6) + "\n";
  s += "Mapa  :" + linkMapa(lat, lng, e.gpsReal);
  return s;
}

String linkMapa(float lat, float lng, bool real) {
  if (!real) return GPS_FALLBACK_LINK;
  char buf[80];
  snprintf(buf, sizeof(buf), "https://maps.google.com/?q=%.6f,%.6f",
    (double)lat, (double)lng);
  return String(buf);
}

String descID(int id) {
  if (id == ID_PROP_1) return "Proprietario ID1";
  if (id == ID_PROP_2) return "Proprietario ID2";
  if (id  >  0)        return "ID " + String(id) + " nao autorizado";
  return "Digital nao cadastrada";
}

String nomeEvento(TipoEvento t) {
  switch (t) {
    case EVT_DISPARO:         return "DISPARO_ATIVO";
    case EVT_ACESSO_NEGADO:   return "ACESSO_NEGADO";
    case EVT_POSSE_PERDIDA:   return "POSSE_PERDIDA";
    case EVT_SOS_EMERGENCIA:  return "SOS_EMERGENCIA";
    case EVT_ENVIO_MANUAL:    return "ENVIO_MANUAL";
    case EVT_SESSION_TIMEOUT: return "SESSION_TIMEOUT";
    case EVT_PERIODICO:       return "LOCALIZACAO_PERIODICA";
    default:                  return "DESCONHECIDO";
  }
}

// ============================================================
//  GSM POWER ON
// ============================================================
void gsmPowerOn() {
  gsmFlush();
  gsmSer.println("AT");
  vTaskDelay(pdMS_TO_TICKS(600));
  String r = "";
  while (gsmSer.available()) r += (char)gsmSer.read();
  if (r.indexOf("OK") >= 0) return;
  digitalWrite(PIN_GSM_PWRKEY, LOW);
  vTaskDelay(pdMS_TO_TICKS(1200));
  digitalWrite(PIN_GSM_PWRKEY, HIGH);
  vTaskDelay(pdMS_TO_TICKS(3000));
}

// ============================================================
//  AT SEND / AT OK
// ============================================================
String atSend(const char *cmd, const char *wait, uint32_t timeout) {
  gsmFlush();
  if (cmd && strlen(cmd) > 0) gsmSer.println(cmd);
  String   resp = "";
  uint32_t t0   = millis();
  while (millis() - t0 < timeout) {
    while (gsmSer.available()) resp += (char)gsmSer.read();
    if (wait && strlen(wait) > 0 && resp.indexOf(wait) >= 0) break;
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  return resp;
}

bool atOK(const char *cmd, uint32_t timeout) {
  return atSend(cmd, "OK", timeout).indexOf("OK") >= 0;
}

void gsmFlush() { while (gsmSer.available()) gsmSer.read(); }

// ============================================================
//  LEDs & BUZZER
// ============================================================
void ledAmSolido(bool on) { digitalWrite(PIN_LED_AM, on ? HIGH : LOW); }
void ledAmBlink(int n, int ms) {
  for (int i = 0; i < n; i++) {
    digitalWrite(PIN_LED_AM, HIGH); vTaskDelay(pdMS_TO_TICKS(ms));
    digitalWrite(PIN_LED_AM, LOW);  vTaskDelay(pdMS_TO_TICKS(ms));
  }
}
void ledAzSolido(bool on) { digitalWrite(PIN_LED_AZ, on ? HIGH : LOW); }
void ledAzBlink(int n, int ms) {
  for (int i = 0; i < n; i++) {
    digitalWrite(PIN_LED_AZ, HIGH); vTaskDelay(pdMS_TO_TICKS(ms));
    digitalWrite(PIN_LED_AZ, LOW);  vTaskDelay(pdMS_TO_TICKS(ms));
  }
}
void buzzer(int ms) {
  digitalWrite(PIN_BUZZER, HIGH); vTaskDelay(pdMS_TO_TICKS(ms)); digitalWrite(PIN_BUZZER, LOW);
}
void buzzerPattern(int n) {
  for (int i = 0; i < n; i++) { buzzer(80); vTaskDelay(pdMS_TO_TICKS(80)); }
}
