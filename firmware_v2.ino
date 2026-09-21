// ============================================================
// PROJETO MOTIVA - Firmware 2.0
// Disciplina: S2-CP02
// Integrantes:
//   Lucas Ferrari Lima           - RM 563119
//   Carlos Eduardo P. Cervelli   - RM 563462
//   Felipe K. dos Santos Menezes - RM 564878
//   Leonardo Lopes Oliveira      - RM 565437
//   Arthur de Souza Matos Dias   - RM 566068
//   Guilherme Carreri Giampietro - RM 565676
//   Mateus Patrício Pereira      - RM 564695
//
// NOVIDADES em relação ao Firmware 1.0:
//   - Ordenação das leituras (bubble sort)
//   - Cálculo e exibição da mediana
//   - Lógica de histerese baseada na mediana
//   - LED RGB indica estado: Verde (NORMAL) ou Vermelho (ALERTA)
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

// --- Configurações de rede ---
const char* SSID     = "Wokwi-GUEST";
const char* PASSWORD = "";

// --- Versão atual deste firmware ---
const String VERSAO_ATUAL = "2.0";

// --- Pinos do LED RGB ---
const int LED_R = 16;
const int LED_G = 17;
const int LED_B = 18;

// --- Constantes de temporização ---
const unsigned long INTERVALO_LEITURA = 2000;   // 2s entre leituras
const unsigned long INTERVALO_SESSAO  = 48000;  // 48s entre sessões
const int TOTAL_LEITURAS              = 5;

// --- Limiares de histerese ---
const float LIMIAR_ALERTA = 16.0;
const float LIMIAR_NORMAL = 14.0;

// --- Estados do sistema ---
typedef enum {
  ESTADO_NORMAL,
  ESTADO_ALERTA
} EstadoSistema;

// --- Variáveis de controle de tempo ---
unsigned long tInicioSessao  = 0;
unsigned long tUltimaLeitura = 0;

// --- Variáveis de estado ---
int          leituras[TOTAL_LEITURAS];
int          leiturasOrdenadas[TOTAL_LEITURAS];
int          contLeituras = 0;
bool         sessaoAtiva  = true;
EstadoSistema estadoAtual = ESTADO_NORMAL; // estado inicial

// ============================================================
// FUNÇÕES DE LED
// ============================================================

void setLED(bool r, bool g, bool b) {
  digitalWrite(LED_R, r ? HIGH : LOW);
  digitalWrite(LED_G, g ? HIGH : LOW);
  digitalWrite(LED_B, b ? HIGH : LOW);
}

// Verde = NORMAL
void ledNormal() {
  setLED(false, true, false);
}

// Vermelho = ALERTA
void ledAlerta() {
  setLED(true, false, false);
}

// Atualiza LED conforme o estado atual
void atualizarLED() {
  if (estadoAtual == ESTADO_NORMAL) {
    ledNormal();
  } else {
    ledAlerta();
  }
}

// ============================================================
// FUNÇÕES DE LEITURA E CÁLCULO
// ============================================================

// Gera leitura pseudoaleatória entre 10 e 20 cm
int gerarLeitura() {
  return random(10, 21);
}

// Calcula média aritmética
float calcularMedia() {
  float soma = 0;
  for (int i = 0; i < TOTAL_LEITURAS; i++) {
    soma += leituras[i];
  }
  return soma / TOTAL_LEITURAS;
}

// Copia leituras para vetor ordenado e aplica bubble sort
void ordenarLeituras() {
  // Copia o vetor original
  for (int i = 0; i < TOTAL_LEITURAS; i++) {
    leiturasOrdenadas[i] = leituras[i];
  }

  // Bubble sort crescente
  for (int i = 0; i < TOTAL_LEITURAS - 1; i++) {
    for (int j = 0; j < TOTAL_LEITURAS - 1 - i; j++) {
      if (leiturasOrdenadas[j] > leiturasOrdenadas[j + 1]) {
        int temp               = leiturasOrdenadas[j];
        leiturasOrdenadas[j]   = leiturasOrdenadas[j + 1];
        leiturasOrdenadas[j+1] = temp;
      }
    }
  }
}

// Retorna mediana (3º elemento do vetor ordenado, índice 2)
int calcularMediana() {
  return leiturasOrdenadas[2];
}

// ============================================================
// LÓGICA DE HISTERESE
// ============================================================
//
// Mediana >= 16 cm          -> entra em ALERTA
// 14 cm < Mediana < 16 cm   -> mantém estado anterior
// Mediana <= 14 cm          -> entra em NORMAL
//
void aplicarHisterese(float mediana) {
  if (mediana >= LIMIAR_ALERTA) {
    estadoAtual = ESTADO_ALERTA;
  } else if (mediana <= LIMIAR_NORMAL) {
    estadoAtual = ESTADO_NORMAL;
  }
  // Se estiver na zona intermediária, não altera o estado atual
}

// ============================================================
// FUNÇÕES DE EXIBIÇÃO
// ============================================================

void exibirCabecalho() {
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE VEGETACAO - FW 2.0");
  Serial.println("========================================");
}

void exibirResultadoSessao() {
  // --- Valores na ordem original ---
  Serial.print("Ordem original  : ");
  for (int i = 0; i < TOTAL_LEITURAS; i++) {
    Serial.print(leituras[i]);
    if (i < TOTAL_LEITURAS - 1) Serial.print(" ");
  }
  Serial.println(" cm");

  // --- Valores na ordem crescente ---
  Serial.print("Ordem crescente : ");
  for (int i = 0; i < TOTAL_LEITURAS; i++) {
    Serial.print(leiturasOrdenadas[i]);
    if (i < TOTAL_LEITURAS - 1) Serial.print(" ");
  }
  Serial.println(" cm");

  // --- Média ---
  float media = calcularMedia();
  Serial.print("Media da sessao : ");
  Serial.print(media, 1);
  Serial.println(" cm");

  // --- Mediana ---
  int mediana = calcularMediana();
  Serial.print("Mediana         : ");
  Serial.print(mediana);
  Serial.println(" cm");

  // --- Estado do sistema após histerese ---
  Serial.print("Estado do sistema: ");
  if (estadoAtual == ESTADO_NORMAL) {
    Serial.println("NORMAL  (LED Verde)");
  } else {
    Serial.println("ALERTA  (LED Vermelho)");
  }

  Serial.println("Proxima sessao em 48 segundos.");
  Serial.println();
}

// ============================================================
// SETUP E LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  // Configura pinos do LED RGB
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // Estado inicial: NORMAL -> LED Verde
  estadoAtual = ESTADO_NORMAL;
  atualizarLED();

  // Inicializa gerador pseudoaleatório
  randomSeed(analogRead(0));

  // Inicializa temporizadores
  tInicioSessao  = millis();
  tUltimaLeitura = millis();

  exibirCabecalho();
}

void loop() {
  unsigned long agora = millis();

  // --- Coleta de leituras: 1 a cada 2s ---
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

  // --- Fim de sessão: processa e exibe quando 5 leituras forem coletadas ---
  if (sessaoAtiva && contLeituras == TOTAL_LEITURAS) {
    // 1. Ordena as leituras
    ordenarLeituras();

    // 2. Calcula mediana e aplica histerese
    int mediana = calcularMediana();
    aplicarHisterese((float)mediana);

    // 3. Atualiza LED conforme novo estado
    atualizarLED();

    // 4. Exibe resultados
    exibirResultadoSessao();

    sessaoAtiva = false;
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
