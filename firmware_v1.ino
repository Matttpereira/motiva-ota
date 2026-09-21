// ============================================================
// PROJETO MOTIVA - Firmware 1.0
// Disciplina: S2-CP02
// Integrantes:
//   Lucas Ferrari Lima           - RM 563119
//   Carlos Eduardo P. Cervelli   - RM 563462
//   Felipe K. dos Santos Menezes - RM 564878
//   Leonardo Lopes Oliveira      - RM 565437
//   Arthur de Souza Matos Dias   - RM 566068
//   Guilherme Carreri Giampietro - RM 565676
//   Mateus Patrício Pereira      - RM 564695
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

// --- Configurações de rede ---
const char* SSID     = "Wokwi-GUEST";
const char* PASSWORD = "";

// --- URL do manifesto de versão no repositório remoto ---
const char* MANIFEST_URL = "https://raw.githubusercontent.com/Matttpereira/motiva-ota/main/version.json";

// --- Versão atual deste firmware ---
const String VERSAO_ATUAL = "1.0";

// --- Pinos do LED RGB (ajuste conforme o diagram.json do Wokwi) ---
const int LED_R = 16;
const int LED_G = 17;
const int LED_B = 18;

// --- Constantes de temporização ---
const unsigned long INTERVALO_LEITURA = 2000;   // 2 segundos entre leituras
const unsigned long INTERVALO_SESSAO  = 48000;  // 48 segundos entre sessões
const int TOTAL_LEITURAS              = 5;

// --- Ciclos antes de verificar OTA ---
const int CICLOS_ANTES_OTA = 3;

// --- Variáveis de controle de tempo ---
unsigned long tInicioSessao  = 0;
unsigned long tUltimaLeitura = 0;

// --- Variáveis de estado ---
int    leituras[TOTAL_LEITURAS];
int    contLeituras   = 0;
int    contSessoes    = 0;
bool   sessaoAtiva    = true;
bool   otaVerificada  = false;

// ============================================================
// FUNÇÕES AUXILIARES
// ============================================================

// Acende o LED na cor desejada (0-255 por canal)
void setLED(int r, int g, int b) {
  digitalWrite(LED_R, r > 0 ? HIGH : LOW);
  digitalWrite(LED_G, g > 0 ? HIGH : LOW);
  digitalWrite(LED_B, b > 0 ? HIGH : LOW);
}

// LED azul = Firmware 1.0 em execução
void indicarFirmware1() {
  setLED(0, 0, 255);
}

// Gera leitura pseudoaleatória entre 10 e 20 cm
int gerarLeitura() {
  return random(10, 21); // random(min, max) exclui max, então usamos 21
}

// Calcula média aritmética do vetor de leituras
float calcularMedia() {
  float soma = 0;
  for (int i = 0; i < TOTAL_LEITURAS; i++) {
    soma += leituras[i];
  }
  return soma / TOTAL_LEITURAS;
}

// Exibe cabeçalho da sessão no Serial Monitor
void exibirCabecalho() {
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 1.0");
  Serial.println("========================================");
}

// Exibe resultado final da sessão
void exibirResultadoSessao() {
  float media = calcularMedia();
  Serial.print("Media da sessao: ");
  Serial.print(media, 1);
  Serial.println(" cm");
  Serial.println("Proxima sessao em 48 segundos.");
  Serial.println();
}

// ============================================================
// FUNÇÕES DE OTA
// ============================================================

// Conecta ao Wi-Fi
bool conectarWiFi() {
  Serial.print("[OTA] Conectando ao Wi-Fi");
  WiFi.begin(SSID, PASSWORD);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[OTA] Wi-Fi conectado: " + WiFi.localIP().toString());
    return true;
  }
  Serial.println("\n[OTA] ERRO: Falha ao conectar ao Wi-Fi.");
  return false;
}

// Lê e interpreta o manifesto version.json
// Retorna true se versão disponível for mais nova
bool verificarVersaoDisponivel(String &urlFirmware) {
  HTTPClient http;
  Serial.println("[OTA] Consultando manifesto: " + String(MANIFEST_URL));

  http.begin(MANIFEST_URL);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.println("[OTA] ERRO: Nao foi possivel acessar o manifesto. HTTP " + String(httpCode));
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // Parse do JSON
  StaticJsonDocument<256> doc;
  DeserializationError erro = deserializeJson(doc, payload);
  if (erro) {
    Serial.println("[OTA] ERRO: Falha ao interpretar o JSON do manifesto.");
    return false;
  }

  String versaoDisponivel = doc["version"].as<String>();
  urlFirmware             = doc["url"].as<String>();

  Serial.println("[OTA] Versao instalada : " + VERSAO_ATUAL);
  Serial.println("[OTA] Versao disponivel: " + versaoDisponivel);

  // Comparação simples: se não forem iguais, considera mais nova
  if (versaoDisponivel != VERSAO_ATUAL) {
    Serial.println("[OTA] Nova versao detectada! Iniciando atualizacao...");
    return true;
  }

  Serial.println("[OTA] Firmware ja esta atualizado. Nenhuma acao necessaria.");
  return false;
}

