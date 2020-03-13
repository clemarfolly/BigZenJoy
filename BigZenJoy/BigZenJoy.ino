/* Arduino USB Joystick HID demo */

/* Author: Darran Hunt
  Released into the public domain.
  Update by RICLAMER in 25/03/2014 to use Analog ports and digital ports
  This code is to be used with Arduino UNO (6 axis and 12 Button )
  This code is compatible with Arduino Mega.

  Alterações Março/2020
  Modificado por Clemar Folly clemarjr[AT]gmail.com
  Incluido leitura analógica para 6 potenciometros.
  Incluida rotina para amenizar as leituras do ADC e aumentar a estabilidade.

  6 entradas analogicos: A0 ate A5
  12 Entradas digitais: 2 ate 13.

  As entradas analogicas que nao foram usadas deverão ser ligadas ao ground.

  Versão 1.0

  Dedicado ao grupo Zentorius Games
  https://www.facebook.com/groups/171182360124959/
*/

/* INSTALATION
  Just install POT in each analog port. Using the _Grnd _Analog _5V Arduino.
  Like this image: http://arduino.cc/en/uploads/Tutorial/joy_sch_480.jpg
  To setup the buttons, just install you prefered button switch under GND and Port Digital 02~13.
  Use Flip to erease and burn this firmware DFU: https://github.com/harlequin-tech/arduino-usb/blob/master/firmwares/Arduino-big-joystick.hex
  I used Arduino R3 with Atmega 16U2.
*/

//Variável de compilação para depurar os resultados pela janela do Serial Monitor.
#undef DEBUG //Depiração desabilitada.
//#define DEBUG //Depiração habilitada.

#define NUM_BUTTONS  40   //Numero de botões que existem no controle. Não modificar.
#define NUM_AXES  6       //Usa 6 eixos no arduino UNO e 8 no MEGA. Se estiver usando o UNO, não altere esse valor.

//Tipo usado para relatorio HID.
typedef struct joyReport_t
{
  int16_t axis[8];
  uint8_t button[(NUM_BUTTONS + 7) / 8]; // 8 buttons per byte
} joyReport_t;

joyReport_t joyReport;

//Variáveis do atenuador das entradas analogicas.
const uint8_t numReadings = 10; // Tamanho do filtro atenuador das entradas analogicas.
//Quanto maior o valor, mais estável fica a leitura. Quanto menor mais rápida é a resposta.
uint16_t readings[NUM_AXES][numReadings]; // As leituras das entradas analógicas.
uint8_t readIndex[NUM_AXES];              // Indices das leituras atuais.
uint32_t total[NUM_AXES];                  // Soma todal das leituras armazenadas.

/* Assinatura dos métodos usados */
void setup(void);
void loop(void);
void setButton(joyReport_t *joy, uint8_t button);
void clearButton(joyReport_t *joy, uint8_t button);
void sendJoyReport(joyReport_t *report);
uint16_t analogSmoothed(uint8_t axis);

void setup()
{
  //Devine os pinos como entrada e resistores pullup ativados. Usado para os botões
  for ( int portId = 2; portId <= 13; portId ++ )
  {
    pinMode( portId, INPUT_PULLUP);
  }

  //Inicia as variáveis.
  for (uint8_t i = 0; i < NUM_AXES; i++)
  {
    for (uint8_t j = 0; j < numReadings; j++)
    {
      readings[i][j] = 0;
    }
    readIndex[i] = 0;
    total[i] = 0;
  }

  //Inicia a porta serial.
  Serial.begin(115200);
  delay(200);

  //Inicia o relatorio HID
  for (uint8_t ind = 0; ind < 8; ind++)
  {
    joyReport.axis[ind] = ind * 1000;
  }

  for (uint8_t ind = 0; ind < sizeof(joyReport.button); ind++)
  {
    joyReport.button[ind] = 0;
  }
}

// Envia o relatório HID para a interface USB.
void sendJoyReport(struct joyReport_t *report)
{
#ifndef DEBUG
  Serial.write((uint8_t *)report, sizeof(joyReport_t));
#else
  // Lista as variáveis de maneira facil de compreender no monitor serial.
  for (uint8_t ind = 0; ind < NUM_AXES; ind++)
  {
    Serial.print("axis[");
    Serial.print(ind);
    Serial.print("]= ");
    Serial.print(report->axis[ind]);
    Serial.print(" ");
  }
  Serial.println();

  for (uint8_t ind = 0; ind < NUM_BUTTONS / 8; ind++)
  {
    Serial.print("button[");
    Serial.print(ind);
    Serial.print("]= ");
    Serial.print(report->button[ind], BIN);
    Serial.print(" ");
  }

  Serial.println();
#endif
}

// Liga um botão do joystick
void setButton(joyReport_t *joy, uint8_t button)
{
  uint8_t index = button / 8;
  uint8_t bit = button - 8 * index;

  joy->button[index] |= 1 << bit;
}

// Desliga um botão do joystick
void clearButton(joyReport_t *joy, uint8_t button)
{
  uint8_t index = button / 8;
  uint8_t bit = button - 8 * index;

  joy->button[index] &= ~(1 << bit);
}

//Suaviza as leituras analogicas.
uint16_t analogSmoothed(uint8_t axis)
{
  //Remove a leitura anterior.
  total[axis] = total[axis] - readings[axis][readIndex[axis]];

  // Le do potenciometro e converte para uma variável de 16 bits.
  readings[axis][readIndex[axis]] = map(analogRead(axis), 0, 1023, 0, 65535);

  // Adiciona a leitura atual ao total.
  total[axis] = total[axis] + readings[axis][readIndex[axis]];

  // Avança para a proxima posição de leitura.
  readIndex[axis] = readIndex[axis] + 1;

  // Se estiver no final da matriz, volta para o inicio.
  if (readIndex[axis] >= numReadings) {
    readIndex[axis] = 0;
  }

  // Calcula a média.
  return total[axis] / numReadings;
}

void loop()
{
  //le as entradas digitais dos botões.
  for (uint8_t i = 2; i <= 13; i++)
  {
    int btn = digitalRead(i);
    if (btn == LOW)
    {
      setButton(&joyReport, i - 2);
    }
    else
    {
      clearButton(&joyReport, i - 2);
    }
  }

  /* leitura das entradas analógicas. */
  for (uint8_t axis = 0; axis < NUM_AXES; axis++)
  {
    joyReport.axis[axis] = map(analogSmoothed(axis), 0, 65535, -32768, 32767);
  }

  //Se forem 6 eixos então zera os eixos 7 e 8
#if (NUM_AXES == 6)
  joyReport.axis[6] = 0;
  joyReport.axis[7] = 0;
#endif

  //Envia os dados para o HID
  sendJoyReport(&joyReport);

  delay(35);
}

