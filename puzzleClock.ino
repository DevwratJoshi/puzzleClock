int sleepTime = 1000; // Sleep time (s) between checking if the pinmode has changed
bool _setCheckPin = false;
int interruptPin = 2;
int inputPins[2] = {6, 5};
int inputPinCount = -1;
int loopCheckTime = 20; // The main loop will sleep for this much time between queries. Need to keep each button pressed for at least this much time
void setup() {
  // put your setup code here, to run once:
  pinMode(2, INPUT);
  // Assumes at least one input pin specified
  inputPinCount = sizeof(inputPins) / sizeof(inputPins[0]);
  for(int i = 0; i < inputPinCount; i++) {
    pinMode(inputPins[i], INPUT);
  }
  Serial.begin(115200);
  attachInterrupt(digitalPinToInterrupt(interruptPin),SetCheckPin,RISING);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(_setCheckPin) {
    noInterrupts();
    int pressedButton = CheckPressedButton();
    Serial.print("Pressed  ");
    Serial.println(pressedButton);
    interrupts();
  }
  delay(loopCheckTime);
}

int CheckPressedButton()
{
  int retPin = -1;
  for(int i = 0; i < inputPinCount; i++) {
    if(digitalRead(inputPins[i])) {
      retPin = inputPins[i];
        break;
    }
  }
  _setCheckPin = false;
  return retPin;
}

void SetCheckPin()
{
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 100)
  {
    _setCheckPin = true;
  }
  last_interrupt_time = interrupt_time;
}