// Faz o download e grava o firmware via OTA
void realizarOTA(const String &urlFirmware) {
  HTTPClient http;
  Serial.println("[OTA] Baixando firmware: " + urlFirmware);

  http.begin(urlFirmware);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.println("[OTA] ERRO: Nao foi possivel baixar o firmware. HTTP " + String(httpCode));
    http.end();
    return;
  }

  int tamanho = http.getSize();
  WiFiClient* stream = http.getStreamPtr();

  Serial.println("[OTA] Tamanho do firmware: " + String(tamanho) + " bytes");
  Serial.println("[OTA] Gravando novo firmware...");

  if (!Update.begin(tamanho)) {
    Serial.println("[OTA] ERRO: Espaco insuficiente para OTA.");
    http.end();
    return;
  }

  size_t escritos = Update.writeStream(*stream);
  http.end();

  if (escritos != (size_t)tamanho) {
    Serial.println("[OTA] ERRO: Download incompleto. Bytes esperados: " +
                   String(tamanho) + " | Recebidos: " + String(escritos));
    return;
  }

  if (!Update.end()) {
    Serial.println("[OTA] ERRO: Falha ao finalizar gravacao OTA. Codigo: " +
                   String(Update.getError()));
    return;
  }

  Serial.println("[OTA] Firmware gravado com sucesso!");
  Serial.println("[OTA] Reiniciando ESP32...");
  delay(1000);
  ESP.restart();
}

// Orquestra todo o processo de verificação e atualização OTA
void verificarOTA() {
  Serial.println();
  Serial.println("======================================");
  Serial.println("[OTA] Iniciando verificacao de atualizacao...");
  Serial.println("======================================");

  if (!conectarWiFi()) return;

  String urlFirmware = "";
  if (verificarVersaoDisponivel(urlFirmware)) {
    realizarOTA(urlFirmware);
  }

  // Desconecta Wi-Fi para economizar recursos
  WiFi.disconnect(true);
  Serial.println("[OTA] Wi-Fi desconectado. Retomando operacao normal.");
  Serial.println();
}

// ============================================================
// SETUP E LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  // Configura pinos do LED
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // Inicializa LED azul (FW 1.0)
  indicarFirmware1();

  // Inicializa gerador pseudoaleatório com valor do ADC flutuante
  randomSeed(analogRead(0));

  // Inicializa temporizadores
  tInicioSessao  = millis();
  tUltimaLeitura = millis();

  exibirCabecalho();
}

void loop() {
  unsigned long agora = millis();

  // --- Fase de leitura: coleta 5 leituras com intervalo de 2s ---
  if (sessaoAtiva && contLeituras < TOTAL_LEITURAS) {
    if (agora - tUltimaLeitura >= INTERVALO_LEITURA) {
      tUltimaLeitura = agora;
      leituras[contLeituras] = gerarLeitura();
      Serial.print("Leitura ");
      Serial.print(contLeituras + 1);
      Serial.print(": ");
      Serial.print(leituras[contLeituras]);
      Serial.println(" cm");
      contLeituras++;
    }
  }

  // --- Fim de sessão: exibe resultados quando as 5 leituras forem coletadas ---
  if (sessaoAtiva && contLeituras == TOTAL_LEITURAS) {
    exibirResultadoSessao();
    contSessoes++;
    sessaoAtiva = false;

    // Verifica OTA após CICLOS_ANTES_OTA sessões completas
    if (contSessoes >= CICLOS_ANTES_OTA && !otaVerificada) {
      otaVerificada = true;
      verificarOTA();
    }
  }

  // --- Inicia nova sessão exatamente 48s após o início da anterior ---
  if (!sessaoAtiva && (agora - tInicioSessao >= INTERVALO_SESSAO)) {
    tInicioSessao  = agora;
    tUltimaLeitura = agora;
    contLeituras   = 0;
    sessaoAtiva    = true;
    exibirCabecalho();
  }
}
